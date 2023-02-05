#pragma once

#include <cmath> // expf

namespace activations {
float relu(float x) { return x > 0.0f ? x : 0.0f; };
float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); };
}; // namespace activations