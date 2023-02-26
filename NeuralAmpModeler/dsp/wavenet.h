#pragma once

#include <string>
#include <vector>

#include "json.hpp"
#include <Eigen/Dense>

#include "dsp.h"

namespace wavenet {
// Rework the initialization API slightly. Merge w/ dsp.h later.
class _DilatedConv : public Conv1D {
public:
  _DilatedConv(const int in_channels, const int out_channels,
               const int kernel_size, const int bias, const int dilation);
};

class _Layer {
public:
  _Layer(const int condition_size, const int channels, const int kernel_size,
         const int dilation, const std::string activation, const bool gated)
      : _activation(activation), _gated(gated),
        _conv(channels, gated ? 2 * channels : channels, kernel_size, true,
              dilation),
        _input_mixin(condition_size, gated ? 2 * channels : channels, false),
        _1x1(channels, channels, true){};
  void set_params_(std::vector<float>::iterator &params);
  // :param `input`: from previous layer
  // :param `output`: to next layer
  void process_(const Eigen::MatrixXf &input, const Eigen::MatrixXf &condition,
                Eigen::MatrixXf &head_input, Eigen::MatrixXf &output,
                const long i_start, const long j_start);
  void set_num_frames_(const long num_frames);
  long get_channels() const { return this->_conv.get_in_channels(); };
  int get_dilation() const { return this->_conv.get_dilation(); };
  long get_kernel_size() const { return this->_conv.get_kernel_size(); };

private:
  // The dilated convolution at the front of the block
  _DilatedConv _conv;
  // Input mixin
  Conv1x1 _input_mixin;
  // The post-activation 1x1 convolution
  Conv1x1 _1x1;
  // The internal state
  Eigen::MatrixXf _z;

  const std::string _activation;
  const bool _gated;
};

class LayerArrayParams {
public:
  LayerArrayParams(const int input_size_, const int condition_size_,
                   const int head_size_, const int channels_,
                   const int kernel_size_, const std::vector<int> &dilations_,
                   const std::string activation_, const bool gated_,
                   const bool head_bias_)
      : input_size(input_size_), condition_size(condition_size_),
        head_size(head_size_), channels(channels_), kernel_size(kernel_size_),
        activation(activation_), gated(gated_), head_bias(head_bias_) {
    for (int i = 0; i < dilations_.size(); i++)
      this->dilations.push_back(dilations_[i]);
  };

  const int input_size;
  const int condition_size;
  const int head_size;
  const int channels;
  const int kernel_size;
  std::vector<int> dilations;
  const std::string activation;
  const bool gated;
  const bool head_bias;
};

// An array of layers with the same channels, kernel sizes, activations.
class _LayerArray {
public:
  _LayerArray(const int input_size, const int condition_size,
              const int head_size, const int channels, const int kernel_size,
              const std::vector<int> &dilations, const std::string activation,
              const bool gated, const bool head_bias);

  void advance_buffers_(const int num_frames);

  // Preparing for frames:
  // Rewind buffers if needed
  // Shift index to prepare
  //
  void prepare_for_frames_(const long num_frames);

  // All arrays are "short".
  void process_(const Eigen::MatrixXf &layer_inputs, // Short
                const Eigen::MatrixXf &condition,    // Short
                Eigen::MatrixXf &layer_outputs,      // Short
                Eigen::MatrixXf &head_inputs,        // Sum up on this.
                Eigen::MatrixXf &head_outputs        // post head-rechannel
  );
  void set_num_frames_(const long num_frames);
  void set_params_(std::vector<float>::iterator &it);

  // "Zero-indexed" receptive field.
  // E.g. a 1x1 convolution has a z.i.r.f. of zero.
  long get_receptive_field() const;

private:
  long _buffer_start;
  // The rechannel before the layers
  Conv1x1 _rechannel;

  // Buffers in between layers.
  // buffer [i] is the input to layer [i].
  // the last layer outputs to a short array provided by outside.
  std::vector<Eigen::MatrixXf> _layer_buffers;
  // The layer objects
  std::vector<_Layer> _layers;

  // Rechannel for the head
  Conv1x1 _head_rechannel;

  long _get_buffer_size() const {
    return this->_layer_buffers.size() > 0 ? this->_layer_buffers[0].cols() : 0;
  };
  long _get_channels() const;
  // "One-indexed" receptive field
  // TODO remove!
  // E.g. a 1x1 convolution has a o.i.r.f. of one.
  long _get_receptive_field() const;
  void _rewind_buffers_();
};

// The head module
// [Act->Conv] x L
class _Head {
public:
  _Head(const int input_size, const int num_layers, const int channels,
        const std::string activation);
  void set_params_(std::vector<float>::iterator &params);
  // NOTE: the head transforms the provided input by applying a nonlinearity
  // to it in-place!
  void process_(Eigen::MatrixXf &inputs, Eigen::MatrixXf &outputs);
  void set_num_frames_(const long num_frames);

private:
  int _channels;
  std::vector<Conv1x1> _layers;
  Conv1x1 _head;
  std::string _activation;

  // Stores the outputs of the convs *except* the last one, which goes in
  // The array `outputs` provided to .process_()
  std::vector<Eigen::MatrixXf> _buffers;

  // Apply the activation to the provided array, in-place
  void _apply_activation_(Eigen::MatrixXf &x);
};

// The main WaveNet model
// Both parametric and not; difference is handled at param read-in.
class WaveNet : public DSP {
public:
  WaveNet(const std::vector<LayerArrayParams> &layer_array_params,
          const float head_scale, const bool with_head,
          nlohmann::json parametric, std::vector<float> params);

  //    WaveNet(WaveNet&&) = default;
  //    WaveNet& operator=(WaveNet&&) = default;
  //    ~WaveNet() = default;

  void finalize_(const int num_frames) override;
  void set_params_(std::vector<float> &params);

private:
  long _num_frames;
  std::vector<_LayerArray> _layer_arrays;
  // Their outputs
  std::vector<Eigen::MatrixXf> _layer_array_outputs;
  // Head _head;

  // Element-wise arrays:
  Eigen::MatrixXf _condition;
  // One more than total layer arrays
  std::vector<Eigen::MatrixXf> _head_arrays;
  float _head_scale;
  Eigen::MatrixXf _head_output;

  // Names of the params, sorted.
  // TODO move this up, ugh.
  std::vector<std::string> _param_names;

  void _advance_buffers_(const int num_frames);
  // Get the info from the parametric config
  void _init_parametric_(nlohmann::json &parametric);
  void _prepare_for_frames_(const long num_frames);
  // Reminder: From ._input_post_gain to ._core_dsp_output
  void _process_core_() override;

  // Ensure that all buffer arrays are the right size for this num_frames
  void _set_num_frames_(const long num_frames);

  // The net starts with random parameters inside; we need to wait for a full
  // receptive field to pass through before we can count on the output being
  // ok. This implements a gentle "ramp-up" so that there's no "pop" at the
  // start.
  long _anti_pop_countdown;
  const long _anti_pop_ramp = 4000;
  void _anti_pop_();
  void _reset_anti_pop_();
};
}; // namespace wavenet
