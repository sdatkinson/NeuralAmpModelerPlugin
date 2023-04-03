#pragma once

#include <filesystem>
#include <vector>
#include "ghc/fs_std_fwd.hpp"

namespace numpy_util {
// Get data from a file to a flat array.
// Don't worry; I flattened everythign when exporting from Python :)
std::vector<float> load_to_vector(fs::path path);
}; // namespace numpy_util