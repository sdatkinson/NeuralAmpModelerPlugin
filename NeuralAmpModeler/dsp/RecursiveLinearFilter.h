//
//  RecursiveLinearFilter.h
//  
//
//  Created by Steven Atkinson on 12/28/22.
//
// Recursive linear filters (LPF, HPF, Peaking, Shelving)

#ifndef RecursiveLinearFilter_h
#define RecursiveLinearFilter_h

#include <vector>
#include "IPlugConstants.h"  // sample

#define MATH_PI 3.14159265358979323846

// TODO refactor base DSP into a common abstraction.

namespace recursive_linear_filter {
  class Params{
  public:
    // Based on the parameters in this object, set the coefficients in the
    // provided arrays for the filter.
    virtual void SetCoefficients(std::vector<double>& inputCoefficients,
                                 std::vector<double>& outputCoefficients) const=0;
  };
  
  class Base {  // TODO: inherit from DSP
  public:
    Base(const size_t inputDegree, const size_t outputDegree);
    
    // Compute the DSP, saving to mOutputs, owned by this instance.
    // Return a pointer-to-pointers of the output array
    // TODO return const
    iplug::sample** Process(iplug::sample** inputs,
                            const int numChannels,
                            const int numFrames,
                            const Params *params);
    
    // Return a pointer-to-pointers for the DSP's output buffers (all channels)
    // Assumes that ._PrepareBuffers()  was called recently enough.
    iplug::sample** GetPointers();
  protected:
    size_t _GetNumChannels() const {return this->mInputHistory.size();};
    size_t _GetInputDegree() const {return this->mInputCoefficients.size();};
    size_t _GetOutputDegree() const {return this->mOutputCoefficients.size();};
    void _PrepareBuffers(const int numChannels, const int numFrames);
    // Resizes the pointer-to-pointers for the vector-of-vectors.
    void _ResizePointers(const size_t numChannels);
    
    // Where the output will get written...
    // Indexing is [channel][sample]
    std::vector<std::vector<iplug::sample>> mOutputs;
    // ...And a pointer to it.
    // Only the first level is allocated; the second level are duplicate
    // pointers to the data that are primarily owned by the vectors.
    iplug::sample** mOutputPointers;
    size_t mOutputPointersSize;  // Keep track of its size.

    // Coefficients for the DSP filter
    std::vector<double> mInputCoefficients;
    std::vector<double> mOutputCoefficients;

    // Arrays holding the history on which the filter depends recursively.
    // First index is channel
    // Second index, [0] is the current input/output, [1] is the previous, [2] is before that, etc.
    std::vector<std::vector<iplug::sample>> mInputHistory;
    std::vector<std::vector<iplug::sample>> mOutputHistory;
    // Indices for history.
    // Designates which index is currently "0". Use modulus to wrap around.
    long mInputStart;
    long mOutputStart;
  };
  
  class LevelParams : public Params {
  public:
    LevelParams(const double gain) : Params(), mGain(gain) {};
    void SetCoefficients(std::vector<double>& inputCoefficients,
                         std::vector<double>& outputCoefficients) const {
      inputCoefficients[0] = this->mGain;
    };
  private:
    // The gain (multiplicative, i.e. not dB)
    double mGain;
  };
  
  class Level : public Base {
  public:
    Level() : Base(1, 0) {};
  };
  
  class LowShelfParams : public Params {
  public:
    LowShelfParams(const double sampleRate, const double frequency, const double quality, const double gainDB) :
    Params(),
    mSampleRate(sampleRate),
    mFrequency(frequency),
    mQuality(quality),
    mGainDB(gainDB) {};
    
    void SetCoefficients(std::vector<double>& inputCoefficients,
                         std::vector<double>& outputCoefficients) const;
  private:
    double mSampleRate;
    double mFrequency;
    double mQuality;
    double mGainDB;
  };
  
  class LowShelf : public Base {
  public:
    LowShelf() : Base(3,3) {};
  };
  
  class PeakingParams : public Params {
  public:
    PeakingParams(const double sampleRate, const double frequency, const double quality, const double gainDB) :
    Params(),
    mSampleRate(sampleRate),
    mFrequency(frequency),
    mQuality(quality),
    mGainDB(gainDB) {};
    
    void SetCoefficients(std::vector<double>& inputCoefficients,
                         std::vector<double>& outputCoefficients) const;
  private:
    double mSampleRate;
    double mFrequency;
    double mQuality;
    double mGainDB;
  };
  
  class Peaking : public Base {
  public:
    Peaking() : Base(3,3) {};
  };
  
  class HighShelfParams : public Params {
  public:
    HighShelfParams(const double sampleRate, const double frequency, const double quality, const double gainDB) :
    Params(),
    mSampleRate(sampleRate),
    mFrequency(frequency),
    mQuality(quality),
    mGainDB(gainDB) {};
    
    void SetCoefficients(std::vector<double>& inputCoefficients,
                         std::vector<double>& outputCoefficients) const;
  private:
    double mSampleRate;
    double mFrequency;
    double mQuality;
    double mGainDB;
  };
  
  class HighShelf : public Base {
  public:
    HighShelf() : Base(3,3) {};
  };
};

#endif /* RecursiveLinearFilter_h */
