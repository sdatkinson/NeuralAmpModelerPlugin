#include <algorithm> // std::clamp
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>

// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Gate", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  DeallocateIOPointers();
}

IGraphics* NeuralAmpModeler::CreateGraphics()
{
#ifdef OS_IOS
  auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
  auto scaleFactor = 1.0f;
#endif
  return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
}

void NeuralAmpModeler::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = mNUM_INTERNAL_CHANNELS;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  if (mNAM != nullptr)
  {
    // TODO remove input / output gains from here.
    const double inputGain = 1.0;
    const double outputGain = 1.0;
    const int nChans = (int)numChannelsInternal;
    mNAM->process(triggerOutput, mOutputPointers, nChans, nFrames, inputGain, outputGain, mNAMParams);
    mNAM->finalize_(nFrames);
  }
  else
  {
    FallbackDSP(triggerOutput, mOutputPointers, numChannelsInternal, numFrames);
  }
  // Apply the noise gate
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  sample** toneStackOutPointers = gateGainOutput;
  if (toneStackActive)
  {
    // Translate params from knob 0-10 to dB.
    // Tuned ranges based on my ear. E.g. seems treble doesn't need nearly as
    // much swing as bass can use.
    const double bassGainDB = 4.0 * (GetParam(kToneBass)->Value() - 5.0); // +/- 20
    const double midGainDB = 3.0 * (GetParam(kToneMid)->Value() - 5.0); // +/- 15
    const double trebleGainDB = 2.0 * (GetParam(kToneTreble)->Value() - 5.0); // +/- 10

    const double bassFrequency = 150.0;
    const double midFrequency = 425.0;
    const double trebleFrequency = 1800.0;
    const double bassQuality = 0.707;
    // Wider EQ on mid bump up to sound less honky.
    const double midQuality = midGainDB < 0.0 ? 1.5 : 0.7;
    const double trebleQuality = 0.707;

    // Define filter parameters
    recursive_linear_filter::BiquadParams bassParams(sampleRate, bassFrequency, bassQuality, bassGainDB);
    recursive_linear_filter::BiquadParams midParams(sampleRate, midFrequency, midQuality, midGainDB);
    recursive_linear_filter::BiquadParams trebleParams(sampleRate, trebleFrequency, trebleQuality, trebleGainDB);
    // Apply tone stack
    // Set parameters
    mToneBass.SetParams(bassParams);
    mToneMid.SetParams(midParams);
    mToneTreble.SetParams(trebleParams);
    sample** bassPointers = mToneBass.Process(gateGainOutput, numChannelsInternal, numFrames);
    sample** midPointers = mToneMid.Process(bassPointers, numChannelsInternal, numFrames);
    sample** treblePointers = mToneTreble.Process(midPointers, numChannelsInternal, numFrames);
    toneStackOutPointers = treblePointers;
  }

  sample** irPointers = toneStackOutPointers;
  if (mIR != nullptr)
    irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  ProcessOutput(irPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(mNAMPath.Get());
  chunk.PutStr(mIRPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int pos)
{
  WDL_String dir;
  pos = chunk.GetStr(mNAMPath, pos);
  pos = chunk.GetStr(mIRPath, pos);
  mNAM = nullptr;
  mIR = nullptr;
  pos = UnserializeParams(chunk, pos);
  if (mNAMPath.GetLength())
    GetNAM(mNAMPath);
  if (mIRPath.GetLength())
    GetIR(mIRPath);
  return pos;
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();
  if (mNAMPath.GetLength())
    SetModelMsg(mNAMPath);
  if (mIRPath.GetLength())
    SetIRMsg(mIRPath);
}

#pragma mark -
#pragma mark Private methods ============================================================

void NeuralAmpModeler::AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::ApplyDSPStaging()
{
  // Move things from staged to live
  if (mStagedNAM != nullptr)
  {
    // Move from staged to active DSP
    mNAM = std::move(mStagedNAM);
    mStagedNAM = nullptr;
  }

  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }

  // Remove marked modules
  if (mFlagRemoveNAM)
  {
    mNAM = nullptr;
    mNAMPath.Set("");
    UnsetModelMsg();
    mFlagRemoveNAM = false;
  }

  if (mFlagRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    UnsetIRMsg();
    mFlagRemoveIR = false;
  }
}

void NeuralAmpModeler::DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void NeuralAmpModeler::FallbackDSP(sample** inputs, sample** outputs, const size_t numChannels, const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      mOutputArray[c][s] = mInputArray[c][s];
}

std::string NeuralAmpModeler::GetNAM(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = mNAMPath;
  try
  {
    auto dspPath = std::filesystem::path(modelPath.Get());
    mStagedNAM = get_dsp(dspPath);
    SetModelMsg(modelPath);
    mNAMPath = modelPath;
  }
  catch (std::exception& e)
  {
    std::stringstream ss;
    ss << "FAILED to load model";
    SendControlMsgFromDelegate(kCtrlTagModelName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
    if (mStagedNAM != nullptr)
    {
      mStagedNAM = nullptr;
    }
    mNAMPath = previousNAMPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::GetIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPath.Get(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::exception& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    SetIRMsg(irPath);
    mIRPath = irPath;
  }
  else
  {
    if (mStagedIR != nullptr)
    {
      mStagedIR = nullptr;
    }
    mIRPath = previousIRPath;
    std::stringstream ss;
    ss << "FAILED to load IR";
    SendControlMsgFromDelegate(kCtrlTagIRName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
  }

  return wavState;
}

size_t NeuralAmpModeler::GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::GetBufferNumFrames() const
{
  if (GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != GetBufferNumChannels();
  const bool updateFrames = updateChannels || (GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
}

void NeuralAmpModeler::PrepareIOPointers(const size_t numChannels)
{
  DeallocateIOPointers();
  AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::ProcessInput(sample** inputs, const size_t nFrames, const size_t nChansIn,
                                    const size_t nChansOut)
{
  // Assume _PrepareBuffers() was already called
  const double gain = pow(10.0, GetParam(kInputLevel)->Value() / 20.0);
  if (nChansOut <= nChansIn) // Many->few: Drop additional channels
    for (size_t c = 0; c < nChansOut; c++)
      for (size_t s = 0; s < nFrames; s++)
        mInputArray[c][s] = gain * inputs[c][s];
  else
  {
    // Something is wrong--this is a mono plugin. How could there be fewer
    // incoming channels?
    throw std::runtime_error("Unexpected input processing--sees fewer than 1 incoming channel?");
  }
}

void NeuralAmpModeler::ProcessOutput(sample** inputs, sample** outputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  const double gain = pow(10.0, GetParam(kOutputLevel)->Value() / 20.0);
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  // Broadcast the internal mono stream to all output channels.
  const size_t cin = 0;
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
}

void NeuralAmpModeler::SetModelMsg(const WDL_String& modelPath)
{
  auto dspPath = std::filesystem::path(modelPath.Get());
  std::stringstream ss;
  ss << "Loaded ";
  if (dspPath.has_filename())
    ss << dspPath.filename().stem(); // /path/to/model.nam -> "model"
  else
    ss << dspPath.parent_path().filename(); // /path/to/model/ -> "model"
  SendControlMsgFromDelegate(kCtrlTagModelName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
}

void NeuralAmpModeler::SetIRMsg(const WDL_String& irPath)
{
  mIRPath = irPath; // This might already be done elsewhere...need to dedup.
  auto dspPath = std::filesystem::path(irPath.Get());
  std::stringstream ss;
  ss << "Loaded " << dspPath.filename().stem();
  SendControlMsgFromDelegate(kCtrlTagIRName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
}

void NeuralAmpModeler::UnsetModelMsg()
{
  UnsetMsg(kCtrlTagModelName, mDefaultNAMString);
}

void NeuralAmpModeler::UnsetIRMsg()
{
  UnsetMsg(kCtrlTagIRName, mDefaultIRString);
}

void NeuralAmpModeler::UnsetMsg(const int tag, const WDL_String& msg)
{
  SendControlMsgFromDelegate(tag, 0, int(strlen(msg.Get())), msg.Get());
}

void NeuralAmpModeler::UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                    const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}
