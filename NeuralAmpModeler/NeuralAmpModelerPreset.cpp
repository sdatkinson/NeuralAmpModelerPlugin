#include "NeuralAmpModelerPreset.h"
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <wdlstring.h>


NeuralAmpModelerPreset::NeuralAmpModelerPreset(std::string name, std::string desc, std::string ampPath,
                                               std::string irPath, std::vector<float> params)
: _name(std::move(name))
, _description(std::move(desc))
, _amp(std::move(ampPath))
, _ir(std::move(irPath))
, _params(std::move(params))
{
  _version = CURRENT_PRESET_VERSION; // future: handle preset versioning
}

const NeuralAmpModelerPreset* NeuralAmpModelerPreset::Deserialize(const WDL_String& presetPath,
                                                                  std::string& errorMessage)
{
  NeuralAmpModelerPreset* result = nullptr;

  try
  {
    auto path = std::filesystem::u8path(presetPath.Get());
    auto pathDirectory = path.parent_path();
    std::ifstream i(path);
    nlohmann::json j;
    i >> j;
    auto version = j["version"];

    auto presetName = j["name"];
    auto presetDescription = j["description"];
    auto ampModel = static_cast<std::string>(j["amp"]);
    auto ampIR = static_cast<std::string>(j["ir"]);
    std::filesystem::path pAmpModel(ampModel);
    std::filesystem::path pAmpIR(ampIR);

    if( pAmpModel.is_relative()) pAmpModel = absolute(pathDirectory / ampModel);
    if( pAmpIR.is_relative()) pAmpIR = absolute(pathDirectory / ampIR);
    // MAKE pAmpModel an absolute path

    if (j.find("params") != j.end())
    {
      auto params_list = j["params"];
      std::vector<float> params;
      for (auto& it : params_list)
        params.push_back(it);
      result = new NeuralAmpModelerPreset(presetName, presetDescription, pAmpModel.string(), pAmpIR.string(), params);
    }
    else
      throw std::runtime_error("Corrupted preset file is missing params.");
  }
  catch (std::exception& e)
  {
    errorMessage = e.what();
    result = nullptr;
  }

  return result;
}

// Generate a method that writes json parameters to a file using nlohmann::json using the same parameters as implemented
// in LoadFrom()
bool NeuralAmpModelerPreset::Serialize(const NeuralAmpModelerPreset& preset, const WDL_String& presetPath, std::string& errorMessage)
{
  // Create a json instance from the preset object
  try
  {
    auto presetFilePath = std::filesystem::u8path(presetPath.Get());
    // force extension of presetPath to be ".nps"
    if (presetFilePath.extension() != ".nps") presetFilePath.replace_extension(".nps");
    const auto presetDirectoryPath = presetFilePath.parent_path();

    // First, try to make the path relative to the preset file
    auto optimizedPathtoAMP = std::filesystem::relative( preset.AmpPath(), presetDirectoryPath);
    auto optimizedPathtoIR = std::filesystem::relative( preset.IrPath(), presetDirectoryPath);

    // if the path can't be made relative, then it's not in the same directory as the preset file, so use absolute path
    if (optimizedPathtoAMP.empty()) optimizedPathtoAMP = std::filesystem::absolute( preset.AmpPath());
    if (optimizedPathtoIR.empty()) optimizedPathtoIR = std::filesystem::absolute( preset.IrPath());

    std::ofstream o(presetFilePath);

    nlohmann::json j;
    // Write all the attributes of the preset object to the json instance
    j["version"] = preset.Version();
    j["name"] = preset.Name();
    j["description"] = preset.Description();
    j["amp"] = optimizedPathtoAMP.string();
    j["ir"] = optimizedPathtoIR.string();
    j["params"] = preset.Params();
    // Write the json instance to the file
    o << std::setw(4) << j << std::endl;
  }
  catch (const std::exception& e)
  {
    errorMessage = e.what();
    return false;
  }

  return true;
}
