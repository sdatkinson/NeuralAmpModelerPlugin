#include "ToneStack.h"

DSP_SAMPLE** dsp::tone_stack::BasicNamToneStack::Process(DSP_SAMPLE** inputs, const int numChannels,
                                                         const int numFrames)
{
  DSP_SAMPLE** bassPointers = mToneBass.Process(inputs, numChannels, numFrames);
  DSP_SAMPLE** midPointers = mToneMid.Process(bassPointers, numChannels, numFrames);
  DSP_SAMPLE** treblePointers = mToneTreble.Process(midPointers, numChannels, numFrames);
  return treblePointers;
}

void dsp::tone_stack::BasicNamToneStack::Reset(const double sampleRate, const int maxBlockSize)
{
  dsp::tone_stack::AbstractToneStack::Reset(sampleRate, maxBlockSize);

  // Refresh the params!
  SetParam("bass", mBassVal);
  SetParam("middle", mMiddleVal);
  SetParam("treble", mTrebleVal);
}

void dsp::tone_stack::BasicNamToneStack::SetParam(const std::string name, const double val)
{
  if (name == "bass")
  {
    // HACK: Store for refresh
    mBassVal = val;
    const double sampleRate = GetSampleRate();
    const double bassGainDB = 4.0 * (val - 5.0); // +/- 20
    const double bassFrequency = 150.0;
    const double bassQuality = 0.707;
    recursive_linear_filter::BiquadParams bassParams(sampleRate, bassFrequency, bassQuality, bassGainDB);
    mToneBass.SetParams(bassParams);
  }
  else if (name == "middle")
  {
    // HACK: Store for refresh
    mMiddleVal = val;
    const double sampleRate = GetSampleRate();
    const double midGainDB = 3.0 * (val - 5.0); // +/- 15
    const double midFrequency = 425.0;
    // Wider EQ on mid bump up to sound less honky.
    const double midQuality = midGainDB < 0.0 ? 1.5 : 0.7;
    recursive_linear_filter::BiquadParams midParams(sampleRate, midFrequency, midQuality, midGainDB);
    mToneMid.SetParams(midParams);
  }
  else if (name == "treble")
  {
    // HACK: Store for refresh
    mTrebleVal = val;
    const double sampleRate = GetSampleRate();
    const double trebleGainDB = 2.0 * (val - 5.0); // +/- 10
    const double trebleFrequency = 1800.0;
    const double trebleQuality = 0.707;
    recursive_linear_filter::BiquadParams trebleParams(sampleRate, trebleFrequency, trebleQuality, trebleGainDB);
    mToneTreble.SetParams(trebleParams);
  }
}
