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
  auto getParamByName = [&](std::string& name) {
    // Could use a map but eh
    for (int i = 0; i < kNumParams; i++)
    {
      iplug::IParam* param = GetParam(i);
      if (strcmp(param->GetName(), name.c_str()) == 0)
      {
        return param;
      }
    }
    // else
    return (iplug::IParam*)nullptr;
  };
  TRACE
  ENTER_PARAMS_MUTEX
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    std::string name = it.key();
    iplug::IParam* pParam = getParamByName(name);
    if (pParam != nullptr)
    {
      pParam->Set(*it);
      iplug::Trace(TRACELOC, "%s %f", pParam->GetName(), pParam->Value());
    }
    else
    {
      iplug::Trace(TRACELOC, "%s NOT-FOUND", name.c_str());
    }
  }
  OnParamReset(iplug::EParamSource::kPresetRecall);
  LEAVE_PARAMS_MUTEX

  mNAMPath.Set(static_cast<std::string>(config["NAMPath"]).c_str());
  mIRPath.Set(static_cast<std::string>(config["IRPath"]).c_str());
  mHighLightColor.Set(static_cast<std::string>(config["HighLightColor"]).c_str());

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
int _UnserializePathsAndExpectedKeys(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config,
                                     std::vector<std::string>& paramNames)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["HighLightColor"] = std::string(path.Get());

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
  for (auto it = newNames.begin(); it != newNames.end(); ++it)
  {
    j[it->second] = j[it->first];
    j.erase(it->first);
  }
}

// v0.7.12

void _UpdateConfigFrom_0_7_12(nlohmann::json& config)
{
  // Fill me in once something changes!
}

int _GetConfigFrom_0_7_12(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_12(config);
  return pos;
}

// 0.7.10

void _UpdateConfigFrom_0_7_10(nlohmann::json& config)
{
  // Note: "OutNorm" is Bool-like in v0.7.10, but "OutputMode" is enum.
  // This works because 0 is "Raw" (cf OutNorm false) and 1 is "Calibrated" (cf OutNorm true).
  std::unordered_map<std::string, std::string> newNames{{"OutNorm", "OutputMode"}};
  _RenameKeys(config, newNames);
  // There are new parameters. If they're not included, then 0.7.12 is ok, but future ones might not be.
  config[kCalibrateInputParamName] = (double)kDefaultCalibrateInput;
  config[kInputCalibrationLevelParamName] = kDefaultInputCalibrationLevel;
  _UpdateConfigFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_10(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};
  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_10(config);
  return pos;
}

// Earlier than 0.7.10 (Assumed to be 0.7.3-0.7.9)

void _UpdateConfigFrom_Earlier(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> newNames{{"Gate", "Threshold"}};
  _RenameKeys(config, newNames);
  _UpdateConfigFrom_0_7_10(config);
}

int _GetConfigFrom_Earlier(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Gate", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_Earlier(config);
  return pos;
}

//==============================================================================

class _Version
{
public:
  _Version(const int major, const int minor, const int patch)
  : mMajor(major)
  , mMinor(minor)
  , mPatch(patch){};
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

  bool operator>=(const _Version& other) const
  {
    // Compare on major version:
    if (GetMajor() > other.GetMajor())
    {
      return true;
    }
    if (GetMajor() < other.GetMajor())
    {
      return false;
    }
    // Compare on minor
    if (GetMinor() > other.GetMinor())
    {
      return true;
    }
    if (GetMinor() < other.GetMinor())
    {
      return false;
    }
    // Compare on patch
    return GetPatch() >= other.GetPatch();
  };

  int GetMajor() const { return mMajor; };
  int GetMinor() const { return mMinor; };
  int GetPatch() const { return mPatch; };

private:
  int mMajor;
  int mMinor;
  int mPatch;
};

int NeuralAmpModeler::_UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos)
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
    pos = _GetConfigFrom_0_7_12(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 10))
  {
    pos = _GetConfigFrom_0_7_10(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 9))
  {
    pos = _GetConfigFrom_Earlier(chunk, pos, config);
  }
  else
  {
    // You shouldn't be here...
    assert(false);
  }
  _UnserializeApplyConfig(config);
  return pos;
}

int NeuralAmpModeler::_UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  nlohmann::json config;
  int pos = _GetConfigFrom_Earlier(chunk, startPos, config);
  _UnserializeApplyConfig(config);
  return pos;
}
