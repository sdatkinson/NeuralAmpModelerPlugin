//
//  Resample.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 1/2/23.
//

#ifndef Resample_h
#define Resample_h

#include <vector>
#include <stdexcept>

namespace dsp {
  // Resample a provided vector in inputs to outputs.
  // Creates an array of the required length to fill all points from the SECOND
  // input to the SECOND-TO-LAST input point and no further.
  // (Seconds bc cubic. and ew want to only interpoalte between points 2 and
  // 3.)
  // tOutputStart: location of first output point relative to the second input
  // point (should be >=0.0)
  template<typename T>
  void ResampleCubic(std::vector<T>& inputs,
                     std::vector<T>& outputs,
                     const double sampleRateInputs,
                     const double sampleRateOutputs,
                     const double tOutputStart);
};

template <typename T>
void dsp::ResampleCubic(std::vector<T>& inputs,
                        std::vector<T>& outputs,
                        const double sampleRateInputs,
                        const double sampleRateOutputs,
                        const double tOutputStart)
{
  if (sampleRateInputs != sampleRateOutputs)
    throw std::runtime_error("Not implemented yet!");
  // TODO real implementation
  for (auto i=1; i< inputs.size()-1; i++)
    outputs.push_back(inputs[i]);
}

#endif /* Resample_h */
