#pragma once

#include "choc_DisableAllWarnings.h"
#include "dsp.h"
#include "choc_ReenableAllWarnings.h"

#include "IPlug_include_in_plug_hdr.h"

#include "ISender.h"

const int kNumPresets = 1;

enum EParams
{
  kInputGain = 0,
  kOutputGain,
  kNumParams
};

enum ECtrlTags
{
  kCtrlTagModelName = 0,
  kCtrlTagMeter,
  kNumCtrlTags
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override { mSender.Reset(GetSampleRate()); }
  void OnIdle() override { mSender.TransmitData(*this); }
  
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  
private:
  void GetDSP(const WDL_String& dspPath);
  void _SetModelMsg(const WDL_String& dspPath);
  bool _HaveModel() const {
      return this->mDSP != nullptr;
  };
  // The DSP actually being used:
  std::unique_ptr<DSP> mDSP;
  // Manages switching what DSP is being used.
  std::unique_ptr<DSP> mStagedDSP;
  
  WDL_String mModelPath;
  
  std::unordered_map<std::string, double> mDSPParams = {
    { "Input", 0.0 },
    { "Output", 0.0 }
  };
  
  iplug::IPeakAvgSender<> mSender;
};
