#pragma once

#include "NeuralAmpModelerCore/NAM/dsp.h"
#include "AudioDSPTools/dsp/ImpulseResponse.h"
#include "AudioDSPTools/dsp/NoiseGate.h"
#include "AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "AudioDSPTools/dsp/dsp.h"
#include "AudioDSPTools/dsp/wav.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#include "NeuralAmpModelerFileManager.h"
#include "NonIntegerResampler.h"

const int kNumPresets = 1;


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
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void _ApplyDSPStaging();
  // Deallocates mInputPointers and mOutputPointers
  // Check whether the sample rate is correct for the NAM model.
  // Adjust the warning control accordingly.
  void _CheckSampleRateWarning();

  // Loads a NAM model and stores it to mStagedNAM
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile);
  // Loads an IR and stores it to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);

  bool _HaveModel() const { return this->mModel != nullptr; };

  // Member data

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  // The model actually being used:
  std::unique_ptr<nam::DSP> mModel;
  // And the IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  // Manages switching what DSP is being used.
  std::unique_ptr<nam::DSP> mStagedModel;
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

  NAMSender mInputSender, mOutputSender;
  FileManager mModelFileManager {"nam"}, mIRFileManager {"wav"};
  iplug::NonIntegerResampler<iplug::sample, 1> mNonIntegerSRC {48000.0f, iplug::ESRCMode::kLancsoz};
};
