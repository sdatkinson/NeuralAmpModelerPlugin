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
    // Return cases
    const int RET_SUCCESS = 0;
    const int RET_ERROR_OPENING = 1;
    const int RET_ERROR_NOT_WAV = 2;
    const int RET_ERROR_INVALID_WAV = 3;
    const int RET_ERROR_NOT_PCM = 4;
    // Load a WAV file into a provided array of doubles,
    // And note the sample rate.
    //
    // Returns: as per return cases above
    int Load(const WDL_String& fileName, std::vector<float> &audio, double& sampleRate);
    
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
