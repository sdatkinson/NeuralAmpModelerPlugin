#include <algorithm> // std::clamp
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "IControls.h"
#include "NeuralAmpModelerCore/NAM/activations.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"

using namespace iplug;
using namespace igraphics;

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};

const IVStyle titleStyle =
DEFAULT_STYLE
.WithValueText(IText(24, COLOR_WHITE, "Ronduit-Light"))
.WithDrawFrame(false)
.WithShadowOffset(2.f);

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
, mInputPointers(nullptr)
, mOutputPointers(nullptr)
, mNoiseGateTrigger()
, mNAM(nullptr)
, mIR(nullptr)
, mStagedNAM(nullptr)
, mStagedIR(nullptr)
, mFlagRemoveNAM(false)
, mFlagRemoveIR(false)
, mToneBass()
, mToneMid()
, mToneTreble()
, mNAMPath()
, mIRPath()
, mInputSender()
, mOutputSender()
, mHighLightColor(PluginColors::NAM_THEMECOLOR.ToColorCode())
{
  activations::Activation::enable_fast_tanh();
  this->GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  this->GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  this->GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  this->GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  this->GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  this->GetParam(kNoiseGateThreshold)->InitGain("Gate", -80.0, -100.0, 0.0, 0.1);
  this->GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  this->GetParam(kEQActive)->InitBool("ToneStack", true);
  this->GetParam(kOutNorm)->InitBool("OutNorm", false);
  this->GetParam(kIRToggle)->InitBool("IRToggle", true);

  this->mNoiseGateTrigger.AddListener(&this->mNoiseGateGain);

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);    
    
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Ronduit-Light", RONDUIT_FN);
    
    auto helpSVG = pGraphics->LoadSVG(HELP_FN);
    auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);

    auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);

    const IRECT b = pGraphics->GetBounds();
    const IRECT mainArea = b.GetPadded(-20);
    const auto content = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = content.GetFromTop(titleHeight);

    // Area for the Noise gate knob
    const float allKnobsHalfPad = 10.0f;
    const float allKnobsPad = 2.0f * allKnobsHalfPad;

    // Areas for knobs
    const float knobsExtraSpaceBelowTitle = 25.0f;
    const float knobHeight = 120.f;
    const float singleKnobPad = -2.0f;
    const auto knobs = content.GetFromTop(knobHeight)
                         .GetReducedFromLeft(allKnobsPad)
                         .GetReducedFromRight(allKnobsPad)
                         .GetTranslated(0.0f, titleHeight + knobsExtraSpaceBelowTitle);
    const IRECT inputKnobArea = knobs.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT noiseGateArea = knobs.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT bassKnobArea = knobs.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT midKnobArea = knobs.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT trebleKnobArea = knobs.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT outputKnobArea = knobs.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const IRECT ngToggleArea = noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const IRECT eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const IRECT outNormToggleArea = outputKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    // Areas for model and IR
    const float fileWidth = 200.0f;
    const float fileHeight = 30.0f;
    const float irYOffset = 38.0f;
    const IRECT modelArea = content.GetFromBottom((2.0f * fileHeight))
                              .GetFromTop(fileHeight)
                              .GetMidHPadded(fileWidth)
                              .GetTranslated(0.0f, -1);
    const IRECT irArea = modelArea.GetTranslated(0.0f, irYOffset);

    // Areas for meters
    const float meterHalfHeight = 0.5f * 385.0f;
    const IRECT inputMeterArea = inputKnobArea.GetFromLeft(allKnobsHalfPad)
                                   .GetMidHPadded(allKnobsHalfPad)
                                   .GetMidVPadded(meterHalfHeight)
                                   .GetTranslated(-allKnobsPad - 18.f, 0.0f);
    const IRECT outputMeterArea = outputKnobArea.GetFromRight(allKnobsHalfPad)
                                    .GetMidHPadded(allKnobsHalfPad)
                                    .GetMidVPadded(meterHalfHeight)
                                    .GetTranslated(allKnobsPad + 18.f, 0.0f);

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mNAMPath and mStagedNAM
        const std::string msg = this->_GetNAM(fileName);
        // TODO error messages like the IR loader.
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n"
             << msg << "\n\n"
             << "If the model is an old \"directory-style\" model, it "
                "can be "
                "converted using the utility at "
                "https://github.com/sdatkinson/nam-model-utility";
          GetUI()->ShowMessageBox(ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        this->mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = this->_GetIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          GetUI()->ShowMessageBox(message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "Neural Amp Modeler", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelArea.GetFromLeft(30).GetTranslated(-40, 10), modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    pGraphics->AttachControl(new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(),
                                                       "nam", loadModelCompletionHandler, style, fileSVG,
                                                       crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap),
                             kCtrlTagModelFileBrowser);
    pGraphics->AttachControl(new ISVGSwitchControl(irArea.GetFromLeft(30).GetTranslated(-40, 0).GetScaledAboutCentre(0.6), { irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearModel, defaultIRString.c_str(), "wav", loadIRCompletionHandler,
                                style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap),
      kCtrlTagIRFileBrowser);
    pGraphics->AttachControl(new NAMSwitchControl(ngToggleArea, kNoiseGateActive, " ", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(outNormToggleArea, kOutNorm, "Normalize", style, switchHandleBitmap), kCtrlTagOutNorm);

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    // The meters
    const float meterMin = -90.0f;
    const float meterMax = -0.01f;
    pGraphics
      ->AttachControl(
        new IVPeakAvgMeterControl(inputMeterArea, "",
                                  style.WithWidgetFrac(0.5).WithShowValue(false).WithDrawFrame(false).WithColor(
                                    kFG, PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f)),
                                  EDirection::Vertical, {}, 0, meterMin, meterMax, {}),
        kCtrlTagInputMeter)
      ->As<IVPeakAvgMeterControl<>>()
      ->SetPeakSize(2.0f);
    pGraphics
      ->AttachControl(
        new IVPeakAvgMeterControl(outputMeterArea, "",
                                  style.WithWidgetFrac(0.5).WithShowValue(false).WithDrawFrame(false).WithColor(
                                    kFG, PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f)),
                                  EDirection::Vertical, {}, 0, meterMin, meterMax, {}),
        kCtrlTagOutputMeter)
      ->As<IVPeakAvgMeterControl<>>()
      ->SetPeakSize(2.0f);

    //     Help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20),
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagAboutBox)->As<NAMAboutBoxControl>()->HideAnimated(false);
      },
      helpSVG));

    pGraphics->AttachControl(new NAMAboutBoxControl(b, backgroundBitmap, style), kCtrlTagAboutBox)->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
    
    pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);

  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  this->_DeallocateIOPointers();
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)this->NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)this->NOutChansConnected();
  const size_t numChannelsInternal = this->mNUM_INTERNAL_CHANNELS;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = this->GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  this->_PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  this->_ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  this->_ApplyDSPStaging();
  const bool noiseGateActive = this->GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = this->GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = this->GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    this->mNoiseGateTrigger.SetParams(triggerParams);
    this->mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = this->mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  if (mNAM != nullptr)
  {
    mNAM->SetNormalize(this->GetParam(kOutNorm)->Value());
    // TODO remove input / output gains from here.
    const double inputGain = 1.0;
    const double outputGain = 1.0;
    const int nChans = (int)numChannelsInternal;
    mNAM->process(triggerOutput, this->mOutputPointers, nChans, nFrames, inputGain, outputGain, mNAMParams);
    mNAM->finalize_(nFrames);
  }
  else
  {
    this->_FallbackDSP(triggerOutput, this->mOutputPointers, numChannelsInternal, numFrames);
  }
  // Apply the noise gate
  sample** gateGainOutput = noiseGateActive
                              ? this->mNoiseGateGain.Process(this->mOutputPointers, numChannelsInternal, numFrames)
                              : this->mOutputPointers;

  sample** toneStackOutPointers = gateGainOutput;
  if (toneStackActive)
  {
    // Translate params from knob 0-10 to dB.
    // Tuned ranges based on my ear. E.g. seems treble doesn't need nearly as
    // much swing as bass can use.
    const double bassGainDB = 4.0 * (this->GetParam(kToneBass)->Value() - 5.0); // +/- 20
    const double midGainDB = 3.0 * (this->GetParam(kToneMid)->Value() - 5.0); // +/- 15
    const double trebleGainDB = 2.0 * (this->GetParam(kToneTreble)->Value() - 5.0); // +/- 10

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
    this->mToneBass.SetParams(bassParams);
    this->mToneMid.SetParams(midParams);
    this->mToneTreble.SetParams(trebleParams);
    sample** bassPointers = this->mToneBass.Process(gateGainOutput, numChannelsInternal, numFrames);
    sample** midPointers = this->mToneMid.Process(bassPointers, numChannelsInternal, numFrames);
    sample** treblePointers = this->mToneTreble.Process(midPointers, numChannelsInternal, numFrames);
    toneStackOutPointers = treblePointers;
  }

  sample** irPointers = toneStackOutPointers;
  if (this->mIR != nullptr && this->GetParam(kIRToggle)->Value())
    irPointers = this->mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  this->_ProcessOutput(irPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  this->_UpdateMeters(this->mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = this->GetSampleRate();
  this->mInputSender.Reset(sampleRate);
  this->mOutputSender.Reset(sampleRate);
}

void NeuralAmpModeler::OnIdle()
{
  this->mInputSender.TransmitData(*this);
  this->mOutputSender.TransmitData(*this);

  if (this->mNewNAMLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
      pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!this->mNAM->HasLoudness());

    this->mNewNAMLoadedInDSP = false;
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(this->mNAMPath.Get());
  chunk.PutStr(this->mIRPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String dir;
  startPos = chunk.GetStr(this->mNAMPath, startPos);
  startPos = chunk.GetStr(this->mIRPath, startPos);
  int retcode = UnserializeParams(chunk, startPos);
  if (this->mNAMPath.GetLength())
    this->_GetNAM(this->mNAMPath);
  if (this->mIRPath.GetLength())
    this->_GetIR(this->mIRPath);
  return retcode;
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

  if (this->mNAMPath.GetLength())
    SendControlMsgFromDelegate(
      kCtrlTagModelFileBrowser, kMsgTagLoadedModel, this->mNAMPath.GetLength(), this->mNAMPath.Get());
  if (this->mIRPath.GetLength())
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, this->mIRPath.GetLength(), this->mIRPath.Get());
  if (this->mNAM != nullptr)
    this->GetUI()->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!this->mNAM->HasLoudness());
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        break;
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active);
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: mFlagRemoveNAM = true; return true;
    case kMsgTagClearIR: mFlagRemoveIR = true; return true;
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*) pData);
      
      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl){
          
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }
      
      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (this->mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  this->mInputPointers = new sample*[nChans];
  if (this->mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (this->mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  this->mOutputPointers = new sample*[nChans];
  if (this->mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
  // Move things from staged to live
  if (this->mStagedNAM != nullptr)
  {
    // Move from staged to active DSP
    this->mNAM = std::move(this->mStagedNAM);
    this->mStagedNAM = nullptr;
    this->mNewNAMLoadedInDSP = true;
  }
  if (this->mStagedIR != nullptr)
  {
    this->mIR = std::move(this->mStagedIR);
    this->mStagedIR = nullptr;
  }
  // Remove marked modules
  if (this->mFlagRemoveNAM)
  {
    this->mNAM = nullptr;
    this->mNAMPath.Set("");
    this->mFlagRemoveNAM = false;
  }
  if (this->mFlagRemoveIR)
  {
    this->mIR = nullptr;
    this->mIRPath.Set("");
    this->mFlagRemoveIR = false;
  }
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  if (this->mInputPointers != nullptr)
  {
    delete[] this->mInputPointers;
    this->mInputPointers = nullptr;
  }
  if (this->mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (this->mOutputPointers != nullptr)
  {
    delete[] this->mOutputPointers;
    this->mOutputPointers = nullptr;
  }
  if (this->mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      this->mOutputArray[c][s] = this->mInputArray[c][s];
}

std::string NeuralAmpModeler::_GetNAM(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = this->mNAMPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    mStagedNAM = get_dsp(dspPath);
    this->mNAMPath = modelPath;
    SendControlMsgFromDelegate(
      kCtrlTagModelFileBrowser, kMsgTagLoadedModel, this->mNAMPath.GetLength(), this->mNAMPath.Get());
  }
  catch (std::exception& e)
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);

    if (this->mStagedNAM != nullptr)
    {
      this->mStagedNAM = nullptr;
    }
    this->mNAMPath = previousNAMPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_GetIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = this->mIRPath;
  const double sampleRate = this->GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    this->mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = this->mStagedIR->GetWavState();
  }
  catch (std::exception& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    this->mIRPath = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, this->mIRPath.GetLength(), this->mIRPath.Get());
  }
  else
  {
    if (this->mStagedIR != nullptr)
    {
      this->mStagedIR = nullptr;
    }
    this->mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return this->mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (this->_GetBufferNumChannels() == 0)
    return 0;
  return this->mInputArray[0].size();
}

void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != this->_GetBufferNumChannels();
  const bool updateFrames = updateChannels || (this->_GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    this->_PrepareIOPointers(numChannels);
    this->mInputArray.resize(numChannels);
    this->mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < this->mInputArray.size(); c++)
    {
      this->mInputArray[c].resize(numFrames);
      std::fill(this->mInputArray[c].begin(), this->mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < this->mOutputArray.size(); c++)
    {
      this->mOutputArray[c].resize(numFrames);
      std::fill(this->mOutputArray[c].begin(), this->mOutputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < this->mInputArray.size(); c++)
    this->mInputPointers[c] = this->mInputArray[c].data();
  for (auto c = 0; c < this->mOutputArray.size(); c++)
    this->mOutputPointers[c] = this->mOutputArray[c].data();
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  this->_DeallocateIOPointers();
  this->_AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  // See also: this->mNUM_INTERNAL_CHANNELS
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nCahnsIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness.
#ifdef APP_API
  const double gain = pow(10.0, GetParam(kInputLevel)->Value() / 20.0);
#else
  const double gain = pow(10.0, GetParam(kInputLevel)->Value() / 20.0) / (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        this->mInputArray[0][s] = gain * inputs[c][s];
      else
        this->mInputArray[0][s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = pow(10.0, GetParam(kOutputLevel)->Value() / 20.0);
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  // Broadcast the internal mono stream to all output channels.
  const size_t cin = 0;
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  this->mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  this->mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}
