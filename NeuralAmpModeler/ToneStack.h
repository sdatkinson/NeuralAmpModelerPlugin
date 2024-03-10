#pragma once

#include <string>
#include "AudioDSPTools/dsp/dsp.h"
#include "AudioDSPTools/dsp/RecursiveLinearFilter.h"

namespace dsp
{
namespace tone_stack
{
class AbstractToneStack
{
public:
  // Compute in the real-time loop
  virtual DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const int numChannels, const int numFrames) = 0;
  // Any preparation. Call from Reset() in the plugin
  virtual void Reset(const double sampleRate, const int maxBlockSize)
  {
    mSampleRate = sampleRate;
    mMaxBlockSize = maxBlockSize;
  };
  // Set the various parameters of your tone stack by name.
  // Call this during OnParamChange()
  virtual void SetParam(const std::string name, const double val) = 0;

protected:
  double GetSampleRate() const { return mSampleRate; };
  double mSampleRate = 0.0;
  int mMaxBlockSize = 0;
};

class BasicNamToneStack : public AbstractToneStack
{
public:
  BasicNamToneStack()
  {
    SetParam("bass", 5.0);
    SetParam("middle", 5.0);
    SetParam("treble", 5.0);
  };
  ~BasicNamToneStack() = default;

  DSP_SAMPLE** Process(DSP_SAMPLE** inputs, const int numChannels, const int numFrames);
  virtual void Reset(const double sampleRate, const int maxBlockSize) override;
  // :param val: Assumed to be between 0 and 10, 5 is "noon"
  void SetParam(const std::string name, const double val);


protected:
  recursive_linear_filter::LowShelf mToneBass;
  recursive_linear_filter::Peaking mToneMid;
  recursive_linear_filter::HighShelf mToneTreble;

  // HACK not DRY w knob defs
  double mBassVal = 5.0;
  double mMiddleVal = 5.0;
  double mTrebleVal = 5.0;
};
}; // namespace tone_stack
}; // namespace dsp
