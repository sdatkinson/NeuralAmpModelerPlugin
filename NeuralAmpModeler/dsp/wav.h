//
//  wav.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 12/31/22.
//

#ifndef wav_h
#define wav_h

#include "wdlstring.h"  // WDL_String

namespace dsp {
  namespace wav {
    enum class LoadReturnCode {
        SUCCESS = 0,
        ERROR_OPENING,
        ERROR_NOT_RIFF,
        ERROR_NOT_WAVE,
        ERROR_MISSING_FMT,
        ERROR_INVALID_FILE,
        ERROR_UNSUPPORTED_FORMAT_IEEE_FLOAT,
        ERROR_UNSUPPORTED_FORMAT_ALAW,
        ERROR_UNSUPPORTED_FORMAT_MULAW,
        ERROR_UNSUPPORTED_FORMAT_EXTENSIBLE,
        ERROR_UNSUPPORTED_BITS_PER_SAMPLE,
        ERROR_NOT_MONO,
        ERROR_OTHER
    };
    // Load a WAV file into a provided array of doubles,
    // And note the sample rate.
    //
    // Returns: as per return cases above
    LoadReturnCode Load(const WDL_String& fileName, std::vector<float> &audio, double& sampleRate);
    
    // Load samples, 16-bit
    void _LoadSamples16(std::ifstream &wavFile, const int chunkSize, std::vector<float>& samples);
    // Load samples, 24-bit
    void _LoadSamples24(std::ifstream &wavFile, const int chunkSize, std::vector<float>& samples);
    // Load samples, 32-bit
    void _LoadSamples32(std::ifstream &wavFile, const int chunkSize, std::vector<float>& samples);
    
    // Read in a 24-bit sample and convert it to an int
    int _ReadSigned24BitInt(std::ifstream& stream);
  };
};

#endif /* wav_h */
