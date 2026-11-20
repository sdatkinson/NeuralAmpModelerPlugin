#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <variant>

#include "../iPlug2/IPlug/IPlugConstants.h"

// AudioDSPTools uses iPlug's constants without including their declarations.
#ifndef DEFAULT_BLOCK_SIZE
namespace dsp
{
using iplug::DEFAULT_BLOCK_SIZE;
}
#endif
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

// Get the sample rate of a NAM model.
// Sometimes, the model doesn't know its own sample rate; this wrapper guesses 48k based on the way that most
// people have used NAM in the past.
inline double GetNAMSampleRate(const std::unique_ptr<nam::DSP>& model)
{
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  const double assumedSampleRate = 48000.0;
  const double reportedEncapsulatedSampleRate = model->GetExpectedSampleRate();
  const double encapsulatedSampleRate =
    reportedEncapsulatedSampleRate <= 0.0 ? assumedSampleRate : reportedEncapsulatedSampleRate;
  return encapsulatedSampleRate;
};

class ResamplingNAM : public nam::DSP
{
public:
  // Resampling wrapper around the NAM models
  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate)
  : nam::DSP(encapsulated->NumInputChannels(), encapsulated->NumOutputChannels(), expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  , mSupportsArbitrarySampleRate(mEncapsulated->SupportsArbitrarySampleRate())
  , mResampler(MakeResampler(NumInputChannels(), NumOutputChannels(), GetEncapsulatedSampleRate()))
  , mBlockProcessFunc(
      [this](NAM_SAMPLE** input, NAM_SAMPLE** output, int frames) { mEncapsulated->process(input, output, frames); })
  {
    // Get the other information from the encapsulated NAM so that we can tell the outside world about what we're
    // holding.
    if (mEncapsulated->HasLoudness())
    {
      SetLoudness(mEncapsulated->GetLoudness());
    }
    if (mEncapsulated->HasInputLevel())
    {
      SetInputLevel(mEncapsulated->GetInputLevel());
    }
    if (mEncapsulated->HasOutputLevel())
    {
      SetOutputLevel(mEncapsulated->GetOutputLevel());
    }

    // And be ready
    int maxBlockSize = 2048; // Conservative
    Reset(expected_sample_rate, maxBlockSize);
  };

  ~ResamplingNAM() = default;

  void prewarm() override { mEncapsulated->prewarm(); };

  // Report the encapsulated model's history length in host-rate samples.
  int GetPrewarmSamples() override
  {
    double samples = mEncapsulated->GetPrewarmSamples();
    if (NeedToResample())
      samples *= GetExpectedSampleRate() / GetEncapsulatedSampleRate();
    return static_cast<int>(std::clamp(std::ceil(samples), 0.0,
                                       static_cast<double>(std::numeric_limits<int>::max())));
  }

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
      // We can afford to be careful
      throw std::runtime_error("More frames were provided than the max expected!");

    if (!NeedToResample())
    {
      mEncapsulated->process(input, output, num_frames);
    }
    else
    {
      std::visit([&](auto& resampler) { resampler.ProcessBlock(input, output, num_frames, mBlockProcessFunc); },
                 mResampler);
    }
  };

  int GetLatency() const
  {
    if (!NeedToResample())
      return 0;
    return std::visit([](const auto& resampler) { return resampler.GetLatency(); }, mResampler);
  };

  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    mExpectedSampleRate = sampleRate;
    mMaxExternalBlockSize = maxBlockSize;
    // Adaptive models convert their own coefficients to the host rate. Fixed-rate models must be reset at the
    // rate of the audio they will actually receive, including Linear children inside a Sequential model.
    const double modelSampleRate = mSupportsArbitrarySampleRate ? sampleRate : GetEncapsulatedSampleRate();
    int maxEncapsulatedBlockSize = maxBlockSize;
    if (NeedToResample())
    {
      std::visit([&](auto& resampler) { resampler.Reset(sampleRate, maxBlockSize); }, mResampler);
      maxEncapsulatedBlockSize =
        static_cast<int>(std::ceil(static_cast<double>(maxBlockSize) * modelSampleRate / sampleRate));
    }
    mEncapsulated->Reset(modelSampleRate, maxEncapsulatedBlockSize);
  };

  // So that we can let the world know if we're resampling (useful for debugging)
  double GetEncapsulatedSampleRate() const { return GetNAMSampleRate(mEncapsulated); };

  nam::SlimmableModel* GetSlimmableModel() { return dynamic_cast<nam::SlimmableModel*>(mEncapsulated.get()); }
  const nam::SlimmableModel* GetSlimmableModel() const
  {
    return dynamic_cast<const nam::SlimmableModel*>(mEncapsulated.get());
  }

private:
  template <int Inputs, int Outputs>
  using ChannelResampler = dsp::ResamplingContainer<NAM_SAMPLE, Inputs, 12, Outputs>;
  using Resampler =
    std::variant<ChannelResampler<1, 1>, ChannelResampler<1, 2>, ChannelResampler<2, 1>, ChannelResampler<2, 2>>;

  static Resampler MakeResampler(int inputs, int outputs, double sampleRate)
  {
    if (inputs > 2 || outputs > 2)
      throw std::runtime_error("ResamplingNAM supports at most two input and output channels.");
    if (inputs == 1)
    {
      if (outputs == 1)
        return Resampler(std::in_place_type<ChannelResampler<1, 1>>, sampleRate);
      return Resampler(std::in_place_type<ChannelResampler<1, 2>>, sampleRate);
    }
    if (outputs == 1)
      return Resampler(std::in_place_type<ChannelResampler<2, 1>>, sampleRate);
    return Resampler(std::in_place_type<ChannelResampler<2, 2>>, sampleRate);
  }

  bool NeedToResample() const
  {
    return !mSupportsArbitrarySampleRate && GetExpectedSampleRate() != GetEncapsulatedSampleRate();
  };
  // The encapsulated NAM
  std::unique_ptr<nam::DSP> mEncapsulated;
  const bool mSupportsArbitrarySampleRate;
  Resampler mResampler;
  std::function<void(NAM_SAMPLE**, NAM_SAMPLE**, int)> mBlockProcessFunc;

  // Used to check that we don't get too large a block to process.
  int mMaxExternalBlockSize = 0;
};
