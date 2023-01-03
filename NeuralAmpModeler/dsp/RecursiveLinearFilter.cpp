//
//  RecursiveLinearFilter.cpp
//  
//
//  Created by Steven Atkinson on 12/28/22.
//
// See: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html

#include <algorithm>  // std::fill
#include <stdexcept>

#include "RecursiveLinearFilter.h"

recursive_linear_filter::Base::Base(const size_t inputDegree,
                                    const size_t outputDegree) :
dsp::DSP(),
mInputStart(inputDegree),  // 1 is subtracted before first use
mOutputStart(outputDegree)
{
  this->mInputCoefficients.resize(inputDegree);
  this->mOutputCoefficients.resize(outputDegree);
}

iplug::sample** recursive_linear_filter::Base::Process(iplug::sample** inputs,
                                                       const size_t numChannels,
                                                       const size_t numFrames)
{
  this->_PrepareBuffers(numChannels, numFrames);
  long inputStart=0;
  long outputStart=0;
  // Degree = longest history
  // E.g. if y[n] explicitly depends on x[n-2], then input degree is 3 (n,n-1,n-2).
  // NOTE: output degree is never 1 because y[n] is never used to explicitly calculate...itself!
  //  0,2,3,... are fine.
  const size_t inputDegree = this->_GetInputDegree();
  const size_t outputDegree = this->_GetOutputDegree();
  for (auto c=0; c<numChannels; c++) {
    inputStart = this->mInputStart;  // Should be plenty fine
    outputStart = this->mOutputStart;
    for (auto s=0; s<numFrames; s++) {
      double out = 0.0;
      // Compute input terms
      inputStart -= 1;
      if (inputStart < 0)
        inputStart = inputDegree - 1;
      this->mInputHistory[c][inputStart] = inputs[c][s];  // Store current input
      for (auto i=0; i<inputDegree; i++)
        out += this->mInputCoefficients[i] * this->mInputHistory[c][(inputStart + i) % inputDegree];
      
      // Output terms
      outputStart -= 1;
      if (outputStart < 0)
        outputStart = outputDegree - 1;
      for (auto i=1; i<outputDegree; i++)
        out += this->mOutputCoefficients[i] * this->mOutputHistory[c][(outputStart + i) % outputDegree];
      // Store the output!
      if (outputDegree >= 1)
        this->mOutputHistory[c][outputStart] = out;
      this->mOutputs[c][s] = out;
    }
  }
  this->mInputStart = inputStart;
  this->mOutputStart = outputStart;
  return this->_GetPointers();
}

void recursive_linear_filter::Base::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  // Check for new channel count *before* parent class ensures they match!
  const bool newChannels = this->_GetNumChannels() != numChannels;
  // Parent implementation takes care of mOutputs and mOutputPointers
  this->dsp::DSP::_PrepareBuffers(numChannels, numFrames);
  if (newChannels) {
    this->mInputHistory.resize(numChannels);
    this->mOutputHistory.resize(numChannels);
    const size_t inputDegree = this->_GetInputDegree();
    const size_t outputDegree = this->_GetOutputDegree();
    for (auto c=0; c<numChannels; c++) {
      this->mInputHistory[c].resize(inputDegree);
      this->mOutputHistory[c].resize(outputDegree);
      std::fill(this->mInputHistory[c].begin(), this->mInputHistory[c].end(), 0.0);
      std::fill(this->mOutputHistory[c].begin(), this->mOutputHistory[c].end(), 0.0);
    }
  }
}

void recursive_linear_filter::Biquad::_AssignCoefficients(const double a0,
                                                          const double a1,
                                                          const double a2,
                                                          const double b0,
                                                          const double b1,
                                                          const double b2)
{
  this->mInputCoefficients[0] = b0 / a0;
  this->mInputCoefficients[1] = b1 / a0;
  this->mInputCoefficients[2] = b2 / a0;
  // this->mOutputCoefficients[0] = 0.0;  // Always
  // Sign flip due so we add during main loop (cf Eq. (4))
  this->mOutputCoefficients[1] = -a1 / a0;
  this->mOutputCoefficients[2] = -a2 / a0;
}

void recursive_linear_filter::LowShelf::SetParams(const recursive_linear_filter::BiquadParams &params)
{
  const double a = params.GetA();
  const double omega_0 = params.GetOmega0();
  const double alpha = params.GetAlpha(omega_0);
  const double cosw = params.GetCosW(omega_0);
  
  const double ap = a + 1.0;
  const double am = a - 1.0;
  const double roota2alpha = 2.0 * sqrt(a) * alpha;
  
  const double b0 = a * (ap - am * cosw + roota2alpha);
  const double b1 = 2.0 * a * (am - ap * cosw);
  const double b2 = a * (ap - am * cosw - roota2alpha);
  const double a0 = ap + am * cosw + roota2alpha;
  const double a1 = -2.0 * (am + ap * cosw);
  const double a2 = ap + am * cosw - roota2alpha;
  
  this->_AssignCoefficients(a0, a1, a2, b0, b1, b2);
}

void recursive_linear_filter::Peaking::SetParams(const recursive_linear_filter::BiquadParams &params)
{
  const double a = params.GetA();
  const double omega_0 = params.GetOmega0();
  const double alpha = params.GetAlpha(omega_0);
  const double cosw = params.GetCosW(omega_0);
  
  const double b0 = 1.0 + alpha * a;
  const double b1 = -2.0 * cosw;
  const double b2 = 1.0 - alpha * a;
  const double a0 = 1.0 + alpha / a;
  const double a1 = -2.0 * cosw;
  const double a2 = 1.0 - alpha / a;
  
  this->_AssignCoefficients(a0, a1, a2, b0, b1, b2);
}

void recursive_linear_filter::HighShelf::SetParams(const recursive_linear_filter::BiquadParams &params)
{
  const double a = params.GetA();
  const double omega_0 = params.GetOmega0();
  const double alpha = params.GetAlpha(omega_0);
  const double cosw = params.GetCosW(omega_0);
  
  const double roota2alpha = 2.0 * sqrt(a) * alpha;
  const double ap = a + 1.0;
  const double am = a - 1.0;
  
  const double b0 = a * (ap + am * cosw + roota2alpha);
  const double b1 = -2.0 * a * (am + ap * cosw);
  const double b2 = a * (ap + am * cosw - roota2alpha);
  const double a0 = ap - am * cosw + roota2alpha;
  const double a1 = 2.0 * (am - ap * cosw);
  const double a2 = ap - am * cosw - roota2alpha;
  
  this->_AssignCoefficients(a0, a1, a2, b0, b1, b2);
}
