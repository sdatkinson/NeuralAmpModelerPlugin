//
//  RecursiveLinearFilter.cpp
//  
//
//  Created by Steven Atkinson on 12/28/22.
//
// See: https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html

#include <algorithm>  // std::fill
#include <cmath>  // pow, sin

#include "RecursiveLinearFilter.h"

recursive_linear_filter::Base::Base(const size_t inputDegree,
                                    const size_t outputDegree) :
mOutputPointers(nullptr),
mOutputPointersSize(0),
mInputStart(inputDegree),  // 1 is subtracted before first use
mOutputStart(outputDegree)
{
  this->mInputCoefficients.resize(inputDegree);
  this->mOutputCoefficients.resize(outputDegree);
}

iplug::sample** recursive_linear_filter::Base::Process(iplug::sample** inputs,
                                                       const int numChannels,
                                                       const int numFrames,
                                                       const Params *params)
{
  this->_PrepareBuffers(numChannels, numFrames);
  params->SetCoefficients(this->mInputCoefficients, this->mOutputCoefficients);
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
  return this->GetPointers();
}

iplug::sample** recursive_linear_filter::Base::GetPointers()
{
  for (auto c=0; c<this->_GetNumChannels(); c++)
    this->mOutputPointers[c] = this->mOutputs[c].data();
  return this->mOutputPointers;
}


void recursive_linear_filter::Base::_PrepareBuffers(const int numChannels, const int numFrames)
{
  if (this->_GetNumChannels() != numChannels) {
    this->mInputHistory.resize(numChannels);
    this->mOutputHistory.resize(numChannels);
    this->mOutputs.resize(numChannels);
    const size_t inputDegree = this->_GetInputDegree();
    const size_t outputDegree = this->_GetOutputDegree();
    for (auto c=0; c<numChannels; c++) {
      this->mInputHistory[c].resize(inputDegree);
      this->mOutputHistory[c].resize(outputDegree);
      this->mOutputs[c].resize(numFrames);
      std::fill(this->mInputHistory[c].begin(), this->mInputHistory[c].end(), 0.0);
      std::fill(this->mOutputHistory[c].begin(), this->mOutputHistory[c].end(), 0.0);
    }
    this->_ResizePointers((size_t) numChannels);
  }
}

void recursive_linear_filter::Base::_ResizePointers(const size_t numChannels)
{
  if (this->mOutputPointersSize == numChannels)
    return;
  if (this->mOutputPointers != nullptr)
    delete[] this->mOutputPointers;
  this->mOutputPointers = new iplug::sample*[numChannels];
  if (this->mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
  this->mOutputPointersSize = numChannels;
}

void recursive_linear_filter::LowShelfParams::SetCoefficients(std::vector<double> &inputCoefficients,
                                                              std::vector<double> &outputCoefficients) const
{
  const double a = pow(10.0, this->mGainDB / 40.0);
  const double omega_0 = 2.0 * MATH_PI * this->mFrequency / this->mSampleRate;
  const double alpha = sin(omega_0) / (2.0 * this->mQuality);
  const double cosw = cos(omega_0);
  
  const double ap = a + 1.0;
  const double am = a - 1.0;
  const double roota2alpha = 2.0 * sqrt(a) * alpha;
  
  const double b0 = a * (ap - am * cosw + roota2alpha);
  const double b1 = 2.0 * a * (am - ap * cosw);
  const double b2 = a * (ap - am * cosw - roota2alpha);
  const double a0 = ap + am * cosw + roota2alpha;
  const double a1 = -2.0 * (am + ap * cosw);
  const double a2 = ap + am * cosw - roota2alpha;
  
  inputCoefficients[0] = b0 / a0;
  inputCoefficients[1] = b1 / a0;
  inputCoefficients[2] = b2 / a0;
  // outputCoefficients[0] = 0.0;  // Always
  // Sign flip due so we add during main loop (cf Eq. (4))
  outputCoefficients[1] = -a1 / a0;
  outputCoefficients[2] = -a2 / a0;
}

void recursive_linear_filter::PeakingParams::SetCoefficients(std::vector<double> &inputCoefficients,
                                                             std::vector<double> &outputCoefficients) const
{
  const double a = pow(10.0, this->mGainDB / 40.0);
  const double omega_0 = 2.0 * MATH_PI * this->mFrequency / this->mSampleRate;
  const double alpha = sin(omega_0) / (2.0 * this->mQuality);
  const double cosw = cos(omega_0);
  
  const double b0 = 1.0 + alpha * a;
  const double b1 = -2.0 * cosw;
  const double b2 = 1.0 - alpha * a;
  const double a0 = 1.0 + alpha / a;
  const double a1 = -2.0 * cosw;
  const double a2 = 1.0 - alpha / a;
  
  inputCoefficients[0] = b0 / a0;
  inputCoefficients[1] = b1 / a0;
  inputCoefficients[2] = b2 / a0;
  // outputCoefficients[0] = 0.0;  // Always
  // Sign flip due so we add during main loop (cf Eq. (4))
  outputCoefficients[1] = -a1 / a0;
  outputCoefficients[2] = -a2 / a0;
}

void recursive_linear_filter::HighShelfParams::SetCoefficients(std::vector<double> &inputCoefficients,
                                                               std::vector<double> &outputCoefficients) const
{
  const double a = pow(10.0, this->mGainDB / 40.0);
  const double omega_0 = 2.0 * MATH_PI * this->mFrequency / this->mSampleRate;
  const double alpha = sin(omega_0) / (2.0 * this->mQuality);
  const double cosw = cos(omega_0);
  const double roota2alpha = 2.0 * sqrt(a) * alpha;
  
  const double ap = a + 1.0;
  const double am = a - 1.0;
  
  const double b0 = a * (ap + am * cosw + roota2alpha);
  const double b1 = -2.0 * a * (am + ap * cosw);
  const double b2 = a * (ap + am * cosw - roota2alpha);
  const double a0 = ap - am * cosw + roota2alpha;
  const double a1 = 2.0 * (am - ap * cosw);
  const double a2 = ap - am * cosw - roota2alpha;
  
  
  inputCoefficients[0] = b0 / a0;
  inputCoefficients[1] = b1 / a0;
  inputCoefficients[2] = b2 / a0;
  // outputCoefficients[0] = 0.0;  // Always
  // Sign flip due so we add during main loop (cf Eq. (4))
  outputCoefficients[1] = -a1 / a0;
  outputCoefficients[2] = -a2 / a0;
}
