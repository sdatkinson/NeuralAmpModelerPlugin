#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

#include "Colors.h"
#include "ToneStack.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#include <algorithm> // std::fill, std::max, std::min
#include <array>
#include <vector>

const int kNumPresets = 1;
// The plugin is mono inside
constexpr size_t kNumChannelsInternal = 1;
// How many NAM models can be blended together at once.
// Slot 0 is the "primary" one: it's the model browser on the main page, and it's the one that the input/output
// calibration is referenced to, so that a session using a single model behaves exactly as it did before blending
// existed.
constexpr size_t kNumModelSlots = 3;
// Blend level at or below which a slot is considered muted.
constexpr double kBlendMuteDB = -40.0;
// Capacity of the per-slot time-alignment delay lines. Comfortably above any latency a resampler will report, so that
// the delay can be changed from the audio thread without reallocating.
constexpr size_t kMaxSlotDelaySamples = 8192;

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
  kIRToggle,
  // Input calibration
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kSlim,
  // Model blending. One level and one polarity invert per model slot.
  // These are last so that the indices above stay put; see the comment at the top of the enum.
  kBlendLevel1,
  kBlendLevel2,
  kBlendLevel3,
  kBlendInvert1,
  kBlendInvert2,
  kBlendInvert3,
  kBlendMute1,
  kBlendMute2,
  kBlendMute3,
  kBlendSolo1,
  kBlendSolo2,
  kBlendSolo3,
  kNumParams
};

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelFileBrowser = 0,
  kCtrlTagIRFileBrowser,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kCtrlTagOutputMode,
  kCtrlTagCalibrateInput,
  kCtrlTagInputCalibrationLevel,
  kCtrlTagSlimmableIcon,
  kCtrlTagSlimOverlayBackdrop,
  kCtrlTagSlimKnob,
  kCtrlTagBlendIcon,
  kCtrlTagBlendPage,
  // Model browsers living on the blend page. Slot 0 has two browsers (this one and the main page's), which is why it
  // gets its own tag here.
  kCtrlTagBlendModelFileBrowser1,
  kCtrlTagBlendModelFileBrowser2,
  kCtrlTagBlendModelFileBrowser3,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearModel2,
  kMsgTagClearModel3,
  kMsgTagClearIR,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedIR,
  // Echoed back to every browser bound to a slot once the model has actually been removed, so that the browser that
  // didn't initiate the clear also resets itself.
  kMsgTagModelCleared,
  // Hands an empty slot's browser the folder that another slot just loaded from, so its arrows work without having
  // to pick the same folder again. Carries a file path; the browser takes the directory and stays "empty".
  kMsgTagSeedFolder,
  kNumMsgTags
};

// Get the sample rate of a NAM model.
// Sometimes, the model doesn't know its own sample rate; this wrapper guesses 48k based on the way that most
// people have used NAM in the past.
double GetNAMSampleRate(const std::unique_ptr<nam::DSP>& model)
{
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  const double assumedSampleRate = 48000.0;
  const double reportedEncapsulatedSampleRate = model->GetExpectedSampleRate();
  const double encapsulatedSampleRate =
    reportedEncapsulatedSampleRate <= 0.0 ? assumedSampleRate : reportedEncapsulatedSampleRate;
  return encapsulatedSampleRate;
};

class ResamplingNAM : public nam::DSP
{
public:
  // Resampling wrapper around the NAM models
  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate)
  : nam::DSP(encapsulated->NumInputChannels(), encapsulated->NumOutputChannels(), expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  , mResampler(GetNAMSampleRate(mEncapsulated))
  {
    // Assign the encapsulated object's processing function  to this object's member so that the resampler can use it:
    auto ProcessBlockFunc = [&](NAM_SAMPLE** input, NAM_SAMPLE** output, int numFrames) {
      mEncapsulated->process(input, output, numFrames);
    };
    mBlockProcessFunc = ProcessBlockFunc;

    // Get the other information from the encapsulated NAM so that we can tell the outside world about what we're
    // holding.
    if (mEncapsulated->HasLoudness())
    {
      SetLoudness(mEncapsulated->GetLoudness());
    }
    if (mEncapsulated->HasInputLevel())
    {
      SetInputLevel(mEncapsulated->GetInputLevel());
    }
    if (mEncapsulated->HasOutputLevel())
    {
      SetOutputLevel(mEncapsulated->GetOutputLevel());
    }

    // NOTE: prewarm samples doesn't mean anything--we can prewarm the encapsulated model as it likes and be good to
    // go.
    // _prewarm_samples = 0;

    // And be ready
    int maxBlockSize = 2048; // Conservative
    Reset(expected_sample_rate, maxBlockSize);
  };

  ~ResamplingNAM() = default;

  void prewarm() override { mEncapsulated->prewarm(); };

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
      // We can afford to be careful
      throw std::runtime_error("More frames were provided than the max expected!");

    if (!NeedToResample())
    {
      mEncapsulated->process(input, output, num_frames);
    }
    else
    {
      mResampler.ProcessBlock(input, output, num_frames, mBlockProcessFunc);
    }
  };

  int GetLatency() const { return NeedToResample() ? mResampler.GetLatency() : 0; };

  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    mExpectedSampleRate = sampleRate;
    mMaxExternalBlockSize = maxBlockSize;
    mResampler.Reset(sampleRate, maxBlockSize);

    // Allocations in the encapsulated model (HACK)
    // Stolen some code from the resampler; it'd be nice to have these exposed as methods? :)
    const double mUpRatio = sampleRate / GetEncapsulatedSampleRate();
    const auto maxEncapsulatedBlockSize = static_cast<int>(std::ceil(static_cast<double>(maxBlockSize) / mUpRatio));
    mEncapsulated->ResetAndPrewarm(sampleRate, maxEncapsulatedBlockSize);
  };

  // So that we can let the world know if we're resampling (useful for debugging)
  double GetEncapsulatedSampleRate() const { return GetNAMSampleRate(mEncapsulated); };

  nam::SlimmableModel* GetSlimmableModel() { return dynamic_cast<nam::SlimmableModel*>(mEncapsulated.get()); }
  const nam::SlimmableModel* GetSlimmableModel() const
  {
    return dynamic_cast<const nam::SlimmableModel*>(mEncapsulated.get());
  }

private:
  bool NeedToResample() const { return GetExpectedSampleRate() != GetEncapsulatedSampleRate(); };
  // The encapsulated NAM
  std::unique_ptr<nam::DSP> mEncapsulated;

  // The resampling wrapper
  dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> mResampler;

  // Used to check that we don't get too large a block to process.
  int mMaxExternalBlockSize = 0;

  // This function is defined to conform to the interface expected by the iPlug2 resampler.
  std::function<void(NAM_SAMPLE**, NAM_SAMPLE**, int)> mBlockProcessFunc;
};

// Fixed-capacity delay line used to time-align model slots against each other.
//
// Models don't all report the same latency: ResamplingNAM only introduces latency when it has to resample, so blending
// (say) a 44.1k model with a 48k one at a 96k session would sum two signals that are offset in time, which comb-filters
// exactly the way two badly-placed microphones do. Every slot is therefore delayed by (max latency - its own latency).
//
// The buffer is allocated once, in OnReset. Changing the delay only moves the read offset, so the latency update that
// happens inside _ApplyDSPStaging -- on the audio thread -- never allocates.
class SlotDelay
{
public:
  void Resize(const size_t capacity)
  {
    mBuffer.resize(std::max(capacity, (size_t)1));
    Clear();
  };

  void SetDelay(const int numSamples)
  {
    const int clamped = std::max(0, std::min(numSamples, (int)mBuffer.size() - 1));
    if (clamped == mDelay)
    {
      return;
    }
    mDelay = clamped;
    Clear();
  };

  void Clear()
  {
    std::fill(mBuffer.begin(), mBuffer.end(), 0.0);
    mWriteIndex = 0;
  };

  // Delay in place.
  void Process(iplug::sample* samples, const int numFrames)
  {
    if (mDelay == 0)
    {
      return;
    }
    const int size = (int)mBuffer.size();
    for (int s = 0; s < numFrames; s++)
    {
      int readIndex = mWriteIndex - mDelay;
      if (readIndex < 0)
      {
        readIndex += size;
      }
      mBuffer[mWriteIndex] = samples[s];
      samples[s] = mBuffer[readIndex];
      if (++mWriteIndex >= size)
      {
        mWriteIndex = 0;
      }
    }
  };

private:
  std::vector<iplug::sample> mBuffer;
  int mWriteIndex = 0;
  int mDelay = 0;
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
  // Run every loaded model on the (gated) input and sum them into mOutputArray, applying each slot's trims, blend
  // level and polarity. Falls back to a passthrough if no slot holds a model.
  void _BlendModels(iplug::sample** triggerOutput, const size_t numFrames);
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  void _InitToneStack();
  // Loads a NAM model and stores it to mStagedModel[slot]
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile, const size_t slot);
  // Loads an IR and stores it to mStagedIR.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);

  // The lowest-numbered slot that currently holds a model, or -1 if there are none.
  // Everything that used to read the one and only model (calibration, model info, ...) is referenced to this slot.
  int _ReferenceSlot() const;
  bool _HaveModel() const { return _ReferenceSlot() >= 0; };
  // Send a message to every model browser bound to a slot. Slot 0 has two of them.
  void _SendToSlotBrowsers(const size_t slot, const int msgTag, const int dataSize = 0, const void* pData = nullptr);
  // Offer a just-loaded model's folder to any slot that's still empty, so that stepping through a folder of captures
  // only costs one trip to the file dialog.
  void _SeedFolderIntoEmptySlots(const WDL_String& modelPath, const size_t sourceSlot);
  bool _AnyModelIsSlimmable() const;
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

  void _SetInputGain();
  void _SetOutputGain();
  // Per-slot corrections that put the non-reference slots at the same level as the reference slot.
  // The global input/output gains already carry the reference slot's calibration; these are only the deltas.
  void _SetSlotTrims();
  // Recompute the blend gain (level + polarity + mute/solo) for one slot, or all of them.
  void _SetBlendGain(const size_t slot);
  void _SetBlendGains();
  // Whether a slot should be heard, given its own mute and the solo state of the whole set.
  bool _SlotIsAudible(const size_t slot);
  void _ApplySlimParamToLoadedNAMs();

  // See: Unserialization.cpp
  void _UnserializeApplyConfig(nlohmann::json& config);
  // 0.7.9 and later
  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  // Hopefully 0.7.3-0.7.8, but no gurantees
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);

  // Update all controls that depend on a model
  void _UpdateControlsFromModel();

  // Make sure that the latency is reported correctly.
  void _UpdateLatency();

  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  // Member data

  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Blended output from the NAMs
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Per-slot scratch. The input copy is only used when a slot needs an input trim; the reference slot is fed the
  // shared buffer directly so that the single-model path is untouched.
  std::array<std::vector<std::vector<iplug::sample>>, kNumModelSlots> mSlotInputArrays;
  std::array<std::vector<std::vector<iplug::sample>>, kNumModelSlots> mSlotOutputArrays;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;
  std::array<iplug::sample**, kNumModelSlots> mSlotInputPointers{};
  std::array<iplug::sample**, kNumModelSlots> mSlotOutputPointers{};

  // Input and output gain
  double mInputGain = 1.0;
  double mOutputGain = 1.0;

  // Per-slot level correction relative to the reference slot. Input trims go before the model, output trims after --
  // the model is nonlinear, so they don't commute.
  std::array<double, kNumModelSlots> mSlotInputTrim{};
  std::array<double, kNumModelSlots> mSlotOutputTrim{};
  // Per-slot blend gain: level in dB turned into an amplitude, times -1 when the slot's polarity is inverted.
  // The previous value is kept so that the gain can be ramped across a block; without that, flipping polarity clicks.
  std::array<double, kNumModelSlots> mSlotBlendGain{};
  std::array<double, kNumModelSlots> mSlotBlendGainPrev{};
  // Time alignment between slots with different resampler latencies.
  std::array<SlotDelay, kNumModelSlots> mSlotDelays;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  // The models actually being used. Their outputs are summed; the IR, tone stack and gate are shared downstream.
  std::array<std::unique_ptr<ResamplingNAM>, kNumModelSlots> mModel;
  // And the IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  // Manages switching what DSP is being used.
  std::array<std::unique_ptr<ResamplingNAM>, kNumModelSlots> mStagedModel;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  // Flags to take away the modules at a safe time.
  std::array<std::atomic<bool>, kNumModelSlots> mShouldRemoveModel{};
  std::atomic<bool> mShouldRemoveIR = false;

  std::array<std::atomic<bool>, kNumModelSlots> mNewModelLoadedInDSP{};
  std::array<std::atomic<bool>, kNumModelSlots> mModelCleared{};

  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;

  // Post-IR filters
  recursive_linear_filter::HighPass mHighPass;
  //  recursive_linear_filter::LowPass mLowPass;

  // Paths to each slot's config.json or model.nam
  std::array<WDL_String, kNumModelSlots> mNAMPath;
  // Path to IR (.wav file)
  WDL_String mIRPath;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  std::unordered_map<std::string, double> mNAMParams = {{"Input", 0.0}, {"Output", 0.0}};

  NAMSender mInputSender, mOutputSender;
};
