#pragma once

#include "choc_DisableAllWarnings.h"
#include "dsp.h"
#include "choc_ReenableAllWarnings.h"

#include "IPlug_include_in_plug_hdr.h"

#include "ISender.h"

const int kNumPresets = 1;

enum EParams
{
  kInputLevel = 0,
  kOutputLevel,
  kNumParams
};

enum ECtrlTags
{
  kCtrlTagModelName = 0,
//  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kNumCtrlTags
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override {
//    this->mInputSender.Reset(GetSampleRate());
    this->mOutputSender.Reset(GetSampleRate());
  }
  void OnIdle() override {
//    this->mInputSender.TransmitData(*this);
    this->mOutputSender.TransmitData(*this);
  }
  
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  
private:
  // Fallback that just copies inputs to outputs if mDSP doesn't hold a model.
  void _FallbackDSP(const int nFrames);
  // Gets a new DSP object and stores it to mStagedDSP
  void _GetDSP(const WDL_String& dspPath);
  // Update the message about which model is loaded.
  void _SetModelMsg(const WDL_String& dspPath);
  bool _HaveModel() const {
      return this->mDSP != nullptr;
  };
  // Prepare the input & output buffers
  void _PrepareBuffers(const int nFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  void _ProcessInput(iplug::sample** inputs, const int nFrames);
  // Copy the output to the output buffer, applying output level.
  void _ProcessOutput(iplug::sample** outputs, const int nFrames);
  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(sample** inputPointer, sample** outputPointer, const int nFrames);
    
  // Input arrays
  std::vector<std::vector<sample>> mInputArray;
  // Output arrays
  std::vector<std::vector<sample>> mOutputArray;
  // Pointer versions
  sample** mInputPointers;
  sample** mOutputPointers;
  size_t mNumChannels;
  size_t mNumFrames;
  
  // The DSP actually being used:
  std::unique_ptr<DSP> mDSP;
  // Manages switching what DSP is being used.
  std::unique_ptr<DSP> mStagedDSP;
  
  WDL_String mModelPath;
  
  std::unordered_map<std::string, double> mDSPParams = {
    { "Input", 0.0 },
    { "Output", 0.0 }
  };
  
//  iplug::IPeakAvgSender<> mInputSender;
  iplug::IPeakAvgSender<> mOutputSender;  // Do I need separate ones?...
};
