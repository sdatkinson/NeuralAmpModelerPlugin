//
//  NoiseGate.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 2/5/23.
//

#include <cmath> // pow

#include "NoiseGate.h"

dsp::noise_gate::Trigger::Trigger()
    : mParams(0.05, -60.0, 1.5, 0.002, 0.050, 0.050), mSampleRate(0) {}

double signum(const double val) { return (0.0 < val) - (val < 0.0); }

iplug::sample **dsp::noise_gate::Trigger::Process(iplug::sample **inputs,
                                                  const size_t numChannels,
                                                  const size_t numFrames) {
  this->_PrepareBuffers(numChannels, numFrames);

  // A bunch of numbers we'll use a few times.
  const double alpha = pow(0.5, -this->mParams.GetTime() * this->mSampleRate);
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
      this->mLevel[c] = std::clamp(alpha * this->mLevel[c] +
                                       beta * (inputs[c][s] * inputs[c][s]),
                                   MINIMUM_LOUDNESS_POWER, 1000.0);
      const double levelDB = 10.0 * log10(this->mLevel[c]);
      if (this->mState[c] == dsp::noise_gate::Trigger::State::HOLDING) {
        this->mGainReductionDB[c][s] = 0.0;
        this->mLastGainReductionDB[c] = 0.0;
        if (levelDB < threshold) {
          this->mTimeHeld[c] += dt;
          if (this->mTimeHeld[c] >= maxHold)
            this->mState[c] = dsp::noise_gate::Trigger::State::MOVING;
        }
      } else { // Moving
        const double targetGainReduction = this->_GetGainReduction(levelDB);
        if (targetGainReduction > this->mLastGainReductionDB[c]) {
          this->mLastGainReductionDB[c] += dOpen;
          if (this->mLastGainReductionDB[c] >= 0.0) {
            this->mLastGainReductionDB[c] = 0.0;
            this->mState[c] = dsp::noise_gate::Trigger::State::HOLDING;
          }
        } else if (targetGainReduction < this->mLastGainReductionDB[c]) {
          this->mLastGainReductionDB[c] += dClose;
          if (this->mLastGainReductionDB[c] < maxGainReduction) {
            this->mLastGainReductionDB[c] = maxGainReduction;
          }
        }
        this->mGainReductionDB[c][s] = this->mLastGainReductionDB[c];
      }
    }
  }

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

  if (numChannels != oldChannels) {
    this->mGainReductionDB.resize(numChannels);
    this->mLastGainReductionDB.resize(numChannels);
    std::fill(this->mLastGainReductionDB.begin(),
              this->mLastGainReductionDB.end(), this->_GetMaxGainReduction());
    this->mState.resize(numChannels);
    std::fill(this->mState.begin(), this->mState.end(),
              dsp::noise_gate::Trigger::State::MOVING);
    this->mLevel.resize(numChannels);
    std::fill(this->mLevel.begin(), this->mLevel.end(), MINIMUM_LOUDNESS_POWER);
  }

  if (numChannels != oldChannels || numFrames != oldFrames) {
    for (auto i = 0; i < this->mGainReductionDB.size(); i++) {
      this->mGainReductionDB[i].resize(numFrames);
      std::fill(this->mGainReductionDB[i].begin(),
                this->mGainReductionDB[i].end(), 0.0);
    }
  }
}
