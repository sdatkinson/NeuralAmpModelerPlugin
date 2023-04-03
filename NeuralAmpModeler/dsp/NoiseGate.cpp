//
//  NoiseGate.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 2/5/23.
//

#include <cmath> // pow

#include "NoiseGate.h"

double clamp(double v, double lo, double hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

double _LevelToDB(const double db) { return 10.0 * log10(db); }

double _DBToLevel(const double level) { return pow(10.0, level / 10.0); }

dsp::noise_gate::Trigger::Trigger()
    : mParams(0.05, -60.0, 1.5, 0.002, 0.050, 0.050), mSampleRate(0) {}

double signum(const double val) { return (0.0 < val) - (val < 0.0); }

iplug::sample **dsp::noise_gate::Trigger::Process(iplug::sample **inputs,
                                                  const size_t numChannels,
                                                  const size_t numFrames) {
  this->_PrepareBuffers(numChannels, numFrames);

  // A bunch of numbers we'll use a few times.
  const double alpha =
      pow(0.5, 1.0 / (this->mParams.GetTime() * this->mSampleRate));
  const double beta = 1.0 - alpha;
  const double threshold = this->mParams.GetThreshold();
  const double dt = 1.0 / this->mSampleRate;
  const double maxHold = this->mParams.GetHoldTime();
  const double maxGainReduction = this->_GetMaxGainReduction();
  // Amount of open or close in a sample: rate times time
  const double dOpen =
      -this->_GetMaxGainReduction() / this->mParams.GetOpenTime() * dt; // >0
  const double dClose =
      this->_GetMaxGainReduction() / this->mParams.GetCloseTime() * dt; // <0

  // The main algorithm: compute the gain reduction
  for (auto c = 0; c < numChannels; c++) {
    for (auto s = 0; s < numFrames; s++) {
      this->mLevel[c] = clamp(alpha * this->mLevel[c] +
                                       beta * (inputs[c][s] * inputs[c][s]),
                                   MINIMUM_LOUDNESS_POWER, 1000.0);
      const double levelDB = _LevelToDB(this->mLevel[c]);
      if (this->mState[c] == dsp::noise_gate::Trigger::State::HOLDING) {
        this->mGainReductionDB[c][s] = 0.0;
        this->mLastGainReductionDB[c] = 0.0;
        if (levelDB < threshold) {
          this->mTimeHeld[c] += dt;
          if (this->mTimeHeld[c] >= maxHold)
            this->mState[c] = dsp::noise_gate::Trigger::State::MOVING;
        } else {
          this->mTimeHeld[c] = 0.0;
        }
      } else { // Moving
        const double targetGainReduction = this->_GetGainReduction(levelDB);
        if (targetGainReduction > this->mLastGainReductionDB[c]) {
          const double dGain = clamp(
              0.5 * (targetGainReduction - this->mLastGainReductionDB[c]), 0.0,
              dOpen);
          this->mLastGainReductionDB[c] += dGain;
          if (this->mLastGainReductionDB[c] >= 0.0) {
            this->mLastGainReductionDB[c] = 0.0;
            this->mState[c] = dsp::noise_gate::Trigger::State::HOLDING;
            this->mTimeHeld[c] = 0.0;
          }
        } else if (targetGainReduction < this->mLastGainReductionDB[c]) {
          const double dGain = clamp(
              0.5 * (targetGainReduction - this->mLastGainReductionDB[c]),
              dClose, 0.0);
          this->mLastGainReductionDB[c] += dGain;
          if (this->mLastGainReductionDB[c] < maxGainReduction) {
            this->mLastGainReductionDB[c] = maxGainReduction;
          }
        }
        this->mGainReductionDB[c][s] = this->mLastGainReductionDB[c];
      }
    }
  }

  // Share the results with gain objects that are listening to this trigger:
  for (auto gain = this->mGainListeners.begin();
       gain != this->mGainListeners.end(); ++gain)
    (*gain)->SetGainReductionDB(this->mGainReductionDB);

  // Copy input to output
  for (auto c = 0; c < numChannels; c++)
    memcpy(this->mOutputs[c].data(), inputs[c],
           numFrames * sizeof(iplug::sample));
  return this->_GetPointers();
}

void dsp::noise_gate::Trigger::_PrepareBuffers(const size_t numChannels,
                                               const size_t numFrames) {
  const size_t oldChannels = this->_GetNumChannels();
  const size_t oldFrames = this->_GetNumFrames();
  this->DSP::_PrepareBuffers(numChannels, numFrames);

  const bool updateChannels = numChannels != oldChannels;
  const bool updateFrames = updateChannels || numFrames != oldFrames;

  if (updateChannels || updateFrames) {
    const double maxGainReduction = this->_GetMaxGainReduction();
    if (updateChannels) {
      this->mGainReductionDB.resize(numChannels);
      this->mLastGainReductionDB.resize(numChannels);
      std::fill(this->mLastGainReductionDB.begin(),
                this->mLastGainReductionDB.end(), maxGainReduction);
      this->mState.resize(numChannels);
      std::fill(this->mState.begin(), this->mState.end(),
                dsp::noise_gate::Trigger::State::MOVING);
      this->mLevel.resize(numChannels);
      std::fill(this->mLevel.begin(), this->mLevel.end(),
                MINIMUM_LOUDNESS_POWER);
      this->mTimeHeld.resize(numChannels);
      std::fill(this->mTimeHeld.begin(), this->mTimeHeld.end(), 0.0);
    }
    if (updateFrames) {
      for (auto i = 0; i < this->mGainReductionDB.size(); i++) {
        this->mGainReductionDB[i].resize(numFrames);
        std::fill(this->mGainReductionDB[i].begin(),
                  this->mGainReductionDB[i].end(), maxGainReduction);
      }
    }
  }
}

// Gain========================================================================

iplug::sample **dsp::noise_gate::Gain::Process(iplug::sample **inputs,
                                               const size_t numChannels,
                                               const size_t numFrames) {
  // Assume that SetGainReductionDB() was just called to get data from a
  // trigger. Could use listeners...
  this->_PrepareBuffers(numChannels, numFrames);

  if (this->mGainReductionDB.size() != numChannels) {
    std::stringstream ss;
    ss << "Gain module expected to operate on " << this->mGainReductionDB.size()
       << "channels, but " << numChannels << " were provided.";
    throw std::runtime_error(ss.str());
  }
  if ((this->mGainReductionDB.size() == 0) && (numFrames > 0)) {
    std::stringstream ss;
    ss << "No channels expected by gain module, yet " << numFrames
       << " were provided?";
    throw std::runtime_error(ss.str());
  } else if (this->mGainReductionDB[0].size() != numFrames) {
    std::stringstream ss;
    ss << "Gain module expected to operate on "
       << this->mGainReductionDB[0].size() << "frames, but " << numFrames
       << " were provided.";
    throw std::runtime_error(ss.str());
  }

  // Apply gain!
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      this->mOutputs[c][s] =
          _DBToLevel(this->mGainReductionDB[c][s]) * inputs[c][s];

  return this->_GetPointers();
}
