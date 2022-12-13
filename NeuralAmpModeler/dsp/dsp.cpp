#include <algorithm>  // std::max_element
#include <cmath>  // pow, tanh, expf
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "json.hpp"
#include "numpy_util.h"
#include "dsp.h"
#include "util.h"

constexpr auto _INPUT_BUFFER_SAFETY_FACTOR = 32;

DSP::DSP()
{ this->_stale_params = true; }

void DSP::process(
  sample** inputs,
  sample** outputs,
  const int num_channels,
  const int num_frames,
  const double input_gain,
  const double output_gain,
  const std::unordered_map<std::string,double>& params
)
{
  this->_get_params_(params);
  this->_apply_input_level_(inputs, num_channels, num_frames, input_gain);
  this->_ensure_core_dsp_output_ready_();
  this->_process_core_();
  this->_apply_output_level_(outputs, num_channels, num_frames, output_gain);
}

void DSP::finalize_(const int num_frames)
{}

void DSP::_get_params_(const std::unordered_map<std::string, double>& input_params)
{
  this->_stale_params = false;
  for (auto it = input_params.begin(); it != input_params.end(); ++it)
  {
    const std::string key = util::lowercase(it->first);
    const double value = it->second;
    if (this->_params.find(key) == this->_params.end())  // Not contained
      this->_stale_params = true;
    else if (this->_params[key] != value)  // Contained but new value
      this->_stale_params = true;
    this->_params[key] = value;
  }
}

void DSP::_apply_input_level_(sample** inputs, const int num_channels, const int num_frames, const double gain)
{
  // Must match exactly; we're going to use the size of _input_post_gain later for num_frames.
  if (this->_input_post_gain.size() != num_frames)
    this->_input_post_gain.resize(num_frames);
  // MONO ONLY
  const int channel = 0;
  for (int i = 0; i < num_frames; i++)
    this->_input_post_gain[i] = float(gain * inputs[channel][i]);
}

void DSP::_ensure_core_dsp_output_ready_()
{
  if (this->_core_dsp_output.size() < this->_input_post_gain.size())
    this->_core_dsp_output.resize(this->_input_post_gain.size());
}

void DSP::_process_core_()
{
  // Default implementation is the null operation
  for (int i = 0; i < this->_input_post_gain.size(); i++)
    this->_core_dsp_output[i] = this->_input_post_gain[i];
}

void DSP::_apply_output_level_(sample** outputs, const int num_channels, const int num_frames, const double gain)
{
  for (int c = 0; c < num_channels; c++)
    for (int s = 0; s < num_frames; s++)
      outputs[c][s] = double(gain * this->_core_dsp_output[s]);
}

// Buffer =====================================================================

Buffer::Buffer(const int receptive_field) : DSP()
{
  this->_set_receptive_field(receptive_field);
}

void Buffer::_set_receptive_field(const int new_receptive_field)
{
  this->_set_receptive_field(new_receptive_field, _INPUT_BUFFER_SAFETY_FACTOR * new_receptive_field);
};

void Buffer::_set_receptive_field(const int new_receptive_field, const int input_buffer_size)
{
  this->_receptive_field = new_receptive_field;
  this->_input_buffer.resize(input_buffer_size);
  this->_reset_input_buffer();
}

void Buffer::_update_buffers_()
{
  const long int num_frames = this->_input_post_gain.size();
  //Make sure that the buffer is big enough for the receptive field and the
  //frames needed!
  {
    const long minimum_input_buffer_size =
      (long)this->_receptive_field
      + _INPUT_BUFFER_SAFETY_FACTOR * num_frames;
    if (this->_input_buffer.size() < minimum_input_buffer_size) {
      long new_buffer_size = 2;
      while (new_buffer_size < minimum_input_buffer_size)
        new_buffer_size *= 2;
      this->_input_buffer.resize(new_buffer_size);
    }
      
  }

  // If we'd run off the end of the input buffer, then we need to move the data back to the start of the
  // buffer and start again.
  if (this->_input_buffer_offset + num_frames > this->_input_buffer.size())
    this->_rewind_buffers_();
  // Put the new samples into the input buffer
  for (long i = this->_input_buffer_offset, j = 0; j < num_frames; i++, j++)
    this->_input_buffer[i] = this->_input_post_gain[j];
  // And resize the output buffer:
  this->_output_buffer.resize(num_frames);
}

void Buffer::_rewind_buffers_()
{
  // Copy the input buffer back
  // RF-1 samples because we've got at least one new one inbound.
  for (long i = 0, j = this->_input_buffer_offset - this->_receptive_field; i < this->_receptive_field; i++, j++)
    this->_input_buffer[i] = this->_input_buffer[j];
  // And reset the offset.
  // Even though we could be stingy about that one sample that we won't be using
  // (because a new set is incoming) it's probably not worth the hyper-optimization
  // and liable for bugs.
  // And the code looks way tidier this way.
  this->_input_buffer_offset = this->_receptive_field;
}

void Buffer::_reset_input_buffer()
{
  this->_input_buffer_offset = this->_receptive_field;
}

void Buffer::finalize_(const int num_frames)
{
  this->DSP::finalize_(num_frames);
  this->_input_buffer_offset += num_frames;
}

// Linear =====================================================================

Linear::Linear(
  const int receptive_field,
  const bool _bias,
  const std::vector<float> &params
) : Buffer(receptive_field)
{
  if (params.size() != (receptive_field + (_bias ? 1 : 0)))
    throw std::runtime_error("Params vector does not match expected size based on architecture parameters");

  this->_weight.resize(this->_receptive_field);
  // Pass in in reverse order so that dot products work out of the box.
  for (int i = 0; i < this->_receptive_field; i++)
    this->_weight(i) = params[receptive_field - 1 - i];
  this->_bias = _bias ? params[receptive_field] : (float) 0.0;
}

void Linear::_process_core_()
{
  this->Buffer::_update_buffers_();

  // Main computation!
  for (long i = 0; i < this->_input_post_gain.size(); i++)
  {
    const long offset = this->_input_buffer_offset - this->_weight.size() + i + 1;
    auto input = Eigen::Map<const Eigen::VectorXf>(&this->_input_buffer[offset], this->_receptive_field);
    this->_core_dsp_output[i] = this->_bias + this->_weight.dot(input);
  }
}

// NN modules =================================================================

void relu_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end,
  const long j_start,
  const long j_end
)
{
  for (long j = j_start; j < j_end; j++)
    for (long i = 0; i < x.rows(); i++)
      x(i, j) = x(i, j) < (float)0.0 ? (float)0.0 : x(i, j);
}

void relu_(
  Eigen::MatrixXf& x,
  const long j_start,
  const long j_end
)
{
  relu_(x, 0, x.rows(), j_start, j_end);
}

void relu_(Eigen::MatrixXf& x) { relu_(x, 0, x.rows(), 0, x.cols()); }

void sigmoid_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end,
  const long j_start,
  const long j_end
)
{
  for (long j = j_start; j < j_end; j++)
    for (long i = i_start; i < i_end; i++)
      x(i, j) = 1.0 / (1.0 + expf(-x(i, j)));
}

void tanh_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end,
  const long j_start,
  const long j_end
)
{
  for (long j = j_start; j < j_end; j++)
    for (long i = i_start; i < i_end; i++)
      x(i, j) = tanh(x(i, j));
}

void tanh_(
  Eigen::MatrixXf& x,
  const long j_start,
  const long j_end
)
{
  tanh_(x, 0, x.rows(), j_start, j_end);
}

void tanh_(Eigen::MatrixXf& x) { tanh_(x, 0, x.rows(), 0, x.cols()); }



void Conv1D::set_params_(std::vector<float>::iterator& params)
{
  if (this->_weight.size() > 0) {
    const int out_channels = this->_weight[0].rows();
    const int in_channels = this->_weight[0].cols();
    // Crazy ordering because that's how it gets flattened.
    for (int i = 0; i < out_channels; i++)
      for (int j = 0; j < in_channels; j++)
        for (int k = 0; k < this->_weight.size(); k++)
          this->_weight[k](i, j) = *(params++);
  }
  for (int i = 0; i < this->_bias.size(); i++)
    this->_bias(i) = *(params++);
}

void Conv1D::set_size_(
  const int in_channels,
  const int out_channels,
  const int kernel_size,
  const bool do_bias,
  const int _dilation
)
{
  this->_weight.resize(kernel_size);
  for (int i = 0; i < this->_weight.size(); i++)
    this->_weight[i].resize(out_channels, in_channels);  // y = Ax, input array (C,L)
  if (do_bias)
    this->_bias.resize(out_channels);
  else
    this->_bias.resize(0);
  this->_dilation = _dilation;
}

void Conv1D::set_size_and_params_(
  const int in_channels,
  const int out_channels,
  const int kernel_size,
  const int _dilation,
  const bool do_bias,
  std::vector<float>::iterator& params
)
{
  this->set_size_(in_channels, out_channels, kernel_size, do_bias, _dilation);
  this->set_params_(params);
}

void Conv1D::process_(
  const Eigen::MatrixXf &input,
  Eigen::MatrixXf &output,
  const long i_start,
  const long ncols,
  const long j_start
) const
{
  // This is the clever part ;)
  for (long k = 0; k < this->_weight.size(); k++) {
    const long offset = this->_dilation * (k + 1 - this->_weight.size());
    if (k == 0)
      output.middleCols(j_start, ncols) = this->_weight[k] * input.middleCols(i_start + offset, ncols);
    else
      output.middleCols(j_start, ncols) += this->_weight[k] * input.middleCols(i_start + offset, ncols);
  }
  if (this->_bias.size() > 0)
    output.middleCols(j_start, ncols).colwise() += this->_bias;
}

long Conv1D::get_num_params() const
{
  long num_params = this->_bias.size();
  for (long i = 0; i < this->_weight.size(); i++)
    num_params += this->_weight[i].size();
  return num_params;
}

Conv1x1::Conv1x1(
  const int in_channels,
  const int out_channels,
  const bool _bias
)
{
  this->_weight.resize(out_channels, in_channels);
  this->_do_bias = _bias;
  if (_bias)
    this->_bias.resize(out_channels);
}

void Conv1x1::set_params_(std::vector<float>::iterator& params)
{
  for (int i = 0; i < this->_weight.rows(); i++)
    for (int j = 0; j < this->_weight.cols(); j++)
      this->_weight(i, j) = *(params++);
  if (this->_do_bias)
    for (int i = 0; i < this->_bias.size(); i++)
      this->_bias(i) = *(params++);
}

Eigen::MatrixXf Conv1x1::process(const Eigen::MatrixXf& input) const
{
  if (this->_do_bias)
    return (this->_weight * input).colwise() + this->_bias;
  else
    return this->_weight * input;
}

// ConvNet ====================================================================

convnet::BatchNorm::BatchNorm(const int dim, std::vector<float>::iterator& params)
{
  // Extract from param buffer
  Eigen::VectorXf running_mean(dim);
  Eigen::VectorXf running_var(dim);
  Eigen::VectorXf _weight(dim);
  Eigen::VectorXf _bias(dim);
  for (int i = 0; i < dim; i++)
    running_mean(i) = *(params++);
  for (int i = 0; i < dim; i++)
    running_var(i) = *(params++);
  for (int i = 0; i < dim; i++)
    _weight(i) = *(params++);
  for (int i = 0; i < dim; i++)
    _bias(i) = *(params++);
  float eps = *(params++);

  // Convert to scale & loc
  this->scale.resize(dim);
  this->loc.resize(dim);
  for (int i = 0; i < dim; i++)
    this->scale(i) = _weight(i) / sqrt(eps + running_var(i));
  this->loc = _bias - this->scale.cwiseProduct(running_mean);
}

void convnet::BatchNorm::process_(Eigen::MatrixXf& x, const long i_start, const long i_end) const
{
  // todo using colwise?
  // #speed but conv probably dominates
  for (int i = i_start; i < i_end; i++) {
    x.col(i) = x.col(i).cwiseProduct(this->scale);
    x.col(i) += this->loc;
  }
}

void convnet::ConvNetBlock::set_params_(
  const int in_channels,
  const int out_channels,
  const int _dilation,
  const bool batchnorm,
  const std::string activation,
  std::vector<float>::iterator &params
)
{
  this->_batchnorm = batchnorm;
  // HACK 2 kernel
  this->conv.set_size_and_params_(in_channels, out_channels, 2, _dilation, !batchnorm, params);
  if (this->_batchnorm)
    this->batchnorm = BatchNorm(out_channels, params);
  this->activation = activation;
}

void convnet::ConvNetBlock::process_(
  const Eigen::MatrixXf& input,
  Eigen::MatrixXf &output,
  const long i_start,
  const long i_end
) const
{
  const long ncols = i_end - i_start;
  this->conv.process_(input, output, i_start, ncols, i_start);
  if (this->_batchnorm)
    this->batchnorm.process_(output, i_start, i_end);
  if (this->activation == "Tanh")
    tanh_(output, i_start, i_end);
  else if (this->activation == "ReLU")
    relu_(output, i_start, i_end);
  else
    throw std::runtime_error("Unrecognized activation");
}

int convnet::ConvNetBlock::get_out_channels() const
{
  return this->conv.get_out_channels();
}

convnet::_Head::_Head(const int channels, std::vector<float>::iterator& params)
{
  this->_weight.resize(channels);
  for (int i = 0; i < channels; i++)
    this->_weight[i] = *(params++);
  this->_bias = *(params++);
}

void convnet::_Head::process_(
  const Eigen::MatrixXf &input,
  Eigen::VectorXf &output,
  const long i_start,
  const long i_end
) const
{
  const long length = i_end - i_start;
  output.resize(length);
  for (long i = 0, j=i_start; i < length; i++, j++)
    output(i) = this->_bias + input.col(j).dot(this->_weight);
}

convnet::ConvNet::ConvNet(
  const int channels,
  const std::vector<int> &dilations,
  const bool batchnorm,
  const std::string activation,
  std::vector<float> &params
) :
  Buffer(*std::max_element(dilations.begin(), dilations.end()))
{
  this->_verify_params(channels, dilations, batchnorm, params.size());
  this->_blocks.resize(dilations.size());
  std::vector<float>::iterator it = params.begin();
  int in_channels = 1;
  for (int i = 0; i < dilations.size(); i++)
    this->_blocks[i].set_params_(i == 0 ? 1 : channels, channels, dilations[i], batchnorm, activation, it);
  this->_block_vals.resize(this->_blocks.size() + 1);
  this->_head = _Head(channels, it);
  if (it != params.end())
    throw std::runtime_error("Didn't touch all the params when initializing wavenet");
  this->_reset_anti_pop_();
}

void convnet::ConvNet::_process_core_()
{
  this->_update_buffers_();
  // Main computation!
  const int i_start = this->_input_buffer_offset;
  const long num_frames = this->_input_post_gain.size();
  const long i_end = i_start + num_frames;
  //TODO one unnecessary copy :/ #speed
  for (int i = i_start; i < i_end; i++)
    this->_block_vals[0](0, i) = this->_input_buffer[i];
  for (long i = 0; i < this->_blocks.size(); i++)
    this->_blocks[i].process_(this->_block_vals[i], this->_block_vals[i + 1], i_start, i_end);
  // TODO clean up this allocation
  this->_head.process_(
    this->_block_vals[this->_blocks.size()],
    this->_head_output,
    i_start,
    i_end
  );
  // Copy to required output array (TODO tighten this up)
  for (int s = 0; s < num_frames; s++)
    this->_core_dsp_output[s] = this->_head_output(s);
  // Apply anti-pop
  this->_anti_pop_();
}

void convnet::ConvNet::_verify_params(
  const int channels,
  const std::vector<int> &dilations ,
  const bool batchnorm,
  const int actual_params
)
{
  // TODO
}

void convnet::ConvNet::_update_buffers_()
{
  this->Buffer::_update_buffers_();
  const long buffer_size = this->_input_buffer.size();
  this->_block_vals[0].resize(1, buffer_size);
  for (long i = 1; i < this->_block_vals.size(); i++)
    this->_block_vals[i].resize(this->_blocks[i-1].get_out_channels(), buffer_size);
}

void convnet::ConvNet::_rewind_buffers_()
{
  //Need to rewind the block vals first because Buffer::rewind_buffers()
  //resets the offset index
  //The last _block_vals is the output of the last block and doesn't need to be
  //rewound.
  for (long k = 0; k < this->_block_vals.size()-1; k++) {
    //We actually don't need to pull back a lot...just as far as the first input sample would
    //grab from dilation
    const long _dilation = this->_blocks[k].conv.get_dilation();
    for (
      long i = this->_receptive_field - _dilation, j = this->_input_buffer_offset - _dilation;
      j < this->_input_buffer_offset;
      i++, j++
    )
      for (long r=0; r<this->_block_vals[k].rows(); r++)
        this->_block_vals[k](r, i) = this->_block_vals[k](r, j);
  }
  // Now we can do the rest of the rewind
  this->Buffer::_rewind_buffers_();
}

void convnet::ConvNet::_anti_pop_()
{
  if (this->_anti_pop_countdown >= this->_anti_pop_ramp)
    return;
  const float slope = 1.0f / float(this->_anti_pop_ramp);
  for (int i = 0; i < this->_core_dsp_output.size(); i++)
  {
    if (this->_anti_pop_countdown >= this->_anti_pop_ramp)
      break;
    const float gain = std::max(slope * float(this->_anti_pop_countdown), float(0.0));
    this->_core_dsp_output[i] *= gain;
    this->_anti_pop_countdown++;
  }
}

void convnet::ConvNet::_reset_anti_pop_()
{
  // You need the "real" receptive field, not the buffers.
  long receptive_field = 1;
  for (int i = 0; i < this->_blocks.size(); i++)
    receptive_field += this->_blocks[i].conv.get_dilation();
  this->_anti_pop_countdown = -receptive_field;
}
