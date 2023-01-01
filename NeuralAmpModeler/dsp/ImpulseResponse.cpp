//
//  ImpulseResponse.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/30/22.
//

#include "ImpulseResponse.h"
#include "wav.h"

dsp::ImpulseResponse::ImpulseResponse(const WDL_String& fileName,
                                      const double sampleRate)
{
  // Try to load the WAV
  dsp::wav::Load(fileName, this->mRawAudio, this->mRawAudioSampleRate);
  // Set the weights based on the raw audio.
  this->_SetWeights(sampleRate);
}

iplug::sample** dsp::ImpulseResponse::Process(iplug::sample** inputs,
                                              const size_t numChannels,
                                              const size_t numFrames)
{
  this->_PrepareBuffers(numChannels, numFrames);
  this->_UpdateHistory(inputs, numChannels, numFrames);
  // TODO real implementation
//  for (auto c=0; c<numChannels; c++)
//    for (auto s=0; s<numFrames; s++)
//      this->mOutputs[c][s] = inputs[c][s];
  
  for (size_t i=0, j=this->mHistoryIndex - this->mHistoryRequired; i<numFrames; i++, j++) {
    auto input = Eigen::Map<const Eigen::VectorXf>(&this->mHistory[j], this->mHistoryRequired+1);
    this->mOutputs[0][i] = (double) this->mWeight.dot(input);
  }
  // Copy out for more-than-mono.
  for (size_t c=1; c<numChannels; c++)
    for (size_t i=0; i<numFrames; i++)
      this->mOutputs[c][i] = this->mOutputs[0][i];
  
  this->_AdvanceHistoryIndex(numFrames);
  return this->_GetPointers();
}

void dsp::ImpulseResponse::_SetWeights(const double sampleRate)
{
  if (this->mRawAudioSampleRate != sampleRate) {
    std::stringstream ss;
    ss << "IR is at sample rate " << this->mRawAudioSampleRate
    << ", but the plugin is running at " << sampleRate << std::endl;
    throw std::runtime_error(ss.str());
  }
  // Simple implementation w/ no resample...
  const size_t irLength = std::min(this->mRawAudio.size(), this->mMaxLength);
  this->mWeight.resize(irLength);
  for (size_t i=0, j=irLength-1; i<irLength; i++,j--)
    this->mWeight[j] = this->mRawAudio[i];
  this->mHistoryRequired = irLength - 1;
}
