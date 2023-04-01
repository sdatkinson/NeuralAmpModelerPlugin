#pragma once

#include "NeuralAmpModelerCore/dsp/ImpulseResponse.h"
#include "NeuralAmpModelerCore/dsp/NoiseGate.h"
#include "NeuralAmpModelerCore/dsp/RecursiveLinearFilter.h"
#include "NeuralAmpModelerCore/dsp/dsp.h"
#include "NeuralAmpModelerCore/dsp/wav.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

using namespace iplug;
using namespace igraphics;

const int kNumPresets = 1;

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
  kNumParams
};

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelName = 0,
  kCtrlTagIRName,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagAboutBox,
  kCtrlTagWebView,
  kNumCtrlTags
};

class NeuralAmpModeler final : public Plugin
{
public:
  NeuralAmpModeler(const InstanceInfo& info);
  ~NeuralAmpModeler();

  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }
  void OnParamChangeUI(int paramIdx, EParamSource source) override;
  IGraphics* CreateGraphics() override;
  void LayoutUI(IGraphics* pGraphics) override;

private:
  // Allocates mInputPointers and mOutputPointers
  void AllocateIOPointers(const size_t nChans);
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void ApplyDSPStaging();
  // Deallocates mInputPointers and mOutputPointers
  void DeallocateIOPointers();
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void FallbackDSP(sample** inputs, sample** outputs, const size_t numChannels, const size_t numFrames);
  // Sizes based on mInputArray
  size_t GetBufferNumChannels() const;
  size_t GetBufferNumFrames() const;
  // Gets a new Neural Amp Model object and stores it to mStagedNAM
  // Returns an emptry string on success, or an error message on failure.
  std::string GetNAM(const WDL_String& dspFile);
  // Gets the IR and stores to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode GetIR(const WDL_String& irPath);
  // Update the message about which model is loaded.
  void SetModelMsg(const WDL_String& dspPath);
  bool HaveModel() const { return mNAM != nullptr; };
  // Prepare the input & output buffers
  void PrepareBuffers(const size_t numChannels, const size_t numFrames);
  // Manage pointers
  void PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  // :param nChansIn: In from external
  // :param nChansOut: Out to the internal of the DSP routine
  void ProcessInput(sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void ProcessOutput(sample** inputs, sample** outputs, const size_t nFrames, const size_t nChansIn,
                     const size_t nChansOut);
  // Update the text in the IR area to say what's loaded.
  void SetIRMsg(const WDL_String& irPath);
  void UnsetModelMsg();
  void UnsetIRMsg();
  void UnsetMsg(const int tag, const WDL_String& msg);
  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames, const size_t nChansIn,
                    const size_t nChansOut);

  // Member data

  // The plugin is mono inside
  const size_t mNUM_INTERNAL_CHANNELS = 1;

  // Input arrays to NAM
  std::vector<std::vector<sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<sample>> mOutputArray;
  // Pointer versions
  sample** mInputPointers;
  sample** mOutputPointers;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  // The Neural Amp Model (NAM) actually being used:
  std::unique_ptr<DSP> mNAM;
  // And the IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  // Manages switching what DSP is being used.
  std::unique_ptr<DSP> mStagedNAM;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  // Flags to take away the modules at a safe time.
  bool mFlagRemoveNAM;
  bool mFlagRemoveIR;
  const WDL_String mDefaultNAMString;
  const WDL_String mDefaultIRString;

  // Tone stack modules
  recursive_linear_filter::LowShelf mToneBass;
  recursive_linear_filter::Peaking mToneMid;
  recursive_linear_filter::HighShelf mToneTreble;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  // Path to IR (.wav file)
  WDL_String mIRPath;

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  IPeakAvgSender<> mInputSender;
  IPeakAvgSender<> mOutputSender;
};
