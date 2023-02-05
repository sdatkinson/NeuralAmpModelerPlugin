#pragma once
// LSTM implementation

#include <map>
#include <vector>

#include <Eigen/Dense>

#include "dsp.h"
#include "json.hpp"

namespace lstm {
// A Single LSTM cell
// i input
// f forget
// g cell
// o output
// c cell state
// h hidden state
class LSTMCell {
public:
  LSTMCell(const int input_size, const int hidden_size,
           std::vector<float>::iterator &params);
  Eigen::VectorXf get_hidden_state() const {
    return this->_xh(Eigen::placeholders::lastN(this->_get_hidden_size()));
  };
  void process_(const Eigen::VectorXf &x);

private:
  // Parameters
  // xh -> ifgo
  // (dx+dh) -> (4*dh)
  Eigen::MatrixXf _w;
  Eigen::VectorXf _b;

  // State
  // Concatenated input and hidden state
  Eigen::VectorXf _xh;
  // Input, Forget, Cell, Output gates
  Eigen::VectorXf _ifgo;

  // Cell state
  Eigen::VectorXf _c;

  int _get_hidden_size() const { return this->_b.size() / 4; };
  int _get_input_size() const {
    return this->_xh.size() - this->_get_hidden_size();
  };
};

// The multi-layer LSTM model
class LSTM : public DSP {
public:
  LSTM(const int num_layers, const int input_size, const int hidden_size,
       std::vector<float> &params, nlohmann::json &parametric);

protected:
  Eigen::VectorXf _head_weight;
  float _head_bias;
  void _process_core_() override;
  std::vector<LSTMCell> _layers;

  float _process_sample(const float x);

  // Initialize the parametric map
  void _init_parametric(nlohmann::json &parametric);

  // Mapping from param name to index in _input_and_params:
  std::map<std::string, int> _parametric_map;
  // Input sample first, params second
  Eigen::VectorXf _input_and_params;
};
}; // namespace lstm
