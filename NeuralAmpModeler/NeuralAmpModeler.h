#pragma once

#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "ResamplingNAM.h"

#include "Colors.h"
#include "ToneStack.h"

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
  kCabToggle,
  // Input calibration
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kSlim,
  kNumParams
};

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelFileBrowser = 0,
  kCtrlTagCabFileBrowser,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kCtrlTagOutputMode,
  kCtrlTagCalibrateInput,
  kCtrlTagInputCalibrationLevel,
  kCtrlTagSlimmableIcon,
  kCtrlTagSlimOverlayBackdrop,
  kCtrlTagSlimKnob,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearCab,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedCab,
  kNumMsgTags
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

  void OnParamChange(int paramIdx) override;
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
  void _DeallocateIOPointers();
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void _FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels, const size_t numFrames);
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  void _InitToneStack();
  // Loads a NAM model and stores it to mStagedNAM
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile);
  // Loads a WAV or NAM cab and stores it to mStagedCab.
  // Returns an empty string on success, or an error message on failure.
  std::string _StageCab(const WDL_String& cabPath);

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
  // Resetting for the model and cab, called by OnReset
  void _ResetModelAndCab(const double sampleRate, const int maxBlockSize);

  void _SetInputGain();
  void _SetOutputGain();
  void _ApplySlimParamToLoadedNAMs();

  // See: Unserialization.cpp
  void _UnserializeApplyConfig(nlohmann::json& config);
  // 0.7.9 and later
  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  // Hopefully 0.7.3-0.7.8, but no gurantees
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);

  // Update all controls that depend on a model
  void _UpdateControlsFromModel();

  // Report latency and tail length to the host.
  void _UpdateLatencyAndTail();

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
  // Separate cab output avoids requiring NAM architectures to support in-place processing.
  std::vector<iplug::sample> mCabinetOutput;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  // Input and output gain
  double mInputGain = 1.0;
  double mOutputGain = 1.0;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  // The model actually being used:
  std::unique_ptr<ResamplingNAM> mModel;
  // The cabinet, loaded from a WAV impulse response or NAM model.
  std::unique_ptr<ResamplingNAM> mCab;
  bool mCabIsWav = false;
  // Manages switching what DSP is being used.
  std::unique_ptr<ResamplingNAM> mStagedModel;
  std::unique_ptr<ResamplingNAM> mStagedCab;
  bool mStagedCabIsWav = false;
  // Flags to take away the modules at a safe time.
  std::atomic<bool> mShouldRemoveModel = false;
  std::atomic<bool> mShouldRemoveCab = false;

  std::atomic<bool> mNewModelLoadedInDSP = false;
  std::atomic<bool> mModelCleared = false;

  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;

  // Post-cab filters
  recursive_linear_filter::HighPass mHighPass;
  //  recursive_linear_filter::LowPass mLowPass;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  // Path to the cab (.wav or .nam file). Keep IRPath in serialized state for compatibility.
  WDL_String mCabPath;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  NAMSender mInputSender, mOutputSender;
};
