#pragma once

#if IPLUG_DSP

#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <Eigen/Dense>
#include "IPlugConstants.h"

enum EArchitectures
{
  kLinear = 0,
  kConvNet,
  kLSTM,
  kCatLSTM,
  kWaveNet,
  kCatWaveNet,
  kNumModels
};

// Class for providing params from the plugin to the DSP module
// For now, we'll work with doubles. Later, we'll add other types.
class DSPParam
{
public:
  const char* name;
  const double val;
};
// And the params shall be provided as a std::vector<DSPParam>.

class DSP
{
public:
  DSP();
  // process() does all of the processing requried to take `inputs` array and
  // fill in the required values on `outputs`.
  // To do this:
  // 1. The parameters from the plugin (I/O levels and any other parametric
  //    inputs) are gotten.
  // 2. The input level is applied
  // 3. The core DSP algorithm is run (This is what should probably be
  //    overridden in subclasses).
  // 4. The output level is applied and the result stored to `output`.
  virtual void process(
    iplug::sample** inputs,
    iplug::sample** outputs,
    const int num_channels,
    const int num_frames,
    const double input_gain,
    const double output_gain,
    const std::unordered_map<std::string, double>& params
  );
  // Anything to take care of before next buffer comes in.
  // For example:
  // * Move the buffer index forward
  // * Does NOT say that params aren't stale; that's the job of the routine
  //   that actually uses them, which varies depends on the particulars of the
  //   DSP subclass implementation.
  virtual void finalize_(const int num_frames);

protected:
  // Parameters (aka "knobs")
  std::unordered_map<std::string, double> _params;
  // If the params have changed since the last buffer was processed:
  bool _stale_params;
  // Where to store the samples after applying input gain
  std::vector<float> _input_post_gain;
  // Location for the output of the core DSP algorithm.
  std::vector<float> _core_dsp_output;

  // Methods

  // Copy the parameters to the DSP module.
  // If anything has changed, then set this->_stale_params to true.
  // (TODO use "listener" approach)
  void _get_params_(const std::unordered_map<std::string, double>& input_params);

  // Apply the input gain
  // Result populates this->_input_post_gain
  void _apply_input_level_(iplug::sample** inputs,
                           const int num_channels,
                           const int num_frames,
                           const double gain);

  // i.e. ensure the size is correct.
  void _ensure_core_dsp_output_ready_();

  // The core of your DSP algorithm.
  // Access the inputs in this->_input_post_gain
  // Place the outputs in this->_core_dsp_output
  virtual void _process_core_();

  // Copy this->_core_dsp_output to output and apply the output volume
  void _apply_output_level_(iplug::sample** outputs,
                            const int num_channels,
                            const int num_frames,
                            const double gain);
};

// Class where an input buffer is kept so that long-time effects can be captured.
// (e.g. conv nets or impulse responses, where we need history that's longer than the
// sample buffer that's coming in.)
class Buffer : public DSP
{
public:
  Buffer(const int receptive_field);
  void finalize_(const int num_frames);
protected:
  // Input buffer
  const int _input_buffer_channels = 1;  // Mono
  int _receptive_field;
  // First location where we add new samples from the input
  long _input_buffer_offset;
  std::vector<float> _input_buffer;
  std::vector<float> _output_buffer;

  void _set_receptive_field(const int new_receptive_field, const int input_buffer_size);
  void _set_receptive_field(const int new_receptive_field);
  void _reset_input_buffer();
  // Use this->_input_post_gain
  virtual void _update_buffers_();
  virtual void _rewind_buffers_();
};

// Basic linear model (an IR!)
class Linear : public Buffer
{
public:
  Linear(const int receptive_field, const bool _bias, const std::vector<float> &params);
  void _process_core_() override;
protected:
  Eigen::VectorXf _weight;
  float _bias;
};

// NN modules =================================================================

// Activations

// In-place ReLU on (N,M) array
void relu_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end,
  const long j_start,
  const long j_end
);
// Subset of the columns
void relu_(
  Eigen::MatrixXf& x,
  const long j_start,
  const long j_end
);
void relu_(Eigen::MatrixXf& x);

// In-place sigmoid
void sigmoid_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end,
  const long j_start,
  const long j_end
);
void sigmoid_(Eigen::MatrixXf& x);

// In-place Tanh on (N,M) array
void tanh_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end,
  const long j_start,
  const long j_end
);
// Subset of the columns
void tanh_(
  Eigen::MatrixXf& x,
  const long i_start,
  const long i_end
);

void tanh_(Eigen::MatrixXf& x);

class Conv1D
{
public:
  Conv1D() { this->_dilation = 1; };
  void set_params_(std::vector<float>::iterator& params);
  void set_size_(
    const int in_channels,
    const int out_channels,
    const int kernel_size,
    const bool do_bias,
    const int _dilation
  );
  void set_size_and_params_(
    const int in_channels,
    const int out_channels,
    const int kernel_size,
    const int _dilation,
    const bool do_bias,
    std::vector<float>::iterator& params
  );
  //Process from input to output
  // Rightmost indices of input go from i_start to i_end,
  // Indices on output for from j_start (to j_start + i_end - i_start)
  void process_(
    const Eigen::MatrixXf& input,
    Eigen::MatrixXf& output,
    const long i_start,
    const long i_end,
    const long j_start
  ) const;
  long get_in_channels() const { return this->_weight.size() > 0 ? this->_weight[0].cols() : 0; };
  long get_kernel_size() const { return this->_weight.size(); };
  long get_num_params() const;
  long get_out_channels() const {return this->_weight.size() > 0 ? this->_weight[0].rows() : 0;};
  int get_dilation() const { return this->_dilation; };
private:
  // Gonna wing this...
  // conv[kernel](cout, cin)
  std::vector<Eigen::MatrixXf> _weight;
  Eigen::VectorXf _bias;
  int _dilation;
};

// Really just a linear layer
class Conv1x1 {
public:
  Conv1x1(
    const int in_channels,
    const int out_channels,
    const bool _bias
  );
  void set_params_(std::vector<float>::iterator& params);
  // :param input: (N,Cin) or (Cin,)
  // :return: (N,Cout) or (Cout,), respectively
  Eigen::MatrixXf process(const Eigen::MatrixXf& input) const;

  int get_out_channels() const { return this->_weight.rows(); };
private:
  Eigen::MatrixXf _weight;
  Eigen::VectorXf _bias;
  bool _do_bias;
};

// ConvNet ====================================================================

namespace convnet {
  // Custom Conv that avoids re-computing on pieces of the input and trusts
  // that the corresponding outputs are where they need to be.
  // Beware: this is clever!
  

  // Batch normalization
  // In prod mode, so really just an elementwise affine layer.
  class BatchNorm
  {
  public:
    BatchNorm() {};
    BatchNorm(const int dim, std::vector<float>::iterator& params);
    void process_(
      Eigen::MatrixXf& input,
      const long i_start,
      const long i_end
    ) const;

  private:
    // TODO simplify to just ax+b
    // y = (x-m)/sqrt(v+eps) * w + bias
    // y = ax+b
    // a = w / sqrt(v+eps)
    // b = a * m + bias
    Eigen::VectorXf scale;
    Eigen::VectorXf loc;
  };

  class ConvNetBlock
  {
  public:
    ConvNetBlock() { this->_batchnorm = false; };
    void set_params_(
      const int in_channels,
      const int out_channels,
      const int _dilation,
      const bool batchnorm,
      const std::string activation,
      std::vector<float>::iterator& params
    );
    void process_(
      const Eigen::MatrixXf& input,
      Eigen::MatrixXf &output,
      const long i_start,
      const long i_end
    ) const;
    int get_out_channels() const;
    Conv1D conv;
  private:
    BatchNorm batchnorm;
    bool _batchnorm;
    std::string activation;
  };

  class _Head
  {
  public:
    _Head() { this->_bias = (float)0.0; };
    _Head(const int channels, std::vector<float>::iterator& params);
    void process_(
      const Eigen::MatrixXf &input,
      Eigen::VectorXf &output,
      const long i_start,
      const long i_end
    ) const;
  private:
    Eigen::VectorXf _weight;
    float _bias;
  };

  class ConvNet : public Buffer
  {
  public:
    ConvNet(
      const int channels,
      const std::vector<int>& dilations,
      const bool batchnorm,
      const std::string activation,
      std::vector<float> &params
    );
  protected:
    std::vector<ConvNetBlock> _blocks;
    std::vector<Eigen::MatrixXf> _block_vals;
    Eigen::VectorXf _head_output;
    _Head _head;
    void _verify_params(
      const int channels,
      const std::vector<int> &dilations,
      const bool batchnorm,
      const int actual_params
    );
    void _update_buffers_() override;
    void _rewind_buffers_() override;

    void _process_core_() override;

    // The net starts with random parameters inside; we need to wait for a full
    // receptive field to pass through before we can count on the output being
    // ok. This implements a gentle "ramp-up" so that there's no "pop" at the
    // start.
    long _anti_pop_countdown;
    const long _anti_pop_ramp = 100;
    void _anti_pop_();
    void _reset_anti_pop_();
  };
};  // namespace convnet

// Utilities ==================================================================
// Implemented in get_dsp.cpp

// Verify that the config that we are building our model from is supported by
// this plugin version.
void verify_config_version(const std::string version);

// Takes the model file and uses it to instantiate an instance of DSP.
std::unique_ptr<DSP> get_dsp(const std::filesystem::path model_file);
// Legacy loader for directory-type DSPs
std::unique_ptr<DSP> get_dsp_legacy(const std::filesystem::path dirname);

// Hard-coded model:
std::unique_ptr<DSP> get_hard_dsp();

// Version 2 DSP abstraction ==================================================

namespace dsp {
  class Params {};
  
  class DSP {
  public:
    DSP();
    ~DSP();
    // The main interface for processing audio.
    // The incoming audio is given as a raw pointer-to-pointers.
    // The indexing is [channel][frame].
    // The output shall be a pointer-to-pointers of matching size.
    // This object instance will own the data referenced by the pointers and be
    // responsible for its allocation and deallocation.
    virtual iplug::sample** Process(iplug::sample** inputs, const size_t numChannels, const size_t numFrames) = 0;
    // Update the parameters of the DSP object according to the provided params.
    // Not declaring a pure virtual bc there's no concrete definition that can
    // use Params.
    // But, use this name :)
    // virtual void SetParams(Params* params) = 0;
    
  protected:
    // Methods
    
    // Allocate mOutputPointers.
    // Assumes it's already null (Use _DeallocateOutputPointers()).
    void _AllocateOutputPointers(const size_t numChannels);
    // Ensure mOutputPointers is freed.
    void _DeallocateOutputPointers();
    
    size_t _GetNumChannels() const {return this->mOutputs.size();};
    // Return a pointer-to-pointers for the DSP's output buffers (all channels)
    // Assumes that ._PrepareBuffers()  was called recently enough.
    iplug::sample** _GetPointers();
    // Resize mOutputs to (numChannels, numFrames) and ensure that the raw
    // pointers are also keeping up.
    virtual void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
    // Resize the pointer-to-pointers for the vector-of-vectors.
    void _ResizePointers(const size_t numChannels);
    
    
    // Attributes
    
    // The output array into which the DSP module's calculations will be written.
    // Pointers to this member's data will be returned by .Process(), and std
    // Will ensure proper allocation.
    std::vector<std::vector<iplug::sample>> mOutputs;
    // A pointer to pointers of which copies will be given out as the output of .Process().
    // This object will ensure proper allocation and deallocation of the first level;
    // The second level points to .data() from mOutputs.
    iplug::sample** mOutputPointers;
    size_t mOutputPointersSize;
  };
  
  // A class where a longer buffer of history is needed to correctly calculate
  // the DSP algorithm (e.g. algorithms involving convolution).
  //
  // Hacky stuff:
  // * Mono
  // * Single-precision floats.
  class History : public DSP {
  public:
    History();
  protected:
    // Called at the end of the DSP, advance the hsitory index to the next open
    // spot.  Does not ensure that it's at a valid address.
    void _AdvanceHistoryIndex(const size_t bufferSize);
    // Drop the new samples into the history array.
    // Manages history array size
    void _UpdateHistory(iplug::sample** inputs,
                        const size_t numChannels,
                        const size_t numFrames);
    
    // The history array that's used for DSP calculations.
    std::vector<float> mHistory;
    // How many samples previous are required.
    // Zero means that no history is required--only the current sample.
    size_t mHistoryRequired;
    // Location of the first sample in the current buffer.
    // Shall always be in the range [mHistoryRequired, mHistory.size()).
    size_t mHistoryIndex;
    
  private:
    // Make sure that the history array is long enough.
    void _EnsureHistorySize(const size_t bufferSize);
    // Copy the end of the history back to the fron and reset mHistoryIndex
    void _RewindHistory();
  };
};

#endif  // IPLUG_DSP
