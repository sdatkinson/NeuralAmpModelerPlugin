#pragma once

#include "choc_DisableAllWarnings.h"
#include "dsp.h"
#include "choc_ReenableAllWarnings.h"
#include "dsp/RecursiveLinearFilter.h"
#include "dsp/ImpulseResponse.h"

#include "IPlug_include_in_plug_hdr.h"

#include "ISender.h"

#include "dsp/wav.h"

const int kNumPresets = 1;

enum EParams
{
  kInputLevel = 0,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  kEQActive,
  kNumParams
};

const int numKnobs = 5;

enum ECtrlTags
{
  kCtrlTagModelName = 0,
  kCtrlTagIRName,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kNumCtrlTags
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override {
    const auto sampleRate = this->GetSampleRate();
    this->mInputSender.Reset(sampleRate);
    this->mOutputSender.Reset(sampleRate);
  }
  void OnIdle() override {
    this->mInputSender.TransmitData(*this);
    this->mOutputSender.TransmitData(*this);
  }
  
  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  
private:
  // Moves DSP modules from staging area to the main area.
  // Also deletes DSP modules that are flagged for removal.
  // Exists so that we don't try to use a DSP module that's only
  // partially-instantiated.
  void _ApplyDSPStaging();
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void _FallbackDSP(const int nFrames);
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  // Gets a new Neural Amp Model object and stores it to mStagedNAM
  void _GetNAM(const WDL_String& dspPath);
  // Gets the IR and stores to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _GetIR(const WDL_String& irPath);
  // Update the message about which model is loaded.
  void _SetModelMsg(const WDL_String& dspPath);
  bool _HaveModel() const {
      return this->mNAM != nullptr;
  };
  // Prepare the input & output buffers
  void _PrepareBuffers(const int nFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  void _ProcessInput(iplug::sample** inputs, const int nFrames);
  // Copy the output to the output buffer, applying output level.
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const int nFrames);
  // Update the text in the IR area to say what's loaded.
  void _SetIRMsg(const WDL_String& irPath);
  void _UnsetModelMsg();
  void _UnsetIRMsg();
  void _UnsetMsg(const int tag, const WDL_String& msg);
  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const int nFrames);

  // Member data
    
  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Pointer versions
  iplug::sample** mInputPointers;
  iplug::sample** mOutputPointers;
  
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
  
  std::unordered_map<std::string, double> mNAMParams = {
    { "Input", 0.0 },
    { "Output", 0.0 }
  };
  
  iplug::IPeakAvgSender<> mInputSender;
  iplug::IPeakAvgSender<> mOutputSender;
};
