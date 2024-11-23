
#include <ifstream>
#include <string>
#include <vector>

#include "json.hpp"

#include "NeuralAmpModeler.h"

// Unserialization
//
// This plugin is used in important places, so we need to be considerate when
// attempting to unserialize. If the project was last saved with a legacy
// version, then we need it to "update" to the current version is as
// reasonable a way as possible.
//
// In order to handle older versions, the pattern is:
// 1. Implement unserialization for every version into a version-specific
//    struct (Let's use our friend nlohmann::json. Why not?)
// 2. Implement an "update" from each struct to the next one.
// 3. Implement assigning the data contained in the current struct to the
//    current plugin configuration.
//
// This way, a constant amount of effort is required every time the
// serialization changes instead of having to implement a current
// unserialization for each past version.

// Add new unserialization versions to the top, then add logic to the class method at the bottom.

// Boilerplate

void NeuralAmpModeler::_UnserializeApplyConfig(nlohmann::json& config)
{
  // cf IPluginBase::UnserializeParams(const IByteChunk& chunk, int startPos)

  nlohmann::json config;

  auto getParamByName = [&, newNames](std::string& name) {
    // Could use a map but eh
    for (int i = 0; i < kNumParams; i++)
    {
      IParam* param = GetParam(i);
      if (strcmp(param->GetName(), name.c_str()) == 0)
      {
        return param;
      }
    }
    // else
    return (IParam*)nullptr;
  };
  TRACE
  ENTER_PARAMS_MUTEX
  int i = 0;
  for (auto it = config.begin(); it != config.end(); ++it, i++)
  {
    IParam* pParam = getParamByName(it->first);
    if (pParam != nullptr)
    {
      pParam->Set(v);
      Trace(TRACELOC, "%d %s %f", i, pParam->GetName(), pParam->Value());
    }
    else
    {
      Trace(TRACELOC, "%s NOT-FOUND", it->first.c_str());
    }
  }
  OnParamReset(kPresetRecall);
  LEAVE_PARAMS_MUTEX

  mNAMPath.Set(config["NAMPath"].c_str());
  mIRPath.Set(config["IRPath"].c_str());

  if (mNAMPath.GetLength())
  {
    _StageModel(mNAMPath);
  }
  if (mIRPath.GetLength())
  {
    _StageIR(mIRPath);
  }
}

// Unserialize NAM Path, IR path, then named keys
int _UnserializePathsAndExpectedKeys(const IByteChunk& chunk, inst startPos, nlohmann::json& config,
                                     std::vector<std::string>& paramNames)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());

  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    config[*it] = v;
  }
  return pos;
}

void _RenameKeys(nlohmann::json& j, std::unordered_map<std::string, std::string> newNames)
{
  // Assumes no aliasing!
  for (it = newNames.begin(); it != newNames.end(); ++it)
  {
    config[it->first] = config[it->second];
    config.erase(it->first);
  }
}

// v0.7.12

void _UpdateConfigFrom_0_7_12(nlohmann::json& config)
{
  // Fill me in once something changes!
}

int _GetConfigFrom_0_7_12(const IByteChunk& chunk, int pos, nlohmann::json& config)
{
  int pos = startPos;
  nlohmann::json config;
  std::vector<std::string> paramNames{"Input",  "Threshold",       "Bass",      "Middle",     "Treble",
                                      "Output", "NoiseGateActive", "ToneStack", "OutputMode", "IRToggle"};

  pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
  // Then update:
  _UnserializeUpdateFrom_0_7_12(config);
  _UnserializeFromJson(config);
  return pos;
}

// 0.7.10

void _UpdateConfigFrom_0_7_10(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> newNames{{"OutNorm", "OutputMode"}};
  _RenameKeys(config, newNames);
  _UnserializeUpdateFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_10(const IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = startPos;
  nlohmann::json config;
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};

  pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
  // Then update:
  _UnserializeUpdateFrom_0_7_10(config);
  _UnserializeFromJson(config);
  return pos;
}

// Earlier than 0.7.10 (Assumed to be 0.7.9)

void _UpdateConfigFrom_Earlier(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> newNames{{"Gate", "Threshold"}};
  _RenameKeys(config, newNames);
  _UpdateConfigFrom_0_7_10(config);
}

int _GetConfigFrom_Earlier(const IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = startPos;
  std::vector<std::string> paramNames{
    "Input", "Gate", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};

  pos = _UnserializePathsAndExpectedKeys(chunk, pos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_Earlier(config);
  _UnserializeFromJson(config);
  return pos;
}

//==============================================================================

class _Version
{
public:
  _Version(const int major, const int minor, const int patch)
  : mMajor(major)
  , mMinor(minor)
  , mPatch(patch) {};
  _Version(const std::string& versionStr)
  {
    std::istringstream stream(versionStr);
    std::string token;
    std::vector<int> parts;

    // Split the string by "."
    while (std::getline(stream, token, '.'))
    {
      parts.push_back(std::stoi(token)); // Convert to int and store
    }

    // Check if we have exactly 3 parts
    if (parts.size() != 3)
    {
      throw std::invalid_argument("Input string does not contain exactly 3 segments separated by '.'");
    }

    // Assign the parts to the provided int variables
    mMajor = parts[0];
    mMinor = parts[1];
    mPatch = parts[2];
  };

  bool operator<=(const _Version& other) const
  {
    // Compare on major version:
    if (other.GetMajor() > GetMajor())
    {
      return true;
    }
    if (other.GetMajor() < GetMajor())
    {
      return false;
    }
    // Compare on minor
    if (other.GetMinor() > GetMinor())
    {
      return true;
    }
    if (other.GetMinor() < GetMinor())
    {
      return false;
    }
    // Compare on patch
    return other.GetPatch() <= GetPatch();

    int GetMajor() const
    {
      return mMajor;
    };
    int GetMinor() const
    {
      return mMinor;
    };
    int GetPatch() const
    {
      return mPatch;
    };

  private:
    int mMajor;
    int mMinor;
    int mPatch;
  };
};

int NeuralAmpModeler::_UnserializeStateWithKnownVersion(const IByteChunk& chunk, int startPos)
{
  // We already got through the header before calling this.
  int pos = startPos;

  // Get the version
  WDL_String wVersion;
  pos = chunk.GetStr(wVersion, pos);
  std::string versionStr(wVersion.Get());
  _Version version(versionStr);
  // Act accordingly
  nlohmann::json config;
  if (version >= _Version(0, 7, 12))
  {
    pos _GetConfigFrom_0_7_12(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 10))
  {
    pos _GetConfigFrom_0_7_10(chunk, pos, config);
  }
  else
  {
    // You shouldn't be here...
    assert(false);
  }
  _UnserializeApplyConfig(config);
  return pos;
}
