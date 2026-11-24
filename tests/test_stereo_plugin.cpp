// Integration coverage against a built VST3, including its saved-state loading path.
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/common/memorystream.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace
{
using Audio = std::vector<std::vector<double>>;
void require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

struct Fixtures
{
  std::filesystem::path directory =
    std::filesystem::temp_directory_path()
    / ("nam-stereo-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Fixtures() { std::filesystem::create_directories(directory); }
  ~Fixtures() { std::filesystem::remove_all(directory); }

  std::string nam(const char* name, const Audio& kernels, double rate, bool fixedRate = false)
  {
    std::vector<double> weights;
    for (const auto& kernel : kernels)
      weights.insert(weights.end(), kernel.begin(), kernel.end());
    nlohmann::json model{
      {"version", "0.7.0"},
      {"architecture", "Linear"},
      {"sample_rate", rate},
      {"config",
       {{"in_channels", 1}, {"out_channels", kernels.size()}, {"receptive_field", kernels[0].size()}, {"bias", false}}},
      {"weights", weights}};
    if (fixedRate)
      model = {{"version", "0.7.0"},
               {"architecture", "Sequential"},
               {"sample_rate", rate},
               {"config", {{"models", nlohmann::json::array({model})}}},
               {"weights", nlohmann::json::array()}};
    const auto path = directory / name;
    std::ofstream(path) << model;
    return path.string();
  }

  std::string wav(const char* name, const Audio& kernels, int rate)
  {
    const auto path = directory / name;
    std::ofstream file(path, std::ios::binary);
    auto integer = [&](uint32_t n, int bytes) {
      for (int i = 0; i < bytes; ++i)
        file.put(static_cast<char>((n >> (8 * i)) & 255));
    };
    const auto bytes = 4 * kernels.size() * kernels[0].size();
    file.write("RIFF", 4);
    integer(36 + bytes, 4);
    file.write("WAVEfmt ", 8);
    integer(16, 4);
    integer(3, 2);
    integer(kernels.size(), 2);
    integer(rate, 4);
    integer(rate * kernels.size() * 4, 4);
    integer(kernels.size() * 4, 2);
    integer(32, 2);
    file.write("data", 4);
    integer(bytes, 4);
    for (size_t i = 0; i < kernels[0].size(); ++i)
      for (const auto& kernel : kernels)
      {
        const float sample = kernel[i];
        uint32_t bits;
        std::memcpy(&bits, &sample, 4);
        integer(bits, 4);
      }
    return path.string();
  }
};

class HostedPlugin
{
public:
  HostedPlugin(const VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& info, double rate,
               int outputs, int inputs = 1, int maxBlockSize = 256)
  : mOutputs(outputs)
  , mInputs(inputs)
  {
    mProvider = owned(new PlugProvider(factory, info, true));
    require(mProvider->initialize(), "Plugin initialization failed");
    mComponent = mProvider->getComponentPtr();
    mProcessor = mComponent;
    SpeakerArrangement in = inputs == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo;
    SpeakerArrangement out = outputs == 1 ? SpeakerArr::kMono : SpeakerArr::kStereo;
    require(mProcessor->setBusArrangements(&in, 1, &out, 1) == kResultOk, "Bus arrangement rejected");
    ProcessSetup setup{kRealtime, kSample64, maxBlockSize, rate};
    require(mProcessor->setupProcessing(setup) == kResultOk, "Processing setup failed");
    mComponent->activateBus(kAudio, kInput, 0, true);
    mComponent->activateBus(kAudio, kOutput, 0, true);
    mComponent->setActive(true);
    mProcessor->setProcessing(true);
  }
  ~HostedPlugin()
  {
    mProcessor->setProcessing(false);
    mComponent->setActive(false);
  }

  void load(const std::string& model, const std::string& cab, bool cabEnabled = true, bool gateAndEQ = false)
  {
    auto state = makeState(model, cab, cabEnabled, gateAndEQ);
    loadState(state);
  }

  std::vector<char> makeState(const std::string& model, const std::string& cab, bool cabEnabled = true,
                              bool gateAndEQ = false)
  {
    MemoryStream saved;
    require(mComponent->getState(&saved) == kResultOk, "Could not read plugin state");
    auto* data = static_cast<char*>(saved.getData());
    size_t pos = 0;
    auto readString = [&]() {
      require(pos + 4 <= size_t(saved.getSize()), "Invalid saved-state header");
      int n;
      std::memcpy(&n, data + pos, 4);
      pos += 4;
      require(n >= 0 && pos + n <= size_t(saved.getSize()), "Invalid saved-state string");
      std::string s(data + pos, n);
      pos += n;
      return s;
    };
    const auto header = readString(), version = readString();
    readString();
    readString();
    std::vector<char> state;
    auto putString = [&](const std::string& s) {
      const int n = s.size();
      const auto* p = reinterpret_cast<const char*>(&n);
      state.insert(state.end(), p, p + 4);
      state.insert(state.end(), s.begin(), s.end());
    };
    putString(header);
    putString(version);
    putString(model);
    putString(cab);
    std::vector<char> params(data + pos, data + saved.getSize());
    // Saved parameter order is part of the plugin's session compatibility contract.
    auto param = [&](int index, double value) {
      require((index + 1) * sizeof(double) <= params.size(), "Missing saved parameter");
      std::memcpy(params.data() + index * sizeof(double), &value, sizeof(double));
    };
    param(0, 0);
    param(5, 0);
    param(6, gateAndEQ);
    param(7, gateAndEQ);
    param(8, cabEnabled);
    param(11, 0); // cab toggle and raw output mode
    param(2, 7);
    param(3, 3);
    param(4, 8);
    state.insert(state.end(), params.begin(), params.end());
    return state;
  }

  void loadState(std::vector<char>& state)
  {
    MemoryStream stream(state.data(), state.size());
    require(mComponent->setState(&stream) == kResultOk, "Could not restore plugin state");
  }

  Audio render(const Audio& input, std::initializer_list<int> blockSizes = {1, 7, 127, 256, 31})
  {
    Audio output(mOutputs, std::vector<double>(input[0].size(), 123.0));
    for (size_t offset = 0, block = 0; offset < input[0].size(); ++block)
    {
      const int count = std::min<size_t>(blockSizes.begin()[block % blockSizes.size()], input[0].size() - offset);
      std::array<double*, 2> in{}, out{};
      for (int ch = 0; ch < mInputs; ++ch)
        in[ch] = const_cast<double*>(input[ch].data() + offset);
      for (int ch = 0; ch < mOutputs; ++ch)
        out[ch] = output[ch].data() + offset;
      AudioBusBuffers ib{}, ob{};
      ib.numChannels = mInputs;
      ob.numChannels = mOutputs;
      ib.channelBuffers64 = in.data();
      ob.channelBuffers64 = out.data();
      ProcessData process{};
      process.processMode = kRealtime;
      process.symbolicSampleSize = kSample64;
      process.numSamples = count;
      process.numInputs = process.numOutputs = 1;
      process.inputs = &ib;
      process.outputs = &ob;
      require(mProcessor->process(process) == kResultOk, "Plugin processing failed");
      offset += count;
    }
    return output;
  }

  uint32 tail() const { return mProcessor->getTailSamples(); }
  uint32 latency() const { return mProcessor->getLatencySamples(); }

private:
  IPtr<PlugProvider> mProvider;
  IPtr<IComponent> mComponent;
  FUnknownPtr<IAudioProcessor> mProcessor;
  int mOutputs, mInputs;
};

std::vector<double> convolve(const std::vector<double>& input, const std::vector<double>& kernel)
{
  std::vector<double> output(input.size(), 0.0);
  for (size_t i = 0; i < input.size(); ++i)
    for (size_t k = 0; k < kernel.size() && k <= i; ++k)
      output[i] += input[i - k] * kernel[k];
  return output;
}

Audio expected_audio(const std::vector<double>& input, const Audio& amp, const Audio& cab, int outputs, double rate,
                     double gain)
{
  Audio signal;
  if (amp.empty())
    signal = {input};
  else
    for (const auto& kernel : amp)
      signal.push_back(convolve(input, kernel));
  if (!cab.empty())
  {
    Audio filtered;
    if (cab.size() == 2)
    {
      auto mono = signal[0];
      if (signal.size() == 2)
        for (size_t i = 0; i < mono.size(); ++i)
          mono[i] = 0.5 * (signal[0][i] + signal[1][i]);
      for (const auto& kernel : cab)
        filtered.push_back(convolve(mono, kernel));
    }
    else
      for (const auto& channel : signal)
        filtered.push_back(convolve(channel, cab[0]));
    signal = std::move(filtered);
  }
  const double alpha = 1.0 / (1.0 + 2.0 * std::acos(-1.0) * 5.0 / rate);
  for (auto& channel : signal)
  {
    double previousInput = 0.0, previousOutput = 0.0;
    for (auto& value : channel)
    {
      const double next = alpha * (previousOutput + value - previousInput);
      previousInput = value;
      previousOutput = next;
      value = gain * next;
    }
  }
  if (outputs == 1 && signal.size() == 2)
  {
    for (size_t i = 0; i < signal[0].size(); ++i)
      signal[0][i] = 0.5 * (signal[0][i] + signal[1][i]);
    signal.resize(1);
  }
  if (outputs == 2 && signal.size() == 1)
    signal.push_back(signal[0]);
  return signal;
}

void compare(const Audio& actual, const Audio& expected)
{
  require(actual.size() == expected.size(), "Wrong output channel count");
  for (size_t ch = 0; ch < actual.size(); ++ch)
    for (size_t i = 0; i < actual[ch].size(); ++i)
      if (!std::isfinite(actual[ch][i]) || std::abs(actual[ch][i] - expected[ch][i]) > 2.0e-6)
      {
        std::cerr << "Mismatch: channel " << ch << ", sample " << i << ": " << actual[ch][i] << " expected "
                  << expected[ch][i] << '\n';
        throw std::runtime_error("Stereo routing changed the expected audio");
      }
}

void test_routing(const VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& info)
{
  Fixtures files;
  const Audio monoAmp{{0.5, 0.0, 0.125, 0.0}};
  const Audio stereoAmp{monoAmp[0], {0.0, -0.25, 0.0, 0.0625}};
  const Audio monoCab{{0.75, 0.0, -0.125, 0.0}};
  const Audio stereoCab{monoCab[0], {0.0, 0.0, 0.125, 0.0}};
  std::vector<double> input(1024);
  input[0] = 1.0;
  input[113] = -0.25;
  input[479] = 0.125;
  int cases = 0;
  for (const double rate : {44100.0, 48000.0, 96000.0})
  {
    const std::vector<Audio> amps{{}, monoAmp, stereoAmp};
    const std::vector<Audio> cabs{{}, monoCab, stereoCab, monoCab, stereoCab};
    const std::vector<std::string> ampPaths{
      "", files.nam("amp-mono.nam", monoAmp, rate), files.nam("amp-stereo.nam", stereoAmp, rate)};
    const std::vector<std::string> cabPaths{
      "", files.nam("cab-mono.nam", monoCab, rate), files.nam("cab-stereo.nam", stereoCab, rate),
      files.wav("cab-mono.WAV", monoCab, rate), files.wav("cab-stereo.WAV", stereoCab, rate)};
    for (size_t amp = 0; amp < amps.size(); ++amp)
      for (size_t cab = 0; cab < cabs.size(); ++cab)
        for (const int outputs : {1, 2})
          for (const bool enabled : {false, true})
          {
            HostedPlugin plugin(factory, info, rate, outputs);
            plugin.load(ampPaths[amp], cabPaths[cab], enabled);
            const auto actual = plugin.render({input});
            const double gain = enabled && cab >= 3 ? std::pow(10.0, -18.0 / 20.0) * 48000.0 / rate : 1.0;
            compare(actual, expected_audio(input, amps[amp], enabled ? cabs[cab] : Audio{}, outputs, rate, gain));
            const uint32 expectedTail = static_cast<uint32>(2 * rate + (amp ? 4 : 0) + (enabled && cab ? 4 : 0));
            require(plugin.tail() == expectedTail, "Incorrect stereo chain tail");
            require(plugin.latency() == 0, "Native Linear processing added latency");
            ++cases;
          }
  }
  // EQ and the input-triggered gate must apply identically to each amp channel.
  const auto amp = files.nam("linked-amp.nam", stereoAmp, 48000.0);
  const auto left = files.nam("linked-left.nam", {stereoAmp[0]}, 48000.0);
  const auto right = files.nam("linked-right.nam", {stereoAmp[1]}, 48000.0);
  const auto cab = files.nam("linked-cab.nam", monoCab, 48000.0);
  std::vector<double> guitar(4096);
  for (size_t i = 0; i < guitar.size(); ++i)
    guitar[i] = 0.2 * std::sin(0.071 * i);
  HostedPlugin stereo(factory, info, 48000.0, 2), monoLeft(factory, info, 48000.0, 1),
    monoRight(factory, info, 48000.0, 1);
  stereo.load(amp, cab, true, true);
  monoLeft.load(left, cab, true, true);
  monoRight.load(right, cab, true, true);
  compare(stereo.render({guitar}), {monoLeft.render({guitar})[0], monoRight.render({guitar})[0]});

  // Replacing either slot must update its width and the mono cab's right instance.
  HostedPlugin replacement(factory, info, 48000.0, 2);
  const std::array<std::pair<Audio, Audio>, 4> changes{
    {{stereoAmp, stereoCab}, {monoAmp, monoCab}, {stereoAmp, monoCab}, {monoAmp, stereoCab}}};
  for (const auto& change : changes)
  {
    replacement.load(
      files.nam("replace-amp.nam", change.first, 48000.0), files.nam("replace-cab.nam", change.second, 48000.0));
    compare(replacement.render({input}), expected_audio(input, change.first, change.second, 2, 48000.0, 1.0));
    replacement.render({std::vector<double>(96000, 0.0)});
  }

  Audio longCab(2, std::vector<double>(16385, 0.0));
  longCab[0].back() = 0.5;
  longCab[1][16001] = -0.25;
  HostedPlugin longIR(factory, info, 48000.0, 2);
  longIR.load("", files.wav("long-stereo.wav", longCab, 48000));
  std::vector<double> impulse(17000, 0.0);
  impulse[0] = 1.0;
  compare(longIR.render({impulse}), expected_audio(impulse, {}, longCab, 2, 48000.0, std::pow(10.0, -18.0 / 20.0)));
  require(longIR.tail() == 96000 + 16385, "Long stereo IR tail was truncated");

  HostedPlugin stereoInput(factory, info, 48000.0, 2, 2);
  stereoInput.load(amp, cab);
  auto rightInput = guitar, averagedInput = guitar;
  for (size_t i = 0; i < guitar.size(); ++i)
  {
    rightInput[i] *= 3.0;
    averagedInput[i] *= 2.0;
  }
  compare(stereoInput.render({guitar, rightInput}), expected_audio(averagedInput, stereoAmp, monoCab, 2, 48000.0, 1.0));

  // A fixed-rate Sequential envelope forces the wrapping resampler to run.
  // Compare each stereo channel against a separate mono plugin instance.
  const auto stereoFixed = files.nam("fixed-stereo.nam", stereoAmp, 48000.0, true);
  const auto leftFixed = files.nam("fixed-left.nam", {stereoAmp[0]}, 48000.0, true);
  const auto rightFixed = files.nam("fixed-right.nam", {stereoAmp[1]}, 48000.0, true);
  const auto monoFixedCab = files.nam("fixed-cab.nam", monoCab, 48000.0, true);
  auto averagedAmp = monoAmp;
  for (size_t i = 0; i < averagedAmp[0].size(); ++i)
    averagedAmp[0][i] = 0.5 * (stereoAmp[0][i] + stereoAmp[1][i]);
  const auto averagedFixed = files.nam("fixed-average.nam", averagedAmp, 48000.0, true);
  for (const double rate : {44100.0, 96000.0})
    for (const bool inCabSlot : {false, true})
    {
      HostedPlugin two(factory, info, rate, 2), l(factory, info, rate, 1), r(factory, info, rate, 1);
      if (inCabSlot)
      {
        two.load("", stereoFixed);
        l.load("", leftFixed);
        r.load("", rightFixed);
      }
      else
      {
        two.load(stereoFixed, monoFixedCab);
        l.load(leftFixed, monoFixedCab);
        r.load(rightFixed, monoFixedCab);
      }
      compare(two.render({guitar}), {l.render({guitar})[0], r.render({guitar})[0]});
      require(two.latency() > 0 && two.latency() == l.latency() && two.latency() == r.latency(),
              "Stereo wrapping resampler reported the wrong latency");
    }
  for (const double rate : {44100.0, 96000.0})
  {
    HostedPlugin two(factory, info, rate, 2), l(factory, info, rate, 1), r(factory, info, rate, 1);
    two.load(stereoFixed, stereoFixed);
    l.load(averagedFixed, leftFixed);
    r.load(averagedFixed, rightFixed);
    compare(two.render({guitar}), {l.render({guitar})[0], r.render({guitar})[0]});
  }
  std::cout << cases << " routing cases plus gate/EQ, replacement, long IR, and resampling checks passed\n";
}

void test_cab_switching_during_playback(const VST3::Hosting::PluginFactory& factory,
                                        const VST3::Hosting::ClassInfo& info)
{
  constexpr double rate = 48000.0;
  Fixtures files;
  const auto amp = files.nam("switch-amp.nam", {{0.5}, {-0.25}}, rate);
  const auto mono = files.nam("switch-mono.nam", {{0.75}}, rate);
  const auto stereo = files.wav("switch-stereo.wav", {{0.5}, {-0.5}}, rate);
  HostedPlugin plugin(factory, info, rate, 2);
  plugin.load(amp, mono);
  // Only replace the cabinet during playback. The stereo amp stays active.
  std::array<std::vector<char>, 2> states{plugin.makeState("", mono), plugin.makeState("", stereo)};
  const Audio input{{0.25, -0.25, 0.25, -0.25}};
  const std::array<std::array<double, 2>, 2> cabGains{{{0.375, -0.1875}, {0.0625, -0.0625}}};
  const std::array<double, 2> outputGains{1.0, std::pow(10.0, -18.0 / 20.0)};
  const double highPassAlpha = 1.0 / (1.0 + 2.0 * std::acos(-1.0) * 5.0 / rate);
  std::array<double, 2> previousInput{}, previousOutput{};
  auto renderCab = [&]() {
    const auto output = plugin.render(input, {4});
    // Each block must match one whole cabinet, including the WAV output gain.
    for (size_t cab = 0; cab < cabGains.size(); ++cab)
    {
      auto lastInput = previousInput, lastOutput = previousOutput;
      bool matches = true;
      for (size_t ch = 0; ch < output.size(); ++ch)
        for (size_t i = 0; i < output[ch].size(); ++i)
        {
          const double sample = cabGains[cab][ch] * input[0][i];
          lastOutput[ch] = highPassAlpha * (lastOutput[ch] + sample - lastInput[ch]);
          lastInput[ch] = sample;
          matches &= std::abs(output[ch][i] - outputGains[cab] * lastOutput[ch]) < 2.0e-6;
        }
      if (matches)
      {
        previousInput = lastInput;
        previousOutput = lastOutput;
        return cab;
      }
    }
    throw std::runtime_error("Playback received a mismatched cabinet");
  };
  require(renderCab() == 0, "Playback did not start with the mono cabinet");

  std::atomic<bool> finished{false};
  std::promise<void> started;
  auto playback = std::async(std::launch::async, [&]() {
    started.set_value();
    std::array<bool, 2> heard{};
    while (!finished.load())
      heard[renderCab()] = true;
    require(heard[0] && heard[1], "Playback did not render both cabinet widths during switching");
  });
  started.get_future().wait();
  try
  {
    // Keep the audio callback running while the host repeatedly loads new state.
    for (int i = 0; i < 2000; ++i)
      plugin.loadState(states[i % states.size()]);
  }
  catch (...)
  {
    finished = true;
    playback.wait();
    throw;
  }
  finished = true;
  playback.get();
  require(renderCab() == 1, "Playback did not adopt the final stereo cabinet");
  std::cout << "Mono and stereo cabinet switches kept both channels coherent during playback\n";
}

void test_large_host_blocks(const VST3::Hosting::PluginFactory& factory, const VST3::Hosting::ClassInfo& info)
{
  constexpr int largeBlockSize = 5000;
  constexpr int finalBlockSize = 371;
  constexpr int frames = 2 * largeBlockSize + finalBlockSize;
  Fixtures files;
  const auto amp = files.nam("large-block-amp.nam", {{0.5, 0.0, 0.125, 0.0}, {0.0, -0.25, 0.0, 0.0625}}, 48000.0, true);
  const auto cab = files.nam("large-block-cab.nam", {{0.75, 0.0, -0.125, 0.0}, {0.0, 0.0, 0.125, 0.0}}, 48000.0, true);
  Audio input(2, std::vector<double>(frames));
  for (int i = 0; i < frames; ++i)
  {
    input[0][i] = 0.2 * std::sin(0.071 * i);
    input[1][i] = 0.3 * std::cos(0.103 * i);
  }

  for (const double rate : {44100.0, 48000.0, 96000.0})
  {
    HostedPlugin large(factory, info, rate, 2, 2, largeBlockSize), small(factory, info, rate, 2, 2);
    large.load(amp, cab, true, true);
    small.load(amp, cab, true, true);
    const auto expected = small.render(input);
    const auto actual = large.render(input, {largeBlockSize});
    for (const auto& channel : expected)
      require(std::any_of(
                channel.end() - finalBlockSize, channel.end(), [](double value) { return std::abs(value) > 1.0e-4; }),
              "Large-block fixture produced no signal in the final block");
    compare(actual, expected);
    require(large.latency() == small.latency(), "Large host blocks changed latency");
    require(large.tail() == small.tail(), "Large host blocks changed the reported tail");
  }
  std::cout << "5000-frame host blocks matched short blocks at 44.1, 48, and 96 kHz\n";
}
} // namespace

int main(int argc, char** argv)
{
  try
  {
    require(argc == 2, "Usage: test_stereo_plugin /path/to/NeuralAmpModeler.vst3");
    HostApplication host;
    PluginContextFactory::instance().setPluginContext(&host);
    std::string error;
    auto module = VST3::Hosting::Module::create(argv[1], error);
    require(bool(module), error.c_str());
    bool tested = false;
    for (const auto& info : module->getFactory().classInfos())
      if (info.category() == kVstAudioEffectClass)
      {
        test_routing(module->getFactory(), info);
        test_cab_switching_during_playback(module->getFactory(), info);
        test_large_host_blocks(module->getFactory(), info);
        tested = true;
      }
    require(tested, "No audio processor found in VST3");
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
