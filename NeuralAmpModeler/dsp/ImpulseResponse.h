//
//  ImpulseResponse.hpp
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/30/22.
//
// Impulse response processing

#ifndef ImpulseResponse_hpp
#define ImpulseResponse_hpp

#include <filesystem>
#include "IPlugConstants.h"  // sample
#include "dsp.h"

namespace dsp {
  class ImpulseResponseParams : public Params {
    std::filesystem::path GetFilePath() const {return this->mFilePath;};
    double GetSampleRate() const {return this->mSampleRate;};
  private:
    std::filesystem::path mFilePath;
    // Sample rate of the plugin
    double mSampleRate;
  };
  
  class ImpulseResponse : public DSP {
  public:
    iplug::sample** Process(iplug::sample** inputs,
                            const size_t numChannels,
                            const size_t numFrames) override;
    void SetParams(const ImpulseResponseParams &params);
  private:
    bool _HaveIR() const {return false;};  // TODO
    void _FallbackProcess(iplug::sample** inputs,
                          const size_t numChannels,
                          const size_t numFrames);
    void _PrepareBuffers(const size_t numChannels, const size_t numFrames) override;
    void _Process(iplug::sample** inputs,
                  const size_t numChannels,
                  const size_t numFrames);
    // Keep a copy of the raw audio that was loaded so that it can be resampled
    std::vector<double> mRawAudio;
    double mRawAudioSampleRate;
  };
};

#endif /* ImpulseResponse_hpp */
