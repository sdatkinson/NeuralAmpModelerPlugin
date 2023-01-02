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

#include <Eigen/Dense>

#include "wdlstring.h"  // WDL_String
#include "IPlugConstants.h"  // sample
#include "dsp.h"

namespace dsp {
  class ImpulseResponse : public History {
  public:
    ImpulseResponse(const WDL_String& fileName, const double sampleRate);
    iplug::sample** Process(iplug::sample** inputs,
                            const size_t numChannels,
                            const size_t numFrames) override;
  private:
    // Set the weights, given that the plugin is running at the provided sample rate.
    void _SetWeights(const double sampleRate);
    
    // Keep a copy of the raw audio that was loaded so that it can be resampled
    std::vector<float> mRawAudio;
    double mRawAudioSampleRate;
    // Resampled to the required sample rate.
    std::vector<float> mResampled;
    
    const size_t mMaxLength = 8192;
    // The weights
    Eigen::VectorXf mWeight;
  };
};

#endif /* ImpulseResponse_hpp */
