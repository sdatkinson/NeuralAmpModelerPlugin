#pragma once

#include <filesystem>
#include <vector>

namespace numpy_util {
  // Get data from a file to a flat array.
  // Don't worry; I flattened everythign when exporting from Python :)
  std::vector<float> load_to_vector(std::filesystem::path path);
};