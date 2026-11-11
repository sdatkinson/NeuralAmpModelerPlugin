#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "ResamplingNAM.h"
#include "NAM/linear.h"
#include "NAM/sequential.h"

namespace
{
void require(bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}

class RateTrackingDSP : public nam::DSP
{
public:
  RateTrackingDSP(double trainingRate, bool adaptive)
  : nam::DSP(1, 1, trainingRate)
  , mAdaptive(adaptive)
  {
  }

  bool SupportsArbitrarySampleRate() override { return mAdaptive; }

  int GetPrewarmSamples() override
  {
    return mAdaptive ? static_cast<int>(std::ceil(0.1 * mProcessingRate)) : 4801;
  }

  void Reset(double sampleRate, int maxBlockSize) override
  {
    mProcessingRate = sampleRate;
    nam::DSP::Reset(sampleRate, maxBlockSize);
  }

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, int frames) override
  {
    require(frames <= GetMaxBufferSize(), "Model received more samples than its prepared buffer size");
    std::copy_n(input[0], frames, output[0]);
  }

  double mProcessingRate = 0.0;

private:
  bool mAdaptive;
};

std::unique_ptr<nam::Linear> make_linear()
{
  const std::vector<float> weights{0.5f, -0.25f, 0.125f, 0.0625f};
  return std::make_unique<nam::Linear>(1, 1, static_cast<int>(weights.size()), false, weights, 48000.0);
}

std::unique_ptr<nam::DSP> make_sequential()
{
  std::vector<std::unique_ptr<nam::DSP>> models;
  models.push_back(std::make_unique<RateTrackingDSP>(48000.0, false));
  models.push_back(make_linear());
  return std::make_unique<nam::sequential::SequentialModel>(std::move(models), 48000.0);
}

template <typename Process>
std::vector<NAM_SAMPLE> render(Process process)
{
  std::vector<NAM_SAMPLE> input(4096), output(input.size());
  for (size_t i = 0; i < input.size(); ++i)
    input[i] = 0.2 * std::sin(0.071 * i) + 0.1 * std::cos(0.013 * i);
  const int blockSizes[]{1, 7, 64, 137, 256, 3};
  size_t offset = 0, block = 0;
  while (offset < input.size())
  {
    const auto frames = std::min<int>(blockSizes[block++ % 6], static_cast<int>(input.size() - offset));
    NAM_SAMPLE* in[]{input.data() + offset};
    NAM_SAMPLE* out[]{output.data() + offset};
    process(in, out, frames);
    offset += frames;
  }
  return output;
}

void require_matching_audio(const std::vector<NAM_SAMPLE>& actual, const std::vector<NAM_SAMPLE>& expected)
{
  for (size_t i = 0; i < actual.size(); ++i)
    require(std::isfinite(actual[i]) && std::abs(actual[i] - expected[i]) < 1.0e-6,
            "Wrapped audio differs from processing at the correct sample rate");
}

void test_reset_rates_and_metadata()
{
  for (bool adaptive : {false, true})
  {
    auto model = std::make_unique<RateTrackingDSP>(48000.0, adaptive);
    auto* tracked = model.get();
    model->SetInputLevel(12.0);
    model->SetOutputLevel(4.0);
    model->SetLoudness(-18.0);
    ResamplingNAM wrapped(std::move(model), 44100.0);
    for (double hostRate : {44100.0, 48000.0, 96000.0, 44100.0})
    {
      wrapped.Reset(hostRate, 256);
      require(tracked->mProcessingRate == (adaptive ? hostRate : 48000.0), "Incorrect model processing rate");
      require(wrapped.GetExpectedSampleRate() == hostRate, "Wrapper lost the host sample rate");
      require(wrapped.GetEncapsulatedSampleRate() == 48000.0, "Wrapper lost the training sample rate");
      require((wrapped.GetLatency() == 0) == (adaptive || hostRate == 48000.0), "Incorrect resampling latency");
      const int expectedHistory = adaptive ? static_cast<int>(std::ceil(0.1 * hostRate))
                                          : static_cast<int>(std::ceil(4801.0 * hostRate / 48000.0));
      require(wrapped.GetPrewarmSamples() == expectedHistory, "History length was not reported in host-rate samples");
      render([&](auto** in, auto** out, int frames) { wrapped.process(in, out, frames); });
    }
    require(wrapped.HasInputLevel() && wrapped.GetInputLevel() == 12.0, "Input calibration was lost");
    require(wrapped.HasOutputLevel() && wrapped.GetOutputLevel() == 4.0, "Output calibration was lost");
    require(wrapped.HasLoudness() && wrapped.GetLoudness() == -18.0, "Loudness metadata was lost");
  }

  auto unknown = std::make_unique<RateTrackingDSP>(NAM_UNKNOWN_EXPECTED_SAMPLE_RATE, false);
  auto* tracked = unknown.get();
  ResamplingNAM legacy(std::move(unknown), 96000.0);
  require(tracked->mProcessingRate == 48000.0 && legacy.GetEncapsulatedSampleRate() == 48000.0,
          "Unknown training rates must retain the legacy 48 kHz fallback");
}

void test_native_linear_audio()
{
  ResamplingNAM wrapped(make_linear(), 48000.0);
  auto reference = make_linear();
  for (double rate : {44100.0, 96000.0, 48000.0, 44100.0})
  {
    wrapped.Reset(rate, 256);
    reference->Reset(rate, 256);
    require(wrapped.GetLatency() == 0, "Adaptive Linear model must not add wrapper latency");
    require(wrapped.GetPrewarmSamples() == static_cast<int>(std::ceil(4.0 * rate / 48000.0)),
            "Native Linear history was converted twice or lost");
    const auto actual = render([&](auto** in, auto** out, int frames) { wrapped.process(in, out, frames); });
    const auto expected = render([&](auto** in, auto** out, int frames) { reference->process(in, out, frames); });
    require_matching_audio(actual, expected);
  }
}

void test_fixed_rate_sequential_audio()
{
  ResamplingNAM wrapped(make_sequential(), 48000.0);
  auto reference = make_sequential();
  dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> resampler(48000.0);
  for (double hostRate : {44100.0, 96000.0, 48000.0, 44100.0})
  {
    wrapped.Reset(hostRate, 256);
    const auto modelBlockSize = static_cast<int>(std::ceil(256.0 * 48000.0 / hostRate));
    reference->Reset(48000.0, modelBlockSize);
    if (hostRate != 48000.0)
      resampler.Reset(hostRate, 256);
    const auto actual = render([&](auto** in, auto** out, int frames) { wrapped.process(in, out, frames); });
    const auto expected = render([&](auto** in, auto** out, int frames) {
      if (hostRate == 48000.0)
        reference->process(in, out, frames);
      else
        resampler.ProcessBlock(in, out, frames, [&](auto** modelIn, auto** modelOut, int count) {
          reference->process(modelIn, modelOut, count);
        });
    });
    require_matching_audio(actual, expected);
  }
}
} // namespace

int main()
{
  try
  {
    test_reset_rates_and_metadata();
    test_native_linear_audio();
    test_fixed_rate_sequential_audio();
    std::cout << "Sample-rate integration tests passed\n";
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
