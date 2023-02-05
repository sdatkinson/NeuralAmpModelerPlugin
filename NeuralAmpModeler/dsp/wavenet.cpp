#include <algorithm>
#include <iostream>

#include <Eigen/Dense>

#include "wavenet.h"

wavenet::_DilatedConv::_DilatedConv(
  const int in_channels,
  const int out_channels,
  const int kernel_size,
  const int bias,
  const int dilation
)
{
  this->set_size_(in_channels, out_channels, kernel_size, bias, dilation);
}

void wavenet::_Layer::set_params_(std::vector<float>::iterator& params)
{
  this->_conv.set_params_(params);
  this->_input_mixin.set_params_(params);
  this->_1x1.set_params_(params);
}

void wavenet::_Layer::process_(
  const Eigen::MatrixXf& input,
  const Eigen::MatrixXf& condition,
  Eigen::MatrixXf& head_input,
  Eigen::MatrixXf& output,
  const long i_start,
  const long j_start
)
{
  const long ncols = condition.cols();
  const long channels = this->get_channels();
  // Input dilated conv
  this->_conv.process_(input, this->_z, i_start, ncols, 0);
  // Mix-in condition
  this->_z += this->_input_mixin.process(condition);
  if (this->_activation == "Tanh")
    tanh_(this->_z, 0, channels, 0, this->_z.cols());
  else if (this->_activation == "ReLU")
    relu_(this->_z, 0, channels, 0, this->_z.cols());
  else
    throw std::runtime_error("Unrecognized activation.");
  if (this->_gated) {
    sigmoid_(this->_z, channels, 2 * channels, 0, this->_z.cols());
     this->_z.topRows(channels).array() *= this->_z.bottomRows(channels).array();
    // this->_z.topRows(channels) = this->_z.topRows(channels).cwiseProduct(
    //   this->_z.bottomRows(channels)
    // );
  }

  head_input += this->_z.topRows(channels);
  output.middleCols(j_start, ncols) = input.middleCols(i_start, ncols)
    + this->_1x1.process(this->_z.topRows(channels));
}

void wavenet::_Layer::set_num_frames_(const int num_frames)
{
  this->_z.resize(this->_conv.get_out_channels(), num_frames);
}

// LayerArray =================================================================

#define LAYER_ARRAY_BUFFER_SIZE 65536

wavenet::_LayerArray::_LayerArray(
  const int input_size,
  const int condition_size,
  const int head_size,
  const int channels,
  const int kernel_size,
  const std::vector<int>& dilations,
  const std::string activation,
  const bool gated,
  const bool head_bias
) :
  _rechannel(input_size, channels, false),
  _head_rechannel(channels, head_size, head_bias)
{
  for (int i = 0; i < dilations.size(); i++)
    this->_layers.push_back(_Layer(condition_size, channels, kernel_size, dilations[i], activation, gated));
  const long receptive_field = this->_get_receptive_field();
  for (int i = 0; i < dilations.size(); i++)
    this->_layer_buffers.push_back(
      Eigen::MatrixXf(channels, LAYER_ARRAY_BUFFER_SIZE + receptive_field - 1)
    );
  this->_buffer_start = this->_get_receptive_field() - 1;
}

void wavenet::_LayerArray::advance_buffers_(const int num_frames)
{
  this->_buffer_start += num_frames;
}

long wavenet::_LayerArray::get_receptive_field() const
{
    long result = 0;
    for (int i = 0; i < this->_layers.size(); i++)
        result += this->_layers[i].get_dilation() * (this->_layers[i].get_kernel_size() - 1);
    return result;
}

void wavenet::_LayerArray::prepare_for_frames_(const int num_frames)
{
  if (this->_buffer_start + num_frames > this->_get_buffer_size())
    this->_rewind_buffers_();
}

void wavenet::_LayerArray::process_(
  const Eigen::MatrixXf& layer_inputs,
  const Eigen::MatrixXf& condition,
  Eigen::MatrixXf& head_inputs,
  Eigen::MatrixXf& layer_outputs,
  Eigen::MatrixXf& head_outputs
)
{
  this->_layer_buffers[0].middleCols(this->_buffer_start, layer_inputs.cols()) =
    this->_rechannel.process(layer_inputs);
  const int last_layer = this->_layers.size() - 1;
  for (int i = 0; i < this->_layers.size(); i++) {
    this->_layers[i].process_(
      this->_layer_buffers[i],
      condition,
      head_inputs,
      i == last_layer ? layer_outputs : this->_layer_buffers[i + 1],
      this->_buffer_start,
      i == last_layer ? 0 : this->_buffer_start
    );
  }
  head_outputs = this->_head_rechannel.process(head_inputs);
}

void wavenet::_LayerArray::set_num_frames_(const int num_frames)
{
  // Wavenet checks for unchanged num_frames; if we made it here, there's
  // something to do.
  if (LAYER_ARRAY_BUFFER_SIZE - num_frames < this->_get_receptive_field()) {
    std::stringstream ss;
    ss << "Asked to accept a buffer of "
      << num_frames
      << " samples, but the buffer is too short ("
      << LAYER_ARRAY_BUFFER_SIZE
      << ") to get out of the recptive field ("
      << this->_get_receptive_field()
      << "); copy errors could occur!\n";
    throw std::runtime_error(ss.str().c_str());
  }
  for (int i = 0; i < this->_layers.size(); i++)
    this->_layers[i].set_num_frames_(num_frames);
}

void wavenet::_LayerArray::set_params_(std::vector<float>::iterator& params)
{
  this->_rechannel.set_params_(params);
  for (int i = 0; i < this->_layers.size(); i++)
    this->_layers[i].set_params_(params);
  this->_head_rechannel.set_params_(params);
}

int wavenet::_LayerArray::_get_channels() const
{
  return this->_layers.size() > 0 ? this->_layers[0].get_channels() : 0;
}

long wavenet::_LayerArray::_get_receptive_field() const
{
  long res = 1;
  for (int i = 0; i < this->_layers.size(); i++)
    res += (this->_layers[i].get_kernel_size() - 1) * this->_layers[i].get_dilation();
  return res;
}

void wavenet::_LayerArray::_rewind_buffers_()
// Consider wrapping instead...
// Can make this smaller--largest dilation, not receptive field!
{
  const long start = this->_get_receptive_field() - 1;
  for (int i = 0; i < this->_layer_buffers.size(); i++) {
    const long d = (this->_layers[i].get_kernel_size()-1) * this->_layers[i].get_dilation();
    this->_layer_buffers[i].middleCols(start - d, d) = this->_layer_buffers[i].middleCols(
      this->_buffer_start - d, d
    );
  }
  this->_buffer_start = start;
}

// Head =======================================================================

wavenet::_Head::_Head(
  const int input_size,
  const int num_layers,
  const int channels,
  const std::string activation
) :
  _channels(channels),
  _activation(activation),
  _head(num_layers > 0 ? channels : input_size, 1, true)
{
  assert(num_layers > 0);
  int dx = input_size;
  for (int i = 0; i < num_layers; i++) {
    this->_layers.push_back(Conv1x1(dx, i == num_layers-1 ? 1 : channels, true));
    dx = channels;
    if (i < num_layers-1)
      this->_buffers.push_back(Eigen::MatrixXf());
  }
}

void wavenet::_Head::set_params_(std::vector<float>::iterator& params)
{
  for (int i = 0; i < this->_layers.size(); i++)
    this->_layers[i].set_params_(params);
}

void wavenet::_Head::process_(
  Eigen::MatrixXf& inputs,
  Eigen::MatrixXf& outputs
)
{
  const int num_layers = this->_layers.size();
  this->_apply_activation_(inputs);
  if (num_layers == 1)
    outputs = this->_layers[0].process(inputs);
  else {
    this->_buffers[0] = this->_layers[0].process(inputs);
    for (int i = 1; i < num_layers; i++) {//Asserted > 0 layers
      this->_apply_activation_(this->_buffers[i - 1]);
      if (i < num_layers - 1)
        this->_buffers[i] = this->_layers[i].process(this->_buffers[i - 1]);
      else
        outputs = this->_layers[i].process(this->_buffers[i - 1]);
    }
  }
}

void wavenet::_Head::set_num_frames_(const int num_frames)
{
  for (int i = 0; i < this->_buffers.size(); i++)
    this->_buffers[i].resize(this->_channels, num_frames);
}

void wavenet::_Head::_apply_activation_(Eigen::MatrixXf& x)
{
  if (this->_activation == "Tanh")
    tanh_(x);
  else if (this->_activation == "ReLU")
    relu_(x);
  else
    throw std::runtime_error("Unrecognized activation.");
}

// WaveNet ====================================================================

wavenet::WaveNet::WaveNet(
  const std::vector<wavenet::LayerArrayParams>& layer_array_params,
  const float head_scale,
  const bool with_head,
  nlohmann::json parametric,
  std::vector<float> params
) :
  //_head(channels, head_layers, head_channels, head_activation),
  _num_frames(0),
  _head_scale(head_scale)
{
  if (with_head)
    throw std::runtime_error("Head not implemented!");
  this->_init_parametric_(parametric);
  for (int i = 0; i < layer_array_params.size(); i++) {
    this->_layer_arrays.push_back(
      wavenet::_LayerArray(
        layer_array_params[i].input_size,
        layer_array_params[i].condition_size,
        layer_array_params[i].head_size,
        layer_array_params[i].channels,
        layer_array_params[i].kernel_size,
        layer_array_params[i].dilations,
        layer_array_params[i].activation,
        layer_array_params[i].gated,
        layer_array_params[i].head_bias
      )
    );
    this->_layer_array_outputs.push_back(Eigen::MatrixXf(layer_array_params[i].channels, 0));
    if (i == 0)
      this->_head_arrays.push_back(Eigen::MatrixXf(layer_array_params[i].channels, 0));
    if (i > 0)
      if (layer_array_params[i].channels != layer_array_params[i - 1].head_size) {
        std::stringstream ss;
        ss << "channels of layer " << i << " (" << layer_array_params[i].channels
          << ") doesn't match head_size of preceding layer ("
          << layer_array_params[i - 1].head_size
          << "!\n";
        throw std::runtime_error(ss.str().c_str());
      }
    this->_head_arrays.push_back(Eigen::MatrixXf(layer_array_params[i].head_size, 0));
  }
  this->_head_output.resize(1, 0);  // Mono output!
  this->set_params_(params);
  this->_reset_anti_pop_();
}

void wavenet::WaveNet::finalize_(const int num_frames)
{
  this->DSP::finalize_(num_frames);
  this->_advance_buffers_(num_frames);
}

void wavenet::WaveNet::set_params_(std::vector<float>& params)
{
  std::vector<float>::iterator it = params.begin();
  for (int i = 0; i < this->_layer_arrays.size(); i++)
    this->_layer_arrays[i].set_params_(it);
  //this->_head.set_params_(it);
  this->_head_scale = *(it++);
  if (it != params.end()) {
    std::stringstream ss;
    for (int i = 0; i < params.size(); i++)
      if (params[i] == *it) {
        ss << "Parameter mismatch: assigned " << i+1 << " parameters, but " << params.size() << " were provided.";
        throw std::runtime_error(ss.str().c_str());
      }
    ss << "Parameter mismatch: provided " << params.size() << " weights, but the model expects more.";
    throw std::runtime_error(ss.str().c_str());
  }
}

void wavenet::WaveNet::_advance_buffers_(const int num_frames)
{
  for (int i = 0; i < this->_layer_arrays.size(); i++)
    this->_layer_arrays[i].advance_buffers_(num_frames);
}

void wavenet::WaveNet::_init_parametric_(nlohmann::json& parametric)
{
  for (nlohmann::json::iterator it = parametric.begin(); it != parametric.end(); ++it)
    this->_param_names.push_back(it.key());
    // TODO assert continuous 0 to 1
  std::sort(this->_param_names.begin(), this->_param_names.end());
}

void wavenet::WaveNet::_prepare_for_frames_(const int num_frames)
{
  for (int i = 0; i < this->_layer_arrays.size(); i++)
    this->_layer_arrays[i].prepare_for_frames_(num_frames);
}

void wavenet::WaveNet::_process_core_()
{
  const long num_frames = this->_input_post_gain.size();
  this->_set_num_frames_(num_frames);
  this->_prepare_for_frames_(num_frames);

  // Fill into condition array:
  // Clumsy...
  for (int j = 0; j < num_frames; j++) {
    this->_condition(0, j) = this->_input_post_gain[j];
    if (this->_stale_params)  // Column-major assignment; good for Eigen. Let the compiler optimize this.
      for (int i = 0; i < this->_param_names.size(); i++)
        this->_condition(i + 1, j) = (float) this->_params[this->_param_names[i]];
  }

  // Main layer arrays:
  // Layer-to-layer
  // Sum on head output
  this->_head_arrays[0].setZero();
  for (int i = 0; i < this->_layer_arrays.size(); i++)
    this->_layer_arrays[i].process_(
      i == 0 ? this->_condition : this->_layer_array_outputs[i-1],
      this->_condition,
      this->_head_arrays[i],
      this->_layer_array_outputs[i],
      this->_head_arrays[i+1]
    );
  //this->_head.process_(
  //  this->_head_input,
  //  this->_head_output
  //);
  // Copy to required output array
  // Hack: apply head scale here; revisit when/if I activate the head.
  // assert(this->_head_output.rows() == 1);

  const int final_head_array = this->_head_arrays.size() - 1;
  assert(this->_head_arrays[final_head_array].rows() == 1);
  for (int s = 0; s < num_frames; s++)
    this->_core_dsp_output[s] = this->_head_scale * this->_head_arrays[final_head_array](0, s);
  // Apply anti-pop
  this->_anti_pop_();
}

void wavenet::WaveNet::_set_num_frames_(const int num_frames)
{
  if (num_frames == this->_num_frames)
    return;

  this->_condition.resize(1 + this->_param_names.size(), num_frames);
  for (int i = 0; i < this->_head_arrays.size(); i++)
    this->_head_arrays[i].resize(this->_head_arrays[i].rows(), num_frames);
  for (int i = 0; i < this->_layer_array_outputs.size(); i++)
    this->_layer_array_outputs[i].resize(this->_layer_array_outputs[i].rows(), num_frames);
  this->_head_output.resize(this->_head_output.rows(), num_frames);
  
  for (int i = 0; i < this->_layer_arrays.size(); i++)
    this->_layer_arrays[i].set_num_frames_(num_frames);
  //this->_head.set_num_frames_(num_frames);
  this->_num_frames = num_frames;
}

void wavenet::WaveNet::_anti_pop_()
{
    if (this->_anti_pop_countdown >= this->_anti_pop_ramp)
        return;
    const float slope = 1.0f / float(this->_anti_pop_ramp);
    for (int i = 0; i < this->_core_dsp_output.size(); i++)
    {
        if (this->_anti_pop_countdown >= this->_anti_pop_ramp)
            break;
        const float gain = std::max(slope * float(this->_anti_pop_countdown), 0.0f);
        this->_core_dsp_output[i] *= gain;
        this->_anti_pop_countdown++;
    }
}

void wavenet::WaveNet::_reset_anti_pop_()
{
    // You need the "real" receptive field, not the buffers.
    long receptive_field = 1;
    for (int i = 0; i < this->_layer_arrays.size(); i++)
        receptive_field += this->_layer_arrays[i].get_receptive_field();
    this->_anti_pop_countdown = -receptive_field;
}
