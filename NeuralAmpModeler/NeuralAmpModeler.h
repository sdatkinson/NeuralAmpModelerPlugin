#pragma once

#include <array>
#include <atomic>
#include <mutex>

#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "ResamplingNAM.h"

#include "Colors.h"
#include "ToneStack.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"


const int kNumPresets = 1;
// Both slots take mono input and may produce mono or stereo output.
constexpr size_t kMaxNumChannelsInternal = 2;

template <int Channels>
class NAMSender : public iplug::IPeakAvgSender<Channels>
{
public:
  NAMSender()
  : iplug::IPeakAvgSender<Channels>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f)
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
  static constexpr int kMaxInternalBlockSize = 2048;

  // Applies pending module and parameter changes at the start of a host block.
  void _ApplyDSPStaging();
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void _FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels, const size_t numFrames);
  void _InitToneStack();
  // Loads a NAM model and stores it to mStagedModel.
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile);
  // Loads a WAV or NAM cab and stores it to mStagedCab.
  // Returns an empty string on success, or an error message on failure.
  std::string _StageCab(const WDL_String& cabPath);

  void _ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames, size_t numChannelsExternalIn,
                     size_t numChannelsExternalOut);
  // Mix the host inputs to mono and apply input gain.
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  // Resetting for the model and cab, called by OnReset
  void _ResetModelAndCab(const double sampleRate);

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
                     const size_t nChansOut);

  // Member data

  using SampleBuffer = std::array<iplug::sample, kMaxInternalBlockSize>;
  SampleBuffer mInputBuffer{};
  std::array<SampleBuffer, kMaxNumChannelsInternal> mModelOutput{};
  // Separate cab output avoids requiring NAM architectures to support in-place processing.
  std::array<SampleBuffer, kMaxNumChannelsInternal> mCabinetOutput{};
  SampleBuffer mCabinetInput{};

  // Input and output gain
  double mInputGain = 1.0;
  double mOutputGain = 1.0;
  bool mCabEnabled = true;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger; // Mono input to the trigger
  dsp::noise_gate::Gain mNoiseGateGain; // Gain reduction applied to left/mono output of the amp...
  dsp::noise_gate::Gain mNoiseGateGainRight; // ...and the right output, if applicable.
  // The model actually being used:
  std::unique_ptr<ResamplingNAM> mModel;
  struct Cabinet
  {
    std::unique_ptr<ResamplingNAM> model;
    // Independent state for a mono cab processing the right side of a stereo amp.
    std::unique_ptr<ResamplingNAM> right;
    bool isWav = false;

    void Reset(double sampleRate);
  };
  Cabinet mCab;

  // Mutex to prevent the staging routines (_StageModel(), _StageCab()) from changing what's in the "mStaged{Model,Cab}"
  // members while _ApplyDSPStaging() is getting from them.
  mutable std::mutex mDSPStagingMutex;
  std::atomic<double> mDSPStagingSampleRate{iplug::DEFAULT_SAMPLE_RATE};
  std::unique_ptr<ResamplingNAM> mStagedModel;
  Cabinet mStagedCab;
  bool mShouldRemoveModel = false;
  bool mShouldRemoveCab = false;

  std::atomic<bool> mDSPGainsAndTimingDirty{true};
  std::atomic<bool> mToneStackDirty{true};
  std::atomic<bool> mSlimDirty{false};
  std::atomic<bool> mNewModelLoadedInDSP = false;
  std::atomic<bool> mModelCleared = false;

  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;

  // Post-cab filters
  recursive_linear_filter::HighPass mHighPass;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  // Path to the cab (.wav or .nam file). Keep IRPath in serialized state for compatibility.
  WDL_String mCabPath;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  NAMSender<1> mInputSender; // Mono input
  NAMSender<2> mOutputSender; // Stereo output
};
