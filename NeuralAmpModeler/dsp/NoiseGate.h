//
//  NoiseGate.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 2/5/23.
//

#ifndef NoiseGate_h
#define NoiseGate_h

#include <cmath>
#include <unordered_set>
#include <vector>

#include "dsp.h"

namespace dsp {
namespace noise_gate {
// Disclaimer: No one told me how noise gates work. I'm just going to try
// and have fun with it and see if I like what I get! :D

// "The noise floor." The loudness of anything quieter than this is bumped
// up to as if it were this loud for gating purposes (i.e. computing gain
// reduction).
const double MINIMUM_LOUDNESS_DB = -120.0;
const double MINIMUM_LOUDNESS_POWER = pow(10.0, MINIMUM_LOUDNESS_DB / 10.0);

// Parts 2: The gain module.
// This applies the gain reduction taht was determined by the trigger.
// It's declared first so that the trigger can define listeners without a
// forward declaration.

// The class that applies the gain reductions calculated by a trigger instance.
class Gain : public DSP {
public:
  iplug::sample **Process(iplug::sample **inputs, const size_t numChannels,
                          const size_t numFrames) override;

  void SetGainReductionDB(std::vector<std::vector<double>> &gainReductionDB) {
    this->mGainReductionDB = gainReductionDB;
  }

private:
  std::vector<std::vector<double>> mGainReductionDB;
};

// Part 1 of the noise gate: the trigger.
// This listens to a stream of incoming audio and determines how much gain
// to apply based on the loudness of the signal.

class TriggerParams {
public:
  TriggerParams(const double time, const double threshold, const double ratio,
                const double openTime, const double holdTime,
                const double closeTime)
      : mTime(time), mThreshold(threshold), mRatio(ratio), mOpenTime(openTime),
        mHoldTime(holdTime), mCloseTime(closeTime){};

  double GetTime() const { return this->mTime; };
  double GetThreshold() const { return this->mThreshold; };
  double GetRatio() const { return this->mRatio; };
  double GetOpenTime() const { return this->mOpenTime; };
  double GetHoldTime() const { return this->mHoldTime; };
  double GetCloseTime() const { return this->mCloseTime; };

private:
  // The time constant for quantifying the loudness of the signal.
  double mTime;
  // The threshold at which expanssion starts
  double mThreshold;
  // The compression ratio.
  double mRatio;
  // How long it takes to go from maximum gain reduction to zero.
  double mOpenTime;
  // How long to stay open before starting to close.
  double mHoldTime;
  // How long it takes to go from open to maximum gain reduction.
  double mCloseTime;
};

class Trigger : public DSP {
public:
  Trigger();

  iplug::sample **Process(iplug::sample **inputs, const size_t numChannels,
                          const size_t numFrames) override;
  std::vector<std::vector<double>> GetGainReduction() const {
    return this->mGainReductionDB;
  };
  void SetParams(const TriggerParams &params) { this->mParams = params; };
  void SetSampleRate(const double sampleRate) {
    this->mSampleRate = sampleRate;
  }
  std::vector<std::vector<double>> GetGainReductionDB() const {
    return this->mGainReductionDB;
  };

  void AddListener(Gain *gain) {
    // This might be risky dropping a raw pointer, but I don't think that the
    // gain would be destructed, so probably ok.
    this->mGainListeners.insert(gain);
  }

private:
  enum class State { MOVING = 0, HOLDING };

  double _GetGainReduction(const double levelDB) const {
    const double threshold = this->mParams.GetThreshold();
    return levelDB < threshold
               ? (this->mParams.GetRatio() - 1.0) * (levelDB - threshold)
               : 0.0;
  }
  double _GetMaxGainReduction() const {
    return this->_GetGainReduction(MINIMUM_LOUDNESS_DB);
  }
  virtual void _PrepareBuffers(const size_t numChannels,
                               const size_t numFrames) override;

  TriggerParams mParams;
  std::vector<State> mState; // One per channel
  std::vector<double> mLevel;

  // Hold the vectors of gain reduction for the block, in dB.
  // These can be given to the Gain object.
  std::vector<std::vector<double>> mGainReductionDB;
  std::vector<double> mLastGainReductionDB;

  double mSampleRate;
  // How long we've been holding
  std::vector<double> mTimeHeld;

  std::unordered_set<Gain *> mGainListeners;
};

}; // namespace noise_gate
}; // namespace dsp

#endif /* NoiseGate_h */
