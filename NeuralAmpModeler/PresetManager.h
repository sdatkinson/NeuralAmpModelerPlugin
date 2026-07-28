#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nam_presets
{

// Raw preset data. Agnostic of the plugin's param enum:
// the caller (NeuralAmpModeler.cpp) maps kX <-> "paramName" when building/reading this.
struct PresetData
{
  std::string name;
  std::string modelPath; // .nam file, absolute
  std::string irPath; // .wav file, absolute, may be empty (no IR)
  std::map<std::string, double> paramValues; // "InputLevel" -> 0.5, etc.
};

enum class PresetError
{
  kNone,
  kFileNotFound,
  kParseError, // corrupt/invalid JSON
  kDirCreateFailed,
  kWriteFailed,
  kDuplicateName,
  kNotFound // preset requested by name doesn't exist
};

class PresetManager
{
public:
  // basePath: ~/Library/Application Support/NeuralAmpModeler (via AppSupportPath).
  // Injected, not hardcoded, so it can be tested with a temp dir.
  explicit PresetManager(std::string basePath);

  // Creates base directory + presets.json if they don't exist. Idempotent.
  PresetError Initialize();

  // Reads the whole store into memory (lazy, first call triggers Initialize()).
  PresetError Load();

  // Writes current in-memory state to disk (atomic write: tmp file + rename).
  PresetError Save() const;

  // In-memory CRUD. Doesn't touch disk until the next Save().
  // Rejects if preset.name already exists (no overwrite) -> kDuplicateName.
  // To replace an existing preset: Remove(name) + Add(preset).
  PresetError Add(const PresetData& preset);
  PresetError Remove(const std::string& name);
  std::optional<PresetData> Get(const std::string& name) const;
  std::vector<std::string> ListNames() const;

private:
  std::string mBasePath;
  std::string mPresetsFilePath; // mBasePath + "/presets.json"
  std::map<std::string, PresetData> mPresets;
};

} // namespace nam_presets
