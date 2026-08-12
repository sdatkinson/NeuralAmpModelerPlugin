#include <fstream>
#include <filesystem>
#include <catch2/catch_all.hpp>
#include <NeuralAmpModelerPreset.h>
#include <json.hpp>

namespace fs = std::filesystem;

static const std::string TestDataDir = "../../UnitTests/TestData/";
static const std::string OutputDataDir = fs::absolute("../../UnitTests/output/").string();
static const auto presetName = "Test Preset";
static const auto presetDescription = "This is a test preset";
static const auto OUTPUT_PATH_CLEAN = fs::absolute(OutputDataDir + "clean.nps").string();
static const auto PRESET_PATH_CLEAN = fs::absolute(TestDataDir   + "clean.nps").string();
static const auto AMP_PATH_CLEAN    = fs::absolute(TestDataDir   + "clean.nam").string();
static const auto IR_PATH_CLEAN     = fs::absolute(TestDataDir   + "clean.wav").string();
static const std::vector params = { 1.0f, 2.0f, 3.0f };

static NeuralAmpModelerPreset CreatePreset()
{
  return {presetName, presetDescription, AMP_PATH_CLEAN, IR_PATH_CLEAN, params};
 
}
static void SetupOutputDirectory()
{
  fs::path outputDir(OutputDataDir);
  if (!exists(outputDir)) create_directory(outputDir);
  else
  {
    // delete all files in the outputDir directory:
    for (auto& p : fs::directory_iterator(outputDir))
      fs::remove(p);
  }
}

TEST_CASE("Can create", "[NeuralAmpModelerPreset]")
{
  NeuralAmpModelerPreset preset = CreatePreset(); // implicit std::move()

  // Check that the preset data matches the original data
  REQUIRE(preset.Name() == presetName );
  REQUIRE(preset.Description() == presetDescription);
  REQUIRE(preset.AmpPath() == fs::absolute(AMP_PATH_CLEAN));
  REQUIRE(preset.IrPath() == fs::absolute(IR_PATH_CLEAN));
  REQUIRE(preset.Params() == params);

}
TEST_CASE("Can read existing preset", "[NeuralAmpModelerPreset]")
{
    std::string errorMessage;
  // Assert that the PRESET_PATH_CLEAN test data exists
  REQUIRE(fs::exists(PRESET_PATH_CLEAN));
  REQUIRE(fs::exists(AMP_PATH_CLEAN));
  REQUIRE(fs::exists(IR_PATH_CLEAN));

  // Deserialize the preset
  WDL_String presetPathWDL(PRESET_PATH_CLEAN.c_str());
  const NeuralAmpModelerPreset* preset = NeuralAmpModelerPreset::Deserialize(presetPathWDL, errorMessage);

  // Check that the deserialized preset matches the original data
  REQUIRE(preset != nullptr);
  REQUIRE(preset->Name() == presetName);
  REQUIRE(preset->Description() == presetDescription);
  REQUIRE(preset->AmpPath() == fs::absolute(AMP_PATH_CLEAN));
  REQUIRE(preset->IrPath() == fs::absolute(IR_PATH_CLEAN));
  REQUIRE(preset->Params() == params);
}   

TEST_CASE("Can deserialize", "[NeuralAmpModelerPreset]")
{
  std::string errorMessage;

  SetupOutputDirectory();
  // Assert that the AMP_PATH_CLEAN test data exists
  REQUIRE(fs::exists(AMP_PATH_CLEAN));
  REQUIRE(fs::exists(IR_PATH_CLEAN));
        

  // Create a JSON file with the preset data
  nlohmann::json j;
  j["version"] = CURRENT_PRESET_VERSION;
  j["name"] = presetName;
  j["description"] = presetDescription;
  j["amp"] = fs::absolute(AMP_PATH_CLEAN).string();
  j["ir"] = fs::absolute(IR_PATH_CLEAN).string();
  j["params"] = params;
  std::ofstream o(OUTPUT_PATH_CLEAN);
  o << j.dump(2);
  o.close();
  WDL_String presetPathWDL(OUTPUT_PATH_CLEAN.c_str());
  // Deserialize the preset
  const NeuralAmpModelerPreset* preset = NeuralAmpModelerPreset::Deserialize(presetPathWDL, errorMessage);

  // Check that the deserialized preset matches the original data
  REQUIRE(preset != nullptr);
  REQUIRE(preset->Name() == presetName);
  REQUIRE(preset->Description() == presetDescription);
  REQUIRE(preset->AmpPath() == fs::absolute(AMP_PATH_CLEAN));
  REQUIRE(preset->IrPath() == fs::absolute(IR_PATH_CLEAN));
  REQUIRE(preset->Params() == params);

  // Clean up the test file
  fs::remove(OUTPUT_PATH_CLEAN);
}

TEST_CASE("Can serialize", "[NeuralAmpModelerPreset]")
{
  SetupOutputDirectory();
  
  // Create the preset
  auto preset = CreatePreset();
  
  // Serialize the preset and save it to file
  WDL_String outputPathWDL(OUTPUT_PATH_CLEAN.c_str());  
  std:: string errorMessage;
  bool success = preset.Serialize(outputPathWDL, errorMessage);
  REQUIRE(success);
  // Load json file into a json object from the output path
  std::ifstream i(outputPathWDL.Get());
  nlohmann::json j;
  i >> j;
  i.close();
// Check that the serialized preset matches the original data
  REQUIRE(j["version"] == CURRENT_PRESET_VERSION);
  REQUIRE(j["name"] == presetName);
  REQUIRE(j["description"] == presetDescription);
  REQUIRE(j["params"] == params);

  // Test that optimal relative paths are used when possible as it facilitates reuse of presets+amp+ir folders on different drives/systems
  REQUIRE(j["amp"] == "..\\TestData\\clean.nam"); // should contain the computed optimal relative amp path to the preset location, as it exists
  REQUIRE(j["ir"]  == "..\\TestData\\clean.wav"); // should contain the computed optimal relative IR path to the preset location, as it exists

      
  // Deserialize the serialized preset
  const NeuralAmpModelerPreset* deserializedPreset = NeuralAmpModelerPreset::Deserialize(outputPathWDL, errorMessage);
  
  // Check that the deserialized preset matches the original preset
  REQUIRE(deserializedPreset != nullptr);
  REQUIRE(deserializedPreset->Name() == presetName);
  REQUIRE(deserializedPreset->Description() == presetDescription);
  REQUIRE(deserializedPreset->AmpPath() == fs::absolute(AMP_PATH_CLEAN));
  REQUIRE(deserializedPreset->IrPath() == fs::absolute(IR_PATH_CLEAN));
  REQUIRE(deserializedPreset->Params() == params);
  
  // Clean up the test file
  fs::remove(OUTPUT_PATH_CLEAN);
}

TEST_CASE("Can serialize and deserialize", "[NeuralAmpModelerPreset]")
{
   SetupOutputDirectory();

  // Create the preset
  auto preset = CreatePreset();

  // Serialize the preset and save it to file
  WDL_String outputPathWDL(OUTPUT_PATH_CLEAN.c_str());
  std::string errorMessage;
  bool success = preset.Serialize(outputPathWDL, errorMessage);
  REQUIRE(success);

  // Deserialize the serialized preset
  const NeuralAmpModelerPreset* deserializedPreset = NeuralAmpModelerPreset::Deserialize(outputPathWDL, errorMessage);

  // Check that the deserialized preset matches the original preset
  REQUIRE(deserializedPreset != nullptr);
  REQUIRE(deserializedPreset->Name() == presetName);
  REQUIRE(deserializedPreset->Description() == presetDescription);
  REQUIRE(deserializedPreset->AmpPath() == fs::absolute(AMP_PATH_CLEAN));
  REQUIRE(deserializedPreset->IrPath() == fs::absolute(IR_PATH_CLEAN));
  REQUIRE(deserializedPreset->Params() == params);

  // Clean up the test file
  fs::remove(OUTPUT_PATH_CLEAN);
}   