#include <fstream>
#include <unordered_set>

#include "HardCodedModel.h"
#include "dsp.h"
#include "json.hpp"
#include "lstm.h"
#include "numpy_util.h"
#include "wavenet.h"

void verify_config_version(const std::string version) {
  const std::unordered_set<std::string> supported_versions({"0.5.0"});
  if (supported_versions.find(version) == supported_versions.end()) {
    std::stringstream ss;
    ss << "Model config is an unsupported version " << version
       << ". Try either converting the model to a more recent version, or "
          "update your version of the NAM plugin.";
    throw std::runtime_error(ss.str());
  }
}

std::vector<float> _get_weights(nlohmann::json const &j,
                                const std::filesystem::path config_path) {
  if (j.find("weights") != j.end()) {
    auto weight_list = j["weights"];
    std::vector<float> weights;
    for (auto it = weight_list.begin(); it != weight_list.end(); ++it)
      weights.push_back(*it);
    return weights;
  } else
    throw std::runtime_error("Corrupted model file is missing weights.");
}

std::unique_ptr<DSP> get_dsp_legacy(const std::filesystem::path model_dir) {
  auto config_filename = model_dir / std::filesystem::path("config.json");
  return get_dsp(config_filename);
}

std::unique_ptr<DSP> get_dsp(const std::filesystem::path config_filename) {
  if (!std::filesystem::exists(config_filename))
    throw std::runtime_error("Config JSON doesn't exist!\n");
  std::ifstream i(config_filename);
  nlohmann::json j;
  i >> j;
  verify_config_version(j["version"]);

  auto architecture = j["architecture"];
  nlohmann::json config = j["config"];
  std::vector<float> params = _get_weights(j, config_filename);

  if (architecture == "Linear") {
    const int receptive_field = config["receptive_field"];
    const bool _bias = config["bias"];
    return std::make_unique<Linear>(receptive_field, _bias, params);
  } else if (architecture == "ConvNet") {
    const int channels = config["channels"];
    const bool batchnorm = config["batchnorm"];
    std::vector<int> dilations;
    for (int i = 0; i < config["dilations"].size(); i++)
      dilations.push_back(config["dilations"][i]);
    const std::string activation = config["activation"];
    return std::make_unique<convnet::ConvNet>(channels, dilations, batchnorm,
                                              activation, params);
  } else if (architecture == "LSTM") {
    const int num_layers = config["num_layers"];
    const int input_size = config["input_size"];
    const int hidden_size = config["hidden_size"];
    auto json = nlohmann::json{};
    return std::make_unique<lstm::LSTM>(num_layers, input_size, hidden_size,
                                        params, json);
  } else if (architecture == "CatLSTM") {
    const int num_layers = config["num_layers"];
    const int input_size = config["input_size"];
    const int hidden_size = config["hidden_size"];
    return std::make_unique<lstm::LSTM>(num_layers, input_size, hidden_size,
                                        params, config["parametric"]);
  } else if (architecture == "WaveNet" || architecture == "CatWaveNet") {
    std::vector<wavenet::LayerArrayParams> layer_array_params;
    for (int i = 0; i < config["layers"].size(); i++) {
      nlohmann::json layer_config = config["layers"][i];
      std::vector<int> dilations;
      for (int j = 0; j < layer_config["dilations"].size(); j++)
        dilations.push_back(layer_config["dilations"][j]);
      layer_array_params.push_back(wavenet::LayerArrayParams(
          layer_config["input_size"], layer_config["condition_size"],
          layer_config["head_size"], layer_config["channels"],
          layer_config["kernel_size"], dilations, layer_config["activation"],
          layer_config["gated"], layer_config["head_bias"]));
    }
    const bool with_head = config["head"] == NULL;
    const float head_scale = config["head_scale"];
    // Solves compilation issue on macOS Error: No matching constructor for
    // initialization of 'wavenet::WaveNet' Solution from
    // https://stackoverflow.com/a/73956681/3768284
    auto parametric_json =
        architecture == "CatWaveNet" ? config["parametric"] : nlohmann::json{};
    return std::make_unique<wavenet::WaveNet>(
        layer_array_params, head_scale, with_head, parametric_json, params);
  } else {
    throw std::runtime_error("Unrecognized architecture");
  }
}

std::unique_ptr<DSP> get_hard_dsp() {
  // Values are defined in HardCodedModel.h
  verify_config_version(std::string(PYTHON_MODEL_VERSION));

  // Uncomment the line that corresponds to the model type that you're using.

  // return std::make_unique<convnet::ConvNet>(CHANNELS, DILATIONS, BATCHNORM,
  // ACTIVATION, PARAMS); return
  // std::make_unique<wavenet::WaveNet>(LAYER_ARRAY_PARAMS, HEAD_SCALE,
  // WITH_HEAD, PARAMETRIC, PARAMS);
  return std::make_unique<lstm::LSTM>(NUM_LAYERS, INPUT_SIZE, HIDDEN_SIZE,
                                      PARAMS, PARAMETRIC);
}
