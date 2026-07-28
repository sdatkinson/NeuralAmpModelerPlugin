#include "PresetManager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

#include "json.hpp"

namespace nam_presets
{

namespace
{
namespace fs = std::filesystem;

nlohmann::json PresetToJson(const PresetData& preset)
{
  nlohmann::json j;
  j["modelPath"] = preset.modelPath;
  j["irPath"] = preset.irPath;
  j["params"] = preset.paramValues;
  return j;
}

// false if the node isn't shaped as expected (that preset is skipped on load, doesn't abort the whole file).
bool PresetFromJson(const std::string& name, const nlohmann::json& j, PresetData& outPreset)
{
  if (!j.is_object())
    return false;

  outPreset.name = name;
  outPreset.modelPath = j.value("modelPath", "");
  outPreset.irPath = j.value("irPath", "");
  outPreset.paramValues.clear();

  if (j.contains("params") && j.at("params").is_object())
  {
    for (auto it = j.at("params").begin(); it != j.at("params").end(); ++it)
    {
      if (it.value().is_number())
        outPreset.paramValues[it.key()] = it.value().get<double>();
    }
  }
  return true;
}
} // namespace

PresetManager::PresetManager(std::string basePath)
: mBasePath(std::move(basePath))
, mPresetsFilePath(mBasePath + "/presets.json")
{
}

PresetError PresetManager::Initialize()
{
  std::error_code ec;
  fs::create_directories(mBasePath, ec);
  if (ec)
    return PresetError::kDirCreateFailed;

  if (!fs::exists(mPresetsFilePath, ec))
  {
    std::ofstream out(mPresetsFilePath, std::ios::trunc);
    if (!out.is_open())
      return PresetError::kWriteFailed;
    out << "{}";
    if (!out.good())
      return PresetError::kWriteFailed;
  }
  return PresetError::kNone;
}

PresetError PresetManager::Load()
{
  const PresetError initErr = Initialize();
  if (initErr != PresetError::kNone)
    return initErr;

  std::ifstream in(mPresetsFilePath);
  if (!in.is_open())
    return PresetError::kFileNotFound;

  std::stringstream buffer;
  buffer << in.rdbuf();

  nlohmann::json root;
  try
  {
    root = nlohmann::json::parse(buffer.str());
  }
  catch (const nlohmann::json::exception&)
  {
    return PresetError::kParseError;
  }

  if (!root.is_object())
    return PresetError::kParseError;

  std::map<std::string, PresetData> loaded;
  for (auto it = root.begin(); it != root.end(); ++it)
  {
    PresetData preset;
    if (PresetFromJson(it.key(), it.value(), preset))
      loaded[it.key()] = std::move(preset);
  }

  mPresets = std::move(loaded);
  return PresetError::kNone;
}

PresetError PresetManager::Save() const
{
  nlohmann::json root = nlohmann::json::object();
  for (const auto& [name, preset] : mPresets)
    root[name] = PresetToJson(preset);

  // Atomic write: write to a tmp file and rename it over the real file,
  // so a crash mid-write never leaves presets.json corrupted.
  const std::string tmpPath = mPresetsFilePath + ".tmp";
  {
    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out.is_open())
      return PresetError::kWriteFailed;
    out << root.dump(2);
    if (!out.good())
      return PresetError::kWriteFailed;
  }

  std::error_code ec;
  fs::rename(tmpPath, mPresetsFilePath, ec);
  if (ec)
    return PresetError::kWriteFailed;

  return PresetError::kNone;
}

PresetError PresetManager::Add(const PresetData& preset)
{
  if (mPresets.find(preset.name) != mPresets.end())
    return PresetError::kDuplicateName;

  mPresets[preset.name] = preset;
  return PresetError::kNone;
}

PresetError PresetManager::Remove(const std::string& name)
{
  const auto it = mPresets.find(name);
  if (it == mPresets.end())
    return PresetError::kNotFound;

  mPresets.erase(it);
  return PresetError::kNone;
}

std::optional<PresetData> PresetManager::Get(const std::string& name) const
{
  const auto it = mPresets.find(name);
  if (it == mPresets.end())
    return std::nullopt;
  return it->second;
}

std::vector<std::string> PresetManager::ListNames() const
{
  std::vector<std::string> names;
  names.reserve(mPresets.size());
  for (const auto& [name, preset] : mPresets)
    names.push_back(name);
  return names;
}

} // namespace nam_presets
