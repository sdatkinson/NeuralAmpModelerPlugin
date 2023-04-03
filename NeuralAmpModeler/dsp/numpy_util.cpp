#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "cnpy.h"
#include "numpy_util.h"

std::vector<float> numpy_util::load_to_vector(fs::path path) {
  cnpy::NpyArray x = cnpy::npy_load(path.string());
  if (x.shape.size() != 1)
    throw std::runtime_error("Expected 1D array.\n");
  return x.as_vec<float>();
}
