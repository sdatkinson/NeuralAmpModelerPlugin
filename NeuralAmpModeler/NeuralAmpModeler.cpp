#include <algorithm> // std::clamp
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
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

const double kDCBlockerFrequency = 5.0;

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
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Gate", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutNorm)->InitBool("OutNorm", true);
  GetParam(kIRToggle)->InitBool("IRToggle", true);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

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
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto helpSVG = pGraphics->LoadSVG(HELP_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);

    // Areas for knobs
    const auto knobsPad = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto knobHeight = 120.f;
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(knobHeight)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);
    const auto inputKnobArea = knobsArea.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto noiseGateArea = knobsArea.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto bassKnobArea = knobsArea.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto trebleKnobArea = knobsArea.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto outNormToggleArea =
      outputKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    // Areas for model and IR
    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto helpButtonArea = mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20);
    const auto sampleRateWarningArea = inputMeterArea.GetFromBottom(16.f).GetTranslated(12.f, 16.f).GetFromLeft(300.f);

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mNAMPath and mStagedNAM
        const std::string msg = _StageModel(fileName);
        // TODO error messages like the IR loader.
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          GetUI()->ShowMessageBox(ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };
    mModelFileManager.SetCompletionHandler(loadModelCompletionHandler);

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          GetUI()->ShowMessageBox(message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };
    mIRFileManager.SetCompletionHandler(loadIRCompletionHandler);

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "NEURAL AMP MODELER", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
    pGraphics->AttachControl(new NAMFileBrowserControl(modelArea, mModelFileManager, kMsgTagClearModel, defaultNamFileString.c_str(), style, fileSVG, crossSVG,
                                                       leftArrowSVG, rightArrowSVG, fileBackgroundBitmap),
                             kCtrlTagModelFileBrowser);
    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, mIRFileManager, kMsgTagClearIR, defaultIRString.c_str(), style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap),
      kCtrlTagIRFileBrowser);
    pGraphics->AttachControl(new NAMSwitchControl(ngToggleArea, kNoiseGateActive, " ", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));
    pGraphics->AttachControl(
      new NAMSwitchControl(outNormToggleArea, kOutNorm, "Normalize", style, switchHandleBitmap), kCtrlTagOutNorm);

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    // The meters
    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // A warning when NAM isn't being run in the right sample rate:
    pGraphics->AttachControl(new NAMSampleRateWarningControl(sampleRateWarningArea), kCtrlTagSampleRateWarning);

    // Help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      helpButtonArea,
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
    
    pGraphics->SetKeyHandlerFunc([&, pGraphics](const IKeyPress& key, bool isUp){
      if (!isUp)
      {
        if (key.VK == kVK_LEFT)
        {
          mModelFileManager.PreviousFile();
          pGraphics->GetControlWithTag(kCtrlTagModelFileBrowser)->As<NAMFileBrowserControl>()->PreviousFile();
          return true;
        }
        else if (key.VK == kVK_RIGHT)
        {
          mModelFileManager.NextFile();
          pGraphics->GetControlWithTag(kCtrlTagModelFileBrowser)->As<NAMFileBrowserControl>()->NextFile();
          return true;
        }
        if (key.VK == kVK_UP)
        {
          mIRFileManager.PreviousFile();
          pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->As<NAMFileBrowserControl>()->PreviousFile();
          return true;
        }
        else if (key.VK == kVK_DOWN)
        {
          mIRFileManager.NextFile();
          pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->As<NAMFileBrowserControl>()->NextFile();
          return true;
        }
      }
      return false;
    });
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const auto nInputs = NInChansConnected();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();
  
#ifdef APP_API
  const double inputGain = GetParam(kInputLevel)->DBToAmp();
#else
  const double inputGain = GetParam(kInputLevel)->DBToAmp() / static_cast<float>(nInputs);
#endif
  
  if (nInputs == 1)
  {
    for (auto s=0; s<nFrames; s++)
    {
      inputs[0][s] *= inputGain;
    }
  }
  else
  {
    for (auto s=0; s<nFrames; s++)
    {
      inputs[0][s] = (inputs[0][s] + inputs[1][s]) * inputGain;
    }
  }
  
  _ApplyDSPStaging();
  
  if (mModel != nullptr)
  {
    // TODO: check we are not mallocing here
    mNonIntegerSRC.ProcessBlock(inputs, outputs, nFrames, [&](sample** newInputs, sample** newOutputs, int newNFrames) {
      mModel->process(newInputs[0], newOutputs[0], newNFrames);
      mModel->finalize_(newNFrames);
    });
  }
  else
  {
    for (auto s=0; s<nFrames; s++)
    {
      outputs[0][s] = inputs[0][s];
    }
  }

  const auto outputGain = GetParam(kOutputLevel)->DBToAmp();
  
  for (auto s=0; s<nFrames; s++)
  {
    outputs[0][s] = outputs[0][s] * outputGain;
    outputs[1][s] = outputs[0][s];
  }
  
  mInputSender.ProcessBlock(inputs, nFrames, kCtrlTagInputMeter, 1);
  mOutputSender.ProcessBlock(outputs, nFrames, kCtrlTagOutputMeter, 1);
  
  // restore previous floating point state
  std::feupdateenv(&fe_state);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  mCheckSampleRateWarning = true;
  mNonIntegerSRC.Reset(sampleRate);
  
  // Setup noise gate
  const auto time = 0.01;
  const auto threshold = GetParam(kNoiseGateThreshold)->Value();
  const auto ratio = 0.1; // Quadratic...
  const auto openTime = 0.005;
  const auto holdTime = 0.01;
  const auto closeTime = 0.05;
  const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
  mNoiseGateTrigger.SetParams(triggerParams);
  mNoiseGateTrigger.SetSampleRate(sampleRate);
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
      pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!mModel->HasLoudness());

    mNewModelLoadedInDSP = false;
  }
  if (mCheckSampleRateWarning)
  {
    _CheckSampleRateWarning();
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(mNAMPath.Get());
  chunk.PutStr(mIRPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String dir;
  startPos = chunk.GetStr(mNAMPath, startPos);
  startPos = chunk.GetStr(mIRPath, startPos);
  int retcode = UnserializeParams(chunk, startPos);
  if (mNAMPath.GetLength())
    _StageModel(mNAMPath);
  if (mIRPath.GetLength())
    _StageIR(mIRPath);
  return retcode;
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

  if (mNAMPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
    // If it's not loaded yet, then mark as failed.
    // If it's yet to be loaded, then the completion handler will set us straight once it runs.
    if (mModel == nullptr && mStagedModel == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (mModel != nullptr)
    GetUI()->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!mModel->HasLoudness());
  mCheckSampleRateWarning = true;
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
    case kMsgTagClearModel: mShouldRemoveModel = true; return true;
    case kMsgTagClearIR: mShouldRemoveIR = true; return true;
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
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

void NeuralAmpModeler::_ApplyDSPStaging()
{
  // Move things from staged to live
  if (mStagedModel != nullptr)
  {
    // Move from staged to active DSP
    mModel = std::move(mStagedModel);
    mStagedModel = nullptr;
    mNewModelLoadedInDSP = true;
    mCheckSampleRateWarning = true;
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
  // Remove marked modules
  if (mShouldRemoveModel)
  {
    mModel = nullptr;
    mNAMPath.Set("");
    mShouldRemoveModel = false;
    mCheckSampleRateWarning = true;
  }
  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  // Move things from staged to live
  if (mStagedModel != nullptr)
  {
    // Move from staged to active DSP
    mModel = std::move(mStagedModel);
    mStagedModel = nullptr;
    mNewModelLoadedInDSP = true;
    mCheckSampleRateWarning = true;
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
}

void NeuralAmpModeler::_CheckSampleRateWarning()
{
  if (auto* pGraphics = GetUI())
  {
    auto* control = pGraphics->GetControlWithTag(kCtrlTagSampleRateWarning)->As<NAMSampleRateWarningControl>();
    bool showWarning = false;
    if (_HaveModel())
    {
      const auto pluginSampleRate = GetSampleRate();
      const auto namSampleRateFromModel = mModel->GetExpectedSampleRate();
      // Any model with "-1" is probably 48k
      const auto namSampleRate = namSampleRateFromModel == -1.0 ? 48000.0 : namSampleRateFromModel;
      control->SetSampleRate(namSampleRate);
      showWarning = pluginSampleRate != namSampleRate;
    }
    control->SetDisabled(!showWarning);
    mCheckSampleRateWarning = false;
  }
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = mNAMPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    mStagedModel = nam::get_dsp(dspPath);
    mNAMPath = modelPath;
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
  }
  catch (std::runtime_error& e)
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);

    if (mStagedModel != nullptr)
    {
      mStagedModel = nullptr;
    }
    mNAMPath = previousNAMPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRPath = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
  }
  else
  {
    if (mStagedIR != nullptr)
    {
      mStagedIR = nullptr;
    }
    mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

