#pragma once

#include "NeuralAmpModelerCore/NAM/dsp.h"
#include "AudioDSPTools/dsp/ImpulseResponse.h"
#include "AudioDSPTools/dsp/NoiseGate.h"
#include "AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "AudioDSPTools/dsp/dsp.h"
#include "AudioDSPTools/dsp/wav.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

const int kNumPresets = 1;
// The plugin is mono inside
constexpr size_t kNumChannelsInternal = 1;

class NAMSender : public iplug::IPeakAvgSender<>
{
public:
  NAMSender()
  : iplug::IPeakAvgSender<>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f)
  {
  }
};

enum EParams
{
  // These need to be the first ones because I use their indices to place
  // their rects in the GUI.
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  // The rest is fine though.
  kNoiseGateActive,
  kEQActive,
  kOutNorm,
  kIRToggle,
  kNumParams
};

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelFileBrowser = 0,
  kCtrlTagIRFileBrowser,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagAboutBox,
  kCtrlTagOutNorm,
  kCtrlTagSampleRateWarning,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearIR,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedIR,
  kNumMsgTags
};

class ResamplingNAM : public nam::DSP
{
public:
  // Resampling wrapper around the NAM models
  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate)
  : nam::DSP(expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  {
    if (mEncapsulated->HasLoudness())
      SetLoudness(mEncapsulated->GetLoudness());
    // NOTE: prewarm samples doesn't mean anything--we can prewarm the encapsulated model as it likes and be good to go.
  };

  ~ResamplingNAM() = default;

  void prewarm() override { mEncapsulated->prewarm(); };

  void process(NAM_SAMPLE* input, NAM_SAMPLE* output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
        // We can afford to be careful
      throw std::runtime_error("More frames were provided than the max expected!");
    if (GetExpectedSampleRate() == GetEncapsulatedSampleRate()) {
      mEncapsulated->process(input, output, num_frames);
      lastNumEncapsulatedFramesProcessed = num_frames;
    }
    else
    {
      ProcessWithResampling(input, output, num_frames);
    }
    lastNumExternalFramesProcessed = num_frames;
  };

  void finalize_(const int num_frames)
  {
    if (num_frames != lastNumExternalFramesProcessed)
      throw std::runtime_error(
        "finalize_() called on ResamplingNAM with a different number of frames from what was just processed. Something "
        "is probably going wrong.");
    mEncapsulated->finalize_(lastNumEncapsulatedFramesProcessed);

    // Invalidate state
    lastNumEncapsulatedFramesProcessed = -1;
    lastNumExternalFramesProcessed = -1;
  };

  void Reset(const double sampleRate, const int maxBlockSize) {
      mExpectedSampleRate = sampleRate;
      mMaxExternalBlockSize = maxBlockSize;
    ResizeEncapsulatedBuffers();
  };


private:
  // The encapsulated NAM
  std::unique_ptr<nam::DSP> mEncapsulated;

  // Audio block arrays for the encapsulated model to process.
  // Mapping into and out of these is accomplished by the "actual" resampling algorithm
  int mMaxExternalBlockSize = 0;
  std::vector<NAM_SAMPLE> mInput;
  std::vector<NAM_SAMPLE> mOutput;
  // Kepp track of how many frames were processed so that we can be sure that finalize_() is consistent.
  // This is kind of hacky, but I'm not sure I want to rethink the core right now.
  int lastNumExternalFramesProcessed = -1;
  int lastNumEncapsulatedFramesProcessed = -1;

  // The more complicated method where resampling actually happens.
  // It's good to have this separate from the main routine because if resampling isn't necessary then we can trim
  // off some ops associated with work with the encapsulated buffers and such.
  void ProcessWithResampling(NAM_SAMPLE* input, NAM_SAMPLE* output, const int numFrames)
  {
    const int numEncapsulatedFrames = ResampleInput(input, numFrames);
    mEncapsulated->process(mInput.data(), mOutput.data(), numEncapsulatedFrames);
    ResampleOutput(output, numFrames);  // TODO what about encapsulated frames and making sure it all matches up?
    lastNumEncapsulatedFramesProcessed = numEncapsulatedFrames;
  };
  
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  double GetEncapsulatedSampleRate() const {
    const double reportedEncapsulatedSampleRate = mEncapsulated->GetExpectedSampleRate();
    const double encapsulatedSampleRate =
      reportedEncapsulatedSampleRate <= 0.0 ? 48000.0 : reportedEncapsulatedSampleRate;
    return encapsulatedSampleRate;
  };

  void ResizeEncapsulatedBuffers(){
    const auto externalSampleRate = GetExpectedSampleRate();
    const auto encapsulatedSampleRate = GetEncapsulatedSampleRate();
    // FIXMEs:
    // * Integer rounding
    // * Receptive field of the interpolator
    const int maxEncapsulatedBlockSize = mMaxExternalBlockSize;  // FIXME
      //(int)(encapsulatedSampleRate * (double)mMaxExternalBlockSize / externalSampleRate);
    mInput.resize(maxEncapsulatedBlockSize);
    mOutput.resize(maxEncapsulatedBlockSize);
  };

  int ResampleInput(NAM_SAMPLE* input, const int numFrames){
    // TODO
    int numEncapsulatedFrames = numFrames;
    for (int i = 0; i < numEncapsulatedFrames; i++)
      mInput[i] = input[i];
    return numEncapsulatedFrames;
  };

  void ResampleOutput(NAM_SAMPLE* output, const int numFrames)
  {
    // TODO
    for (int i = 0; i < numFrames; i++)
      output[i] = mOutput[i];
  };
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);
  ~NeuralAmpModeler();

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;

  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }

  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void _ApplyDSPStaging();
  // Deallocates mInputPointers and mOutputPointers
  // Check whether the sample rate is correct for the NAM model.
  // Adjust the warning control accordingly.
  void _CheckSampleRateWarning();
  void _DeallocateIOPointers();
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void _FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels, const size_t numFrames);
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  // Apply the normalization for the model output (if possible)
  void _NormalizeModelOutput(iplug::sample** buffer, const size_t numChannels, const size_t numFrames);
  // Loads a NAM model and stores it to mStagedNAM
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile);
  // Loads an IR and stores it to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);

  bool _HaveModel() const { return this->mModel != nullptr; };
  // Prepare the input & output buffers
  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  // :param nChansIn: In from external
  // :param nChansOut: Out to the internal of the DSP routine
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  // Resetting for models and IRs, called by OnReset
  void _ResetModelAndIR(const double sampleRate, const int maxBlockSize);

  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  // Member data

  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  // The model actually being used:
  std::unique_ptr<ResamplingNAM> mModel;
  // And the IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  // Manages switching what DSP is being used.
  std::unique_ptr<ResamplingNAM> mStagedModel;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  // Flags to take away the modules at a safe time.
  std::atomic<bool> mShouldRemoveModel = false;
  std::atomic<bool> mShouldRemoveIR = false;

  std::atomic<bool> mNewModelLoadedInDSP = false;
  // Flag to check whether the playback sample rate is correct for the model being used.
  std::atomic<bool> mCheckSampleRateWarning = true;

  // Tone stack modules
  recursive_linear_filter::LowShelf mToneBass;
  recursive_linear_filter::Peaking mToneMid;
  recursive_linear_filter::HighShelf mToneTreble;

  // Post-IR filters
  recursive_linear_filter::HighPass mHighPass;
  //  recursive_linear_filter::LowPass mLowPass;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  // Path to IR (.wav file)
  WDL_String mIRPath;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  NAMSender mInputSender, mOutputSender;
};
