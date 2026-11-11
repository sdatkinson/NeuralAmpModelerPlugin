#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "ResamplingNAM.h"
#include "NAM/get_dsp.h"
#include "../AudioDSPTools/dsp/ImpulseResponse.h"

namespace
{
void require(bool value, const char* message)
{
  if (!value)
    throw std::runtime_error(message);
}

struct Fixture
{
  std::filesystem::path directory =
    std::filesystem::temp_directory_path()
    / ("nam-cab-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixture() { std::filesystem::create_directories(directory); }
  ~Fixture() { std::filesystem::remove_all(directory); }

  std::filesystem::path wav(const std::string& name, const std::vector<float>& samples, int rate = 48000,
                            int channels = 1)
  {
    const auto path = directory / std::filesystem::u8path(name);
    std::ofstream file(path, std::ios::binary);
    auto integer = [&](uint32_t value, int bytes) {
      for (int i = 0; i < bytes; ++i)
        file.put(static_cast<char>((value >> (8 * i)) & 0xff));
    };
    file.write("RIFF", 4);
    integer(36 + 4 * samples.size(), 4);
    file.write("WAVEfmt ", 8);
    integer(16, 4);
    integer(3, 2);
    integer(channels, 2);
    integer(rate, 4);
    integer(rate * channels * 4, 4);
    integer(channels * 4, 2);
    integer(32, 2);
    file.write("data", 4);
    integer(4 * samples.size(), 4);
    for (float sample : samples)
    {
      uint32_t bits;
      std::memcpy(&bits, &sample, 4);
      integer(bits, 4);
    }
    return path;
  }

  std::filesystem::path nam(const std::string& name, const nlohmann::json& config)
  {
    const auto path = directory / name;
    std::ofstream(path) << config;
    return path;
  }
};

nlohmann::json linear(double rate = 48000.0, int inputs = 1, int outputs = 1)
{
  return {{"version", "0.7.0"},
          {"architecture", "Linear"},
          {"sample_rate", rate},
          {"config", {{"receptive_field", 1}, {"bias", false}, {"in_channels", inputs}, {"out_channels", outputs}}},
          {"weights", std::vector<float>(std::max(inputs, outputs), 0.5f)}};
}

template <typename Process>
std::vector<NAM_SAMPLE> render(Process process, int samples = 4096, bool impulse = false)
{
  std::vector<NAM_SAMPLE> input(samples), output(samples);
  for (int i = 0; i < samples; ++i)
    input[i] = impulse ? (i == 0 ? 1.0 : 0.0) : 0.2 * std::sin(0.03 * i);
  for (int offset = 0; offset < samples;)
  {
    const int count = std::min(137, samples - offset);
    NAM_SAMPLE* in[]{input.data() + offset};
    NAM_SAMPLE* out[]{output.data() + offset};
    process(in, out, count);
    offset += count;
  }
  return output;
}

void test_wav_level_and_rate_changes(Fixture& files)
{
  for (int sourceRate : {44100, 48000, 96000})
  {
    const auto file = files.wav("cab-\xf0\x9f\x8e\xb8.WAV", {0.5f, -0.25f, 0.125f, 0.0f}, sourceRate);
    ResamplingNAM cab(nam::get_dsp(file), 48000.0);
    // Reset must use the loaded data, even after the original file disappears.
    std::filesystem::remove(file);
    for (double hostRate : {44100.0, 96000.0, 48000.0, 44100.0})
    {
      dsp::ImpulseResponse::IRData oldData{{0.5f, -0.25f, 0.125f, 0.0f}, static_cast<double>(sourceRate)};
      dsp::ImpulseResponse legacy(oldData, hostRate);
      cab.Reset(hostRate, 256);
      require(cab.GetLatency() == 0, "WAV cab added resampler latency");
      const auto actual = render([&](auto** in, auto** out, int frames) { cab.process(in, out, frames); });
      const auto oldOutput = render([&](auto** in, auto** out, int frames) {
        const auto processed = legacy.Process(in, 1, frames);
        std::copy_n(processed[0], frames, out[0]);
      });
      // The output-stage adjustment is independent of the host rate; Core
      // already compensates for the changed tap density when resampling.
      const double gainDB = -18.0 + 20.0 * std::log10(48000.0 / cab.GetEncapsulatedSampleRate());
      const double gain = std::pow(10.0, gainDB / 20.0);
      for (size_t i = 0; i < actual.size(); ++i)
        require(std::isfinite(actual[i]) && std::abs(actual[i] * gain - oldOutput[i]) < 1.0e-6,
                "WAV playback differs from the previous processor");
    }
  }
}

void test_long_ir(Fixture& files)
{
  std::vector<float> weights(144001, 0.0f);
  weights.back() = 1.0f;
  const auto path = files.wav("long.wav", weights);
  ResamplingNAM cab(nam::get_dsp(path), 48000.0);
  cab.Reset(48000.0, 256);
  const auto output = render([&](auto** in, auto** out, int frames) { cab.process(in, out, frames); }, 144256, true);
  require(std::abs(output[144000] - 1.0) < 1.0e-6, "Long IR was truncated");
  require(cab.GetPrewarmSamples() == 144001, "Long IR tail was not reported");
  cab.Reset(96000.0, 256);
  require(cab.GetPrewarmSamples() == 288002, "IR tail did not adapt to the host rate");
}

void test_nam_architectures(Fixture& files)
{
  const auto child = linear();
  nlohmann::json sequential{{"version", "0.7.0"},
                            {"architecture", "Sequential"},
                            {"sample_rate", 48000.0},
                            {"config", {{"models", {child, child}}}},
                            {"weights", nlohmann::json::array()}};
  const auto path = files.nam("cab.nam", sequential);
  ResamplingNAM cab(nam::get_dsp(path), 48000.0);
  cab.Reset(48000.0, 256);
  const auto result = render([&](auto** in, auto** out, int frames) { cab.process(in, out, frames); });
  for (size_t i = 0; i < result.size(); ++i)
    require(std::abs(result[i] - 0.05 * std::sin(0.03 * i)) < 1.0e-6, "NAM cab's stored level changed");
  cab.Reset(44100.0, 256);
  require(cab.GetLatency() > 0, "Fixed-rate NAM cab did not report its resampling latency");
  require(cab.GetPrewarmSamples() == 2, "Sequential tail was not converted to host-rate samples");
  const auto output = render([&](auto** in, auto** out, int frames) { cab.process(in, out, frames); });
  for (auto sample : output)
    require(std::isfinite(sample), "Resampled NAM cab produced non-finite audio");

  const auto lstm = std::filesystem::path(NAM_TEST_MODELS_DIR) / "lstm.nam";
  ResamplingNAM nonlinear(nam::get_dsp(lstm), 48000.0);
  nonlinear.Reset(48000.0, 256);
  require(nonlinear.GetPrewarmSamples() == 24000, "LSTM tail estimate was lost");
  auto reference = nam::get_dsp(lstm);
  reference->Reset(48000.0, 256);
  const auto actual = render([&](auto** in, auto** out, int frames) { nonlinear.process(in, out, frames); });
  const auto expected = render([&](auto** in, auto** out, int frames) { reference->process(in, out, frames); });
  for (size_t i = 0; i < actual.size(); ++i)
    require(std::abs(actual[i] - expected[i]) < 1.0e-6, "Nonlinear NAM cab's stored level changed");
  for (const double rate : {44100.0, 96000.0, 48000.0})
  {
    nonlinear.Reset(rate, 256);
    require(nonlinear.GetPrewarmSamples() == static_cast<int>(rate / 2), "LSTM tail did not track host rate");
  }
}

} // namespace

int main()
{
  try
  {
    Fixture files;
    test_wav_level_and_rate_changes(files);
    test_long_ir(files);
    test_nam_architectures(files);
    std::cout << "Cab slot compatibility tests passed\n";
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
