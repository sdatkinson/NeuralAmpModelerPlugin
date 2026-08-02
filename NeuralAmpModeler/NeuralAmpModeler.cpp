#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
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
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;


NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  _InitToneStack();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);
  // Model blending. Slots sum like mixer channels: two slots at 0 dB are about 6 dB louder than one.
  GetParam(kBlendLevel1)->InitGain("Blend1", 0.0, kBlendMuteDB, 6.0, 0.1);
  GetParam(kBlendLevel2)->InitGain("Blend2", 0.0, kBlendMuteDB, 6.0, 0.1);
  GetParam(kBlendLevel3)->InitGain("Blend3", 0.0, kBlendMuteDB, 6.0, 0.1);
  GetParam(kBlendInvert1)->InitBool("Invert1", false);
  GetParam(kBlendInvert2)->InitBool("Invert2", false);
  GetParam(kBlendInvert3)->InitBool("Invert3", false);
  GetParam(kBlendMute1)->InitBool("Mute1", false);
  GetParam(kBlendMute2)->InitBool("Mute2", false);
  GetParam(kBlendMute3)->InitBool("Mute3", false);
  GetParam(kBlendSolo1)->InitBool("Solo1", false);
  GetParam(kBlendSolo2)->InitBool("Solo2", false);
  GetParam(kBlendSolo3)->InitBool("Solo3", false);

  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    mSlotInputTrim[slot] = 1.0;
    mSlotOutputTrim[slot] = 1.0;
  }
  _SetBlendGains();
  mSlotBlendGainPrev = mSlotBlendGain;

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

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);
    const auto blendIconSVG = pGraphics->LoadSVG(BLEND_ICON_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
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
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
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

    // Areas for model and IR
    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    // Two 28-wide icon slots to the right of the model row: the blend page button (always visible) and the slim knob
    // button (only shown for slimmable models).
    const auto blendIconArea =
      IRECT(modelArea.R + 6.f, modelArea.MH() - 14.f, modelArea.R + 6.f + 28.f, modelArea.MH() + 14.f);
    const auto slimIconArea =
      IRECT(modelArea.R + 38.f, modelArea.MH() - 14.f, modelArea.R + 38.f + 28.f, modelArea.MH() + 14.f);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

    // Model loader button. One handler per slot; they all do the same thing to a different slot.
    auto makeLoadModelCompletionHandler = [this](const size_t slot) {
      return [this, slot](const WDL_String& fileName, const WDL_String& path) {
        if (fileName.GetLength())
        {
          // Sets mNAMPath[slot] and mStagedModel[slot]
          const std::string msg = _StageModel(fileName, slot);
          // TODO error messages like the IR loader.
          if (msg.size())
          {
            std::stringstream ss;
            ss << "Failed to load NAM model. Message:\n\n" << msg;
            _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
          }
          std::cout << "Loaded: " << fileName.Get() << std::endl;
        }
      };
    };
    auto loadModelCompletionHandler = makeLoadModelCompletionHandler(0);

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

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "NEURAL AMP MODELER", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl),
      kCtrlTagModelFileBrowser);

    auto hideSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(true);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(true);
      ui->SetAllControlsDirty();
    };
    auto showSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(false);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(false);
      ui->SetAllControlsDirty();
    };

    pGraphics
      ->AttachControl(
        new NAMSquareButtonControl(slimIconArea, DefaultClickActionFunc, slimIconSVG), kCtrlTagSlimmableIcon)
      ->SetAnimationEndActionFunction(showSlimOverlay)
      ->Hide(true);

    auto showBlendPage = [](IControl* pCaller) {
      pCaller->GetUI()->GetControlWithTag(kCtrlTagBlendPage)->As<NAMBlendPageControl>()->HideAnimated(false);
    };
    pGraphics
      ->AttachControl(
        new NAMSquareButtonControl(blendIconArea, DefaultClickActionFunc, blendIconSVG), kCtrlTagBlendIcon)
      ->SetAnimationEndActionFunction(showBlendPage)
      ->SetTooltip("Blend up to 3 models together");

    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl),
      kCtrlTagIRFileBrowser);
    pGraphics->AttachControl(
      new NAMSwitchControl(ngToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));

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

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    // Blend page: the other two model slots, plus a level and a polarity invert for each of the three.
    std::array<IFileDialogCompletionHandlerFunc, kNumModelSlots> blendCompletionHandlers;
    for (size_t slot = 0; slot < kNumModelSlots; slot++)
    {
      blendCompletionHandlers[slot] = makeLoadModelCompletionHandler(slot);
    }
    pGraphics
      ->AttachControl(new NAMBlendPageControl(b, backgroundBitmap, fileBackgroundBitmap, knobBackgroundBitmap, crossSVG,
                                              fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, globeSVG, style,
                                              defaultNamFileString.c_str(), getUrl, blendCompletionHandlers),
                      kCtrlTagBlendPage)
      ->Hide(true);

    const auto slimKnobArea = b.GetCentredInside(100.f, NAM_KNOB_HEIGHT + 24.f);
    pGraphics->AttachControl(new NAMSlimOverlayBackdropControl(b, hideSlimOverlay), kCtrlTagSlimOverlayBackdrop)
      ->Hide(true);
    pGraphics
      ->AttachControl(new NAMKnobControl(slimKnobArea, kSlim, "Slim", style, knobBackgroundBitmap), kCtrlTagSlimKnob)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  _DeallocateIOPointers();
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();
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

  // Run every loaded model on the same signal and sum them into mOutputArray.
  _BlendModels(triggerOutput, numFrames);
  // Apply the noise gate after the NAM
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  sample** toneStackOutPointers = (toneStackActive && mToneStack != nullptr)
                                    ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                    : gateGainOutput;

  sample** irPointers = toneStackOutPointers;
  if (mIR != nullptr && GetParam(kIRToggle)->Value())
    irPointers = mIR->Process(toneStackOutPointers, numChannelsInternal, numFrames);

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  // const double lowPassCutoffFreq = 20000.0;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  // mLowPass.SetParams(lowPassParams);
  sample** hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, numFrames);
  // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // Allocate the slot alignment delays here, once, so that _UpdateLatency never allocates on the audio thread.
  for (auto& delay : mSlotDelays)
  {
    delay.Resize(kMaxSlotDelaySamples);
  }
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mToneStack->Reset(sampleRate, maxBlockSize);
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mNewModelLoadedInDSP[slot])
    {
      if (auto* pGraphics = GetUI())
      {
        _UpdateControlsFromModel();
        mNewModelLoadedInDSP[slot] = false;
      }
    }
    if (mModelCleared[slot])
    {
      if (auto* pGraphics = GetUI())
      {
        // Let every browser bound to this slot know, including the one that didn't ask for the clear.
        _SendToSlotBrowsers(slot, kMsgTagModelCleared);
        // FIXME -- need to disable only the "normalized" model
        // pGraphics->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
        if (!_HaveModel())
        {
          static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
        }
        else
        {
          // Other slots are still loaded, so the model-dependent controls need to follow whatever's left.
          _UpdateControlsFromModel();
        }
        if (!_AnyModelIsSlimmable())
        {
          if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
            p->Hide(true);
          if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
            p->Hide(true);
          if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimKnob))
            p->Hide(true);
        }
        pGraphics->SetAllControlsDirty();
        mModelCleared[slot] = false;
      }
    }
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // If this isn't here when unserializing, then we know we're dealing with something before v0.8.0.
  WDL_String header("###NeuralAmpModeler###"); // Don't change this!
  chunk.PutStr(header.Get());
  // Plugin version, so we can load legacy serialized states in the future!
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  // Model directories (don't serialize the models themselves; we'll just load them again
  // when we unserialize). One per blend slot, in order.
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    chunk.PutStr(mNAMPath[slot].Get());
  }
  chunk.PutStr(mIRPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###NeuralAmpModeler###";
  if (strcmp(header.Get(), kExpectedHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mNAMPath[slot].GetLength())
    {
      _SendToSlotBrowsers(slot, kMsgTagLoadedModel, mNAMPath[slot].GetLength(), mNAMPath[slot].Get());
      // If it's not loaded yet, then mark as failed.
      // If it's yet to be loaded, then the completion handler will set us straight once it runs.
      if (mModel[slot] == nullptr && mStagedModel[slot] == nullptr)
        _SendToSlotBrowsers(slot, kMsgTagLoadFailed);
      // Reopening the UI shouldn't cost the folder convenience that was there before it was closed.
      _SeedFolderIntoEmptySlots(mNAMPath[slot], slot);
    }
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (_HaveModel())
  {
    _UpdateControlsFromModel();
  }
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    // Changes to the input gain
    case kCalibrateInput:
    case kInputCalibrationLevel:
      _SetInputGain();
      _SetSlotTrims();
      break;
    case kInputLevel: _SetInputGain(); break;
    // Changes to the output gain
    case kOutputLevel: _SetOutputGain(); break;
    case kOutputMode:
      _SetOutputGain();
      _SetSlotTrims();
      break;
    // Model blending. Mute only affects its own slot, but solo changes what every other slot does, so those
    // recompute the whole set.
    case kBlendLevel1:
    case kBlendInvert1:
    case kBlendMute1: _SetBlendGain(0); break;
    case kBlendLevel2:
    case kBlendInvert2:
    case kBlendMute2: _SetBlendGain(1); break;
    case kBlendLevel3:
    case kBlendInvert3:
    case kBlendMute3: _SetBlendGain(2); break;
    case kBlendSolo1:
    case kBlendSolo2:
    case kBlendSolo3: _SetBlendGains(); break;
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kSlim: _ApplySlimParamToLoadedNAMs(); break;
    default: break;
  }
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
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: mShouldRemoveModel[0] = true; return true;
    case kMsgTagClearModel2: mShouldRemoveModel[1] = true; return true;
    case kMsgTagClearModel3: mShouldRemoveModel[2] = true; return true;
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

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
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
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mSlotInputPointers[slot] != nullptr || mSlotOutputPointers[slot] != nullptr)
      throw std::runtime_error("Tried to re-allocate slot pointers without freeing");
    mSlotInputPointers[slot] = new sample*[nChans];
    mSlotOutputPointers[slot] = new sample*[nChans];
    if (mSlotInputPointers[slot] == nullptr || mSlotOutputPointers[slot] == nullptr)
      throw std::runtime_error("Failed to allocate pointer to slot buffer!\n");
  }
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
  bool modelsChanged = false;

  // Remove marked modules
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mShouldRemoveModel[slot])
    {
      mModel[slot] = nullptr;
      mNAMPath[slot].Set("");
      mShouldRemoveModel[slot] = false;
      mModelCleared[slot] = true;
      modelsChanged = true;
    }
  }
  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  // Move things from staged to live
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mStagedModel[slot] != nullptr)
    {
      mModel[slot] = std::move(mStagedModel[slot]);
      mStagedModel[slot] = nullptr;
      mNewModelLoadedInDSP[slot] = true;
      modelsChanged = true;
    }
  }
  // The reference slot -- and therefore every gain derived from it -- can move when any slot changes, so these are
  // recomputed once for the whole set rather than per slot.
  if (modelsChanged)
  {
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
    _SetSlotTrims();
    // Which slots are loaded decides whether a solo is in effect at all, so the blend gains have to follow too.
    _SetBlendGains();
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
}

void NeuralAmpModeler::_BlendModels(sample** triggerOutput, const size_t numFrames)
{
  const int nFrames = (int)numFrames;
  bool anyModel = false;

  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mModel[slot] == nullptr)
      continue;
    anyModel = true;

    // The reference slot's trim is exactly 1.0, so it gets the shared buffer without a copy. That keeps the
    // single-model signal path identical to what it was before blending existed.
    sample** modelInput = triggerOutput;
    if (mSlotInputTrim[slot] != 1.0)
    {
      const double trim = mSlotInputTrim[slot];
      sample* dest = mSlotInputArrays[slot][0].data();
      for (size_t s = 0; s < numFrames; s++)
        dest[s] = trim * triggerOutput[0][s];
      modelInput = mSlotInputPointers[slot];
    }
    mModel[slot]->process(modelInput, mSlotOutputPointers[slot], nFrames);
    // Line this slot up with whichever slot has the most latency.
    mSlotDelays[slot].Process(mSlotOutputArrays[slot][0].data(), nFrames);
  }

  if (!anyModel)
  {
    // Keep the ramp's starting point current even while nothing is loaded, so that the first block after a model
    // finally loads doesn't ramp away from a gain that was never actually applied.
    for (size_t slot = 0; slot < kNumModelSlots; slot++)
      mSlotBlendGainPrev[slot] = mSlotBlendGain[slot] * mSlotOutputTrim[slot];
    _FallbackDSP(triggerOutput, mOutputPointers, kNumChannelsInternal, numFrames);
    return;
  }

  sample* out = mOutputArray[0].data();
  std::fill(out, out + numFrames, 0.0);
  const double rampScale = numFrames > 0 ? 1.0 / (double)numFrames : 0.0;
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    const double target = mSlotBlendGain[slot] * mSlotOutputTrim[slot];
    const double start = mSlotBlendGainPrev[slot];
    mSlotBlendGainPrev[slot] = target;
    if (mModel[slot] == nullptr || (start == 0.0 && target == 0.0))
      continue;
    // Ramp across the block. A polarity flip is a full sign change and would click otherwise.
    const double step = (target - start) * rampScale;
    const sample* in = mSlotOutputArrays[slot][0].data();
    for (size_t s = 0; s < numFrames; s++)
      out[s] += (start + step * (double)s) * in[s];
  }
}

void NeuralAmpModeler::_DeallocateIOPointers()
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
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    delete[] mSlotInputPointers[slot];
    mSlotInputPointers[slot] = nullptr;
    delete[] mSlotOutputPointers[slot];
    mSlotOutputPointers[slot] = nullptr;
  }
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      mOutputArray[c][s] = mInputArray[c][s];
}

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  // Models
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mStagedModel[slot] != nullptr)
    {
      mStagedModel[slot]->Reset(sampleRate, maxBlockSize);
    }
    else if (mModel[slot] != nullptr)
    {
      mModel[slot]->Reset(sampleRate, maxBlockSize);
    }
  }

  // IR
  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
}

int NeuralAmpModeler::_ReferenceSlot() const
{
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mModel[slot] != nullptr)
      return (int)slot;
  }
  return -1;
}

bool NeuralAmpModeler::_AnyModelIsSlimmable() const
{
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mModel[slot] != nullptr && mModel[slot]->GetSlimmableModel() != nullptr)
      return true;
  }
  return false;
}

void NeuralAmpModeler::_SendToSlotBrowsers(const size_t slot, const int msgTag, const int dataSize, const void* pData)
{
  // Slot 0 is shown twice: on the main page and on the blend page.
  if (slot == 0)
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, msgTag, dataSize, pData);
  }
  const int blendTags[kNumModelSlots] = {
    kCtrlTagBlendModelFileBrowser1, kCtrlTagBlendModelFileBrowser2, kCtrlTagBlendModelFileBrowser3};
  SendControlMsgFromDelegate(blendTags[slot], msgTag, dataSize, pData);
}

void NeuralAmpModeler::_SeedFolderIntoEmptySlots(const WDL_String& modelPath, const size_t sourceSlot)
{
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (slot == sourceSlot)
      continue;
    // Only offer it to slots the user hasn't put anything in yet -- never step on a slot that already has a model,
    // or one that's mid-load.
    if (mModel[slot] != nullptr || mStagedModel[slot] != nullptr || mNAMPath[slot].GetLength())
      continue;
    _SendToSlotBrowsers(slot, kMsgTagSeedFolder, modelPath.GetLength(), modelPath.Get());
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration, referenced to the first loaded slot.
  const int ref = _ReferenceSlot();
  if ((ref >= 0) && (mModel[ref]->HasInputLevel()) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - mModel[ref]->GetInputLevel();
  }
  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  const int ref = _ReferenceSlot();
  if (ref >= 0)
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mModel[ref]->HasLoudness())
        {
          const double loudness = mModel[ref]->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mModel[ref]->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mModel[ref]->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
  mOutputGain = DBToAmp(gainDB);
}

void NeuralAmpModeler::_SetSlotTrims()
{
  // mInputGain and mOutputGain already carry the reference slot's calibration. What's left is to bring the other
  // slots to the same place, so that the blend balances the models rather than their metadata.
  const int ref = _ReferenceSlot();
  const int outputMode = GetParam(kOutputMode)->Int();
  const bool calibrateInput = GetParam(kCalibrateInput)->Bool();

  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    double inputTrimDB = 0.0;
    double outputTrimDB = 0.0;

    if (ref >= 0 && (int)slot != ref && mModel[slot] != nullptr)
    {
      // Not const: nam::DSP's metadata accessors aren't const-qualified.
      ResamplingNAM* reference = mModel[ref].get();
      ResamplingNAM* model = mModel[slot].get();

      if (calibrateInput && reference->HasInputLevel() && model->HasInputLevel())
      {
        inputTrimDB = reference->GetInputLevel() - model->GetInputLevel();
      }
      switch (outputMode)
      {
        case 1: // Normalized
          if (reference->HasLoudness() && model->HasLoudness())
          {
            outputTrimDB = reference->GetLoudness() - model->GetLoudness();
          }
          break;
        case 2: // Calibrated
          if (reference->HasOutputLevel() && model->HasOutputLevel())
          {
            outputTrimDB = model->GetOutputLevel() - reference->GetOutputLevel();
          }
          break;
        case 0: // Raw
        default: break;
      }
    }

    mSlotInputTrim[slot] = inputTrimDB == 0.0 ? 1.0 : DBToAmp(inputTrimDB);
    mSlotOutputTrim[slot] = outputTrimDB == 0.0 ? 1.0 : DBToAmp(outputTrimDB);
  }
}

bool NeuralAmpModeler::_SlotIsAudible(const size_t slot)
{
  if (GetParam((int)(kBlendMute1 + slot))->Bool())
  {
    return false;
  }
  // Solo silences everything that isn't soloed. Only loaded slots count towards "is anything soloed", so that a solo
  // left on an empty row doesn't silence the whole blend.
  bool anySolo = false;
  for (size_t i = 0; i < kNumModelSlots; i++)
  {
    if (mModel[i] != nullptr && GetParam((int)(kBlendSolo1 + i))->Bool())
    {
      anySolo = true;
      break;
    }
  }
  return !anySolo || GetParam((int)(kBlendSolo1 + slot))->Bool();
}

void NeuralAmpModeler::_SetBlendGain(const size_t slot)
{
  if (!_SlotIsAudible(slot))
  {
    mSlotBlendGain[slot] = 0.0;
    return;
  }
  const double levelDB = GetParam((int)(kBlendLevel1 + slot))->Value();
  const double amp = levelDB <= kBlendMuteDB ? 0.0 : DBToAmp(levelDB);
  const bool invert = GetParam((int)(kBlendInvert1 + slot))->Bool();
  mSlotBlendGain[slot] = invert ? -amp : amp;
}

void NeuralAmpModeler::_SetBlendGains()
{
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    _SetBlendGain(slot);
  }
}

void NeuralAmpModeler::_ApplySlimParamToLoadedNAMs()
{
  const double v = GetParam(kSlim)->Value();
  auto apply = [v](ResamplingNAM* p) {
    if (p == nullptr)
      return;
    if (nam::SlimmableModel* s = p->GetSlimmableModel())
      s->SetSlimmableSize(v);
  };
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    apply(mModel[slot].get());
    apply(mStagedModel[slot].get());
  }
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath, const size_t slot)
{
  WDL_String previousNAMPath = mNAMPath[slot];
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);

    // Check that the model has 1 input and 1 output channel
    if (model->NumInputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 input channel, but has " + std::to_string(model->NumInputChannels()));
    }
    if (model->NumOutputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 output channel, but has "
                               + std::to_string(model->NumOutputChannels()));
    }

    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    if (nam::SlimmableModel* slimmable = temp->GetSlimmableModel())
    {
      slimmable->SetSlimmableSize(GetParam(kSlim)->Value());
    }
    mStagedModel[slot] = std::move(temp);
    mNAMPath[slot] = modelPath;
    _SendToSlotBrowsers(slot, kMsgTagLoadedModel, mNAMPath[slot].GetLength(), mNAMPath[slot].Get());
    _SeedFolderIntoEmptySlots(mNAMPath[slot], slot);
  }
  catch (std::runtime_error& e)
  {
    _SendToSlotBrowsers(slot, kMsgTagLoadFailed);

    if (mStagedModel[slot] != nullptr)
    {
      mStagedModel[slot] = nullptr;
    }
    mNAMPath[slot] = previousNAMPath;
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

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}
void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
    for (size_t slot = 0; slot < kNumModelSlots; slot++)
    {
      mSlotInputArrays[slot].resize(numChannels);
      mSlotOutputArrays[slot].resize(numChannels);
    }
  }
  if (updateFrames)
  {
    auto resizeAndClear = [numFrames](std::vector<std::vector<iplug::sample>>& array) {
      for (auto c = 0; c < array.size(); c++)
      {
        array[c].resize(numFrames);
        std::fill(array[c].begin(), array[c].end(), 0.0);
      }
    };
    resizeAndClear(mInputArray);
    resizeAndClear(mOutputArray);
    for (size_t slot = 0; slot < kNumModelSlots; slot++)
    {
      resizeAndClear(mSlotInputArrays[slot]);
      resizeAndClear(mSlotOutputArrays[slot]);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    for (auto c = 0; c < mSlotInputArrays[slot].size(); c++)
      mSlotInputPointers[slot][c] = mSlotInputArrays[slot][c].data();
    for (auto c = 0; c < mSlotOutputArrays[slot].size(); c++)
      mSlotOutputPointers[slot][c] = mSlotOutputArrays[slot][c].data();
  }
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = mInputGain;
#ifndef APP_API
  gain /= (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
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

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  const int ref = _ReferenceSlot();
  if (ref < 0)
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    // The model info panel describes the reference slot.
    ResamplingNAM* reference = mModel[ref].get();
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = reference->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = reference->HasInputLevel();
    modelInfo.inputCalibrationLevel.value = reference->HasInputLevel() ? reference->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = reference->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value = reference->HasOutputLevel() ? reference->GetOutputLevel() : 0.0;

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !reference->HasInputLevel();
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      // These modes are only meaningful if *every* loaded model can be levelled that way; otherwise one of them would
      // sit at the wrong level in the blend. So take the intersection.
      bool allHaveLoudness = true;
      bool allHaveOutputLevel = true;
      for (size_t slot = 0; slot < kNumModelSlots; slot++)
      {
        if (mModel[slot] == nullptr)
          continue;
        allHaveLoudness = allHaveLoudness && mModel[slot]->HasLoudness();
        allHaveOutputLevel = allHaveOutputLevel && mModel[slot]->HasOutputLevel();
      }
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!allHaveLoudness);
      c->SetCalibratedDisable(!allHaveOutputLevel);
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      pSlimIcon->Hide(!_AnyModelIsSlimmable());
    }
  }
}

void NeuralAmpModeler::_UpdateLatency()
{
  // Slots don't all report the same latency, so report the largest and delay the others up to match it. Blending
  // signals that aren't time-aligned would comb-filter.
  int latency = 0;
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    if (mModel[slot] != nullptr)
    {
      latency = std::max(latency, mModel[slot]->GetLatency());
    }
  }
  for (size_t slot = 0; slot < kNumModelSlots; slot++)
  {
    mSlotDelays[slot].SetDelay(mModel[slot] != nullptr ? latency - mModel[slot]->GetLatency() : 0);
  }
  // Other things that add latency here...

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// HACK
#include "Unserialization.cpp"
