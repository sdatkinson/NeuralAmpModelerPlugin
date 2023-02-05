#include <algorithm>
#include <string>
#include <vector>

#include "activations.h"
#include "lstm.h"

lstm::LSTMCell::LSTMCell(const int input_size, const int hidden_size,
                         std::vector<float>::iterator &params) {
  // Resize arrays
  this->_w.resize(4 * hidden_size, input_size + hidden_size);
  this->_b.resize(4 * hidden_size);
  this->_xh.resize(input_size + hidden_size);
  this->_ifgo.resize(4 * hidden_size);
  this->_c.resize(hidden_size);

  // Assign in row-major because that's how PyTorch goes.
  for (int i = 0; i < this->_w.rows(); i++)
    for (int j = 0; j < this->_w.cols(); j++)
      this->_w(i, j) = *(params++);
  for (int i = 0; i < this->_b.size(); i++)
    this->_b[i] = *(params++);
  const int h_offset = input_size;
  for (int i = 0; i < hidden_size; i++)
    this->_xh[i + h_offset] = *(params++);
  for (int i = 0; i < hidden_size; i++)
    this->_c[i] = *(params++);
}

void lstm::LSTMCell::process_(const Eigen::VectorXf &x) {
  const long hidden_size = this->_get_hidden_size();
  const long input_size = this->_get_input_size();
  // Assign inputs
  this->_xh(Eigen::seq(0, input_size - 1)) = x;
  // The matmul
  this->_ifgo = this->_w * this->_xh + this->_b;
  // Elementwise updates (apply nonlinearities here)
  const long i_offset = 0;
  const long f_offset = hidden_size;
  const long g_offset = 2 * hidden_size;
  const long o_offset = 3 * hidden_size;
  for (auto i = 0; i < hidden_size; i++)
    this->_c[i] =
        activations::sigmoid(this->_ifgo[i + f_offset]) * this->_c[i] +
        activations::sigmoid(this->_ifgo[i + i_offset]) *
            tanhf(this->_ifgo[i + g_offset]);
  const long h_offset = input_size;
  for (int i = 0; i < hidden_size; i++)
    this->_xh[i + h_offset] =
        activations::sigmoid(this->_ifgo[i + o_offset]) * tanhf(this->_c[i]);
}

lstm::LSTM::LSTM(const int num_layers, const int input_size,
                 const int hidden_size, std::vector<float> &params,
                 nlohmann::json &parametric) {
  this->_init_parametric(parametric);
  std::vector<float>::iterator it = params.begin();
  for (int i = 0; i < num_layers; i++)
    this->_layers.push_back(
        LSTMCell(i == 0 ? input_size : hidden_size, hidden_size, it));
  this->_head_weight.resize(hidden_size);
  for (int i = 0; i < hidden_size; i++)
    this->_head_weight[i] = *(it++);
  this->_head_bias = *(it++);
  assert(it == params.end());
}

void lstm::LSTM::_init_parametric(nlohmann::json &parametric) {
  std::vector<std::string> parametric_names;
  for (nlohmann::json::iterator it = parametric.begin(); it != parametric.end();
       ++it) {
    parametric_names.push_back(it.key());
  }
  std::sort(parametric_names.begin(), parametric_names.end());
  {
    int i = 1;
    for (std::vector<std::string>::iterator it = parametric_names.begin();
         it != parametric_names.end(); ++it, i++)
      this->_parametric_map[*it] = i;
  }

  this->_input_and_params.resize(1 + parametric.size()); // TODO amp parameters
}

void lstm::LSTM::_process_core_() {
  // Get params into the input vector before starting
  if (this->_stale_params) {
    for (std::unordered_map<std::string, double>::iterator it =
             this->_params.begin();
         it != this->_params.end(); ++it)
      this->_input_and_params[this->_parametric_map[it->first]] = it->second;
    this->_stale_params = false;
  }
  // Process samples, placing results in the required output location
  for (int i = 0; i < this->_input_post_gain.size(); i++)
    this->_core_dsp_output[i] =
        this->_process_sample(this->_input_post_gain[i]);
}

float lstm::LSTM::_process_sample(const float x) {
  if (this->_layers.size() == 0)
    return x;
  this->_input_and_params(0) = x;
  this->_layers[0].process_(this->_input_and_params);
  for (int i = 1; i < this->_layers.size(); i++)
    this->_layers[i].process_(this->_layers[i - 1].get_hidden_state());
  return this->_head_weight.dot(
             this->_layers[this->_layers.size() - 1].get_hidden_state()) +
         this->_head_bias;
}
