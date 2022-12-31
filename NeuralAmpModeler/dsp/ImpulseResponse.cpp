//
//  ImpulseResponse.cpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/30/22.
//

#include "ImpulseResponse.h"

iplug::sample** dsp::ImpulseResponse::Process(iplug::sample** inputs,
                                              const size_t numChannels,
                                              const size_t numFrames)
{
  this->_PrepareBuffers(numChannels, numFrames);
  if (this->_HaveIR())
    // TODO real implementation!
    this->_FallbackProcess(inputs, numChannels, numFrames);
  else
    this->_Process(inputs, numChannels, numFrames);
  return this->_GetPointers();
}

void dsp::ImpulseResponse::SetParams(const dsp::ImpulseResponseParams &params)
{
  // TODO
  // Compare the filepath and see if anything needs to be done.
  // Load in the raw audio.
  // Resample.
  // Truncate to max length.
  // Set the history buffers to the needed length.
}

void dsp::ImpulseResponse::_FallbackProcess(iplug::sample** inputs,
                                            const size_t numChannels,
                                            const size_t numFrames)
{
  for (auto c=0; c<numChannels; c++)
    for (auto s=0; s<numFrames; s++)
      this->mOutputs[c][s] = inputs[c][s];
}

void dsp::ImpulseResponse::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  this->dsp::DSP::_PrepareBuffers(numChannels, numFrames);
  // TODO  the long buffers for the history
}

void dsp::ImpulseResponse::_Process(iplug::sample** inputs,
                                    const size_t numChannels,
                                    const size_t numFrames)
{
  // TODO
  this->_FallbackProcess(inputs, numChannels, numFrames);
}
