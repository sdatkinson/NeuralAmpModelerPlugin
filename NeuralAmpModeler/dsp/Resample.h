//
//  Resample.h
//  NeuralAmpModeler-macOS
//
//  Created by Steven Atkinson on 1/2/23.
//

#ifndef Resample_h
#define Resample_h

#include <cmath>
#include <stdexcept>
#include <vector>

namespace dsp {
// Resample a provided vector in inputs to outputs.
// Creates an array of the required length to fill all points from the SECOND
// input to the SECOND-TO-LAST input point, exclusive.
// (Seconds bc cubic. and ew want to only interpoalte between points 2 and
// 3.)
// tOutputStart: location of first output point relative to the second input
// point (should be >=0.0)
template <typename T>
void ResampleCubic(const std::vector<T> &inputs,
                   const double originalSampleRate,
                   const double desiredSampleRate, const double tOutputStart,
                   std::vector<T> &outputs);
// Interpolate the 4 provided equispaced points to x in [-1,2]
template <typename T> T _CubicInterpolation(T p[4], T x) {
  return p[1] + 0.5 * x *
                    (p[2] - p[0] +
                     x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] +
                          x * (3.0 * (p[1] - p[2]) + p[3] - p[0])));
};
}; // namespace dsp

template <typename T>
void dsp::ResampleCubic(const std::vector<T> &inputs,
                        const double originalSampleRate,
                        const double desiredSampleRate,
                        const double tOutputStart, std::vector<T> &outputs) {
  if (tOutputStart < 0.0)
    throw std::runtime_error("Starting time must be non-negative");

  // Time increment for each sample in the original audio file
  const double timeIncrement = 1.0 / originalSampleRate;

  // Time increment for each sample in the resampled audio file
  const double resampledTimeIncrement = 1.0 / desiredSampleRate;

  // Current time
  double time = timeIncrement + tOutputStart;

  const double endTimeOriginal = (inputs.size() - 1) * timeIncrement;
  while (time < endTimeOriginal) {
    // Find the index of the sample in the original audio file that is just
    // before the current time in the resampled audio file
    int index = (long)std::floor(time / timeIncrement);

    // Calculate the time difference between the current time in the resampled
    // audio file and the sample in the original audio file
    double timeDifference = time - index * timeIncrement;

    // Get the four surrounding samples in the original audio file for cubic
    // interpolation
    double p[4];
    p[0] = (index == 0) ? inputs[0] : inputs[index - 1];
    p[1] = inputs[index];
    p[2] = (index == inputs.size() - 1) ? inputs[inputs.size() - 1]
                                        : inputs[index + 1];
    p[3] = (index == inputs.size() - 2) ? inputs[inputs.size() - 1]
                                        : inputs[index + 2];

    // Use cubic interpolation to estimate the value of the audio signal at the
    // current time in the resampled audio file
    T resampledValue =
        dsp::_CubicInterpolation(p, timeDifference / timeIncrement);

    // Add the estimated value to the resampled audio file
    outputs.push_back(resampledValue);

    // Update the current time in the resampled audio file
    time += resampledTimeIncrement;
  }
}

#endif /* Resample_h */
