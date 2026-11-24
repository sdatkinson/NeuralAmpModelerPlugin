#include <algorithm> // std::clamp, std::min
#include <cctype>
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <limits>
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
  GetParam(kCabToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
  mNoiseGateTrigger.AddListener(&mNoiseGateGainRight);

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
    const auto cabIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto cabIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);

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

    // Areas for model and cab
    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto cabYOffset = 38.0f;
    const auto modelArea =
      contentArea.GetFromBottom((2.0f * fileHeight)).GetFromTop(fileHeight).GetMidHPadded(fileWidth).GetVShifted(-1);
    const auto modelInfoIconArea =
      IRECT(modelArea.R + 6.f, modelArea.MH() - 9.f, modelArea.R + 6.f + 18.f, modelArea.MH() + 9.f);
    const auto slimIconArea = IRECT(
      modelInfoIconArea.R + 4.f, modelArea.MH() - 14.f, modelInfoIconArea.R + 4.f + 2.f * 28.f, modelArea.MH() + 14.f);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto cabArea = modelArea.GetVShifted(cabYOffset);
    const auto cabSwitchArea = cabArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mNAMPath and mStagedNAM
        const std::string msg = _StageModel(fileName);
        // TODO error messages like the cab loader.
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    // Cab loader button
    auto loadCabCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        const std::string error = _StageCab(fileName);
        if (!error.empty())
        {
          std::stringstream message;
          message << "Failed to load cab file " << fileName.Get() << ":\n" << error;
          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load cab!", kMB_OK);
        }
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "NEURAL AMP MODELER", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultCabString = "Select cab directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultCabString = "Select cab...";
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

    // Shows the model's metadata in its tooltip
    pGraphics
      ->AttachControl(
        new NAMInfoIconControl(modelInfoIconArea, PluginColors::NAM_THEMEFONTCOLOR), kCtrlTagModelInfoIcon)
      ->Hide(true);

    pGraphics->AttachControl(new ISVGSwitchControl(cabSwitchArea, {cabIconOffSVG, cabIconOnSVG}, kCabToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(cabArea, kMsgTagClearCab, defaultCabString.c_str(), "wav nam", loadCabCompletionHandler,
                                style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get cabs", getUrl),
      kCtrlTagCabFileBrowser);
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
    pGraphics->AttachControl(new NAMMeterControl<1>(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl<2>(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

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

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  _ApplyDSPStaging();

  // The host buses have at most two channels. Offset each channel within the host block.
  std::array<sample*, kMaxNumChannelsInternal> chunkInputs{}, chunkOutputs{};
  for (int offset = 0; offset < nFrames;)
  {
    const int chunkFrames = std::min(nFrames - offset, kMaxInternalBlockSize);
    for (size_t ch = 0; ch < numChannelsExternalIn; ++ch)
      chunkInputs[ch] = inputs[ch] + offset;
    for (size_t ch = 0; ch < numChannelsExternalOut; ++ch)
      chunkOutputs[ch] = outputs[ch] + offset;

    _ProcessBlock(chunkInputs.data(), chunkOutputs.data(), chunkFrames, numChannelsExternalIn, numChannelsExternalOut);
    offset += chunkFrames;
  }
}

void NeuralAmpModeler::_ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames,
                                    size_t numChannelsExternalIn, size_t numChannelsExternalOut)
{
  const size_t numModelOutputs = mModel != nullptr ? mModel->NumOutputChannels() : 1;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _ProcessInput(inputs, numFrames, numChannelsExternalIn);
  sample* modelInput[]{mInputBuffer.data()};
  sample* modelOutput[]{mModelOutput[0].data(), mModelOutput[1].data()};
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = modelInput;
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
    triggerOutput = mNoiseGateTrigger.Process(modelInput, 1, numFrames);
  }

  if (mModel != nullptr)
  {
    mModel->process(triggerOutput, modelOutput, nFrames);
  }
  else
  {
    _FallbackDSP(triggerOutput, modelOutput, 1, numFrames); // Fallback is mono-mono
  }
  // Apply the same input-triggered gate envelope to both amp output channels.
  sample* gateGainPointers[]{modelOutput[0], modelOutput[1]};
  if (noiseGateActive)
  {
    gateGainPointers[0] = mNoiseGateGain.Process(modelOutput, 1, numFrames)[0];
    if (numModelOutputs == 2)
      gateGainPointers[1] = mNoiseGateGainRight.Process(modelOutput + 1, 1, numFrames)[0];
  }

  sample** toneStackOutPointers = (toneStackActive && mToneStack != nullptr)
                                    ? mToneStack->Process(gateGainPointers, numModelOutputs, nFrames)
                                    : gateGainPointers;

  size_t numOutputChannels = numModelOutputs;
  sample** cabPointers = toneStackOutPointers;
  sample* cabinetOutputPointers[]{mCabinetOutput[0].data(), mCabinetOutput[1].data()};
  if (mCab != nullptr && GetParam(kCabToggle)->Bool())
  {
    if (mCab->NumOutputChannels() == 2)
    {
      sample* cabInputPointers[]{toneStackOutPointers[0]};
      if (numModelOutputs == 2)
      {
        for (size_t i = 0; i < numFrames; ++i)
          mCabinetInput[i] = 0.5 * (toneStackOutPointers[0][i] + toneStackOutPointers[1][i]);
        cabInputPointers[0] = mCabinetInput.data();
      }
      mCab->process(cabInputPointers, cabinetOutputPointers, nFrames);
      numOutputChannels = 2;
    }
    else
    {
      // Mono cabs process stereo amp outputs independently, preserving L/R.
      mCab->process(toneStackOutPointers, cabinetOutputPointers, nFrames);
      if (numModelOutputs == 2)
        mCabRight->process(toneStackOutPointers + 1, cabinetOutputPointers + 1, nFrames);
    }
    cabPointers = cabinetOutputPointers;
  }

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  sample** hpfPointers = mHighPass.Process(cabPointers, numOutputChannels, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // Adapt the internal mono/stereo result to the host output bus.
  _ProcessOutput(hpfPointers, outputs, numFrames, numOutputChannels, numChannelsExternalOut);
  _UpdateMeters(modelInput, outputs, numFrames, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();

  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or cab loaded, they need to be checked for resampling.
  _ResetModelAndCab(sampleRate);
  mToneStack->Reset(sampleRate, kMaxInternalBlockSize);
  _UpdateLatencyAndTail();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
  if (mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      // FIXME -- need to disable only the "normalized" model
      // pGraphics->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
      static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimKnob))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagModelInfoIcon))
        p->Hide(true);
      // Needed on macOS so that the tooltip regions match the visible controls
      pGraphics->UpdateTooltips();
      pGraphics->SetAllControlsDirty();
      mModelCleared = false;
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
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(mNAMPath.Get());
  chunk.PutStr(mCabPath.Get());
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

  if (mNAMPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
    // If it's not loaded yet, then mark as failed.
    // If it's yet to be loaded, then the completion handler will set us straight once it runs.
    if (mModel == nullptr && mStagedModel == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
  }

  if (mCabPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagCabFileBrowser, kMsgTagLoadedCab, mCabPath.GetLength(), mCabPath.Get());
    if (mCab == nullptr && mStagedCab == nullptr)
      SendControlMsgFromDelegate(kCtrlTagCabFileBrowser, kMsgTagLoadFailed);
  }

  if (mModel != nullptr)
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
    case kInputLevel: _SetInputGain(); break;
    // Changes to the output gain
    case kOutputLevel:
    case kOutputMode: _SetOutputGain(); break;
    // Tone stack:
    case kToneBass: mToneStack->SetParam("bass", GetParam(paramIdx)->Value()); break;
    case kToneMid: mToneStack->SetParam("middle", GetParam(paramIdx)->Value()); break;
    case kToneTreble: mToneStack->SetParam("treble", GetParam(paramIdx)->Value()); break;
    case kSlim: _ApplySlimParamToLoadedNAMs(); break;
    case kCabToggle:
      _SetOutputGain();
      _UpdateLatencyAndTail();
      break;
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
      case kCabToggle: pGraphics->GetControlWithTag(kCtrlTagCabFileBrowser)->SetDisabled(!active); break;
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: mShouldRemoveModel = true; return true;
    case kMsgTagClearCab: mShouldRemoveCab = true; return true;
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
  // Remove marked modules
  if (mShouldRemoveModel)
  {
    mModel = nullptr;
    mNAMPath.Set("");
    mNAMGearMake.clear();
    mNAMGearModel.clear();
    mNAMModeledBy.clear();
    mShouldRemoveModel = false;
    mModelCleared = true;
    _UpdateLatencyAndTail();
    _SetInputGain();
    _SetOutputGain();
  }
  if (mShouldRemoveCab)
  {
    mCab = nullptr;
    mCabRight = nullptr;
    mCabIsWav = false;
    mCabPath.Set("");
    mShouldRemoveCab = false;
    _UpdateLatencyAndTail();
    _SetOutputGain();
  }
  // Move things from staged to live
  if (mStagedModel != nullptr)
  {
    mModel = std::move(mStagedModel);
    mStagedModel = nullptr;
    mNewModelLoadedInDSP = true;
    _UpdateLatencyAndTail();
    _SetInputGain();
    _SetOutputGain();
  }
  if (mStagedCab != nullptr)
  {
    mCab = std::move(mStagedCab);
    mCabRight = std::move(mStagedCabRight);
    mCabIsWav = mStagedCabIsWav;
    mStagedCab = nullptr;
    _UpdateLatencyAndTail();
    _SetOutputGain();
  }
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      outputs[c][s] = inputs[c][s];
}

void NeuralAmpModeler::_ResetModelAndCab(const double sampleRate)
{
  // Model
  if (mStagedModel != nullptr)
  {
    mStagedModel->Reset(sampleRate, kMaxInternalBlockSize);
  }
  else if (mModel != nullptr)
  {
    mModel->Reset(sampleRate, kMaxInternalBlockSize);
  }

  // Core retains the original model data and handles coefficient resampling on Reset.
  if (mStagedCab != nullptr)
  {
    mStagedCab->Reset(sampleRate, kMaxInternalBlockSize);
    if (mStagedCabRight != nullptr)
      mStagedCabRight->Reset(sampleRate, kMaxInternalBlockSize);
  }
  else if (mCab != nullptr)
  {
    mCab->Reset(sampleRate, kMaxInternalBlockSize);
    if (mCabRight != nullptr)
      mCabRight->Reset(sampleRate, kMaxInternalBlockSize);
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration
  if ((mModel != nullptr) && (mModel->HasInputLevel()) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - mModel->GetInputLevel();
  }
  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  if (mModel != nullptr)
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mModel->HasLoudness())
        {
          const double loudness = mModel->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mModel->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mModel->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
  if (mCab != nullptr && mCabIsWav && GetParam(kCabToggle)->Bool())
  {
    // Core compensates by sourceRate / hostRate when resampling IR coefficients.
    // This remaining factor preserves the legacy -18 dB * 48000 / hostRate level.
    gainDB += -18.0 + 20.0 * std::log10(48000.0 / mCab->GetEncapsulatedSampleRate());
  }
  mOutputGain = DBToAmp(gainDB);
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
  apply(mModel.get());
  apply(mStagedModel.get());
  _UpdateLatencyAndTail();
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = mNAMPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    nam::dspData modelData;
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath, modelData);

    // Support mono-input models with mono or stereo output.
    if (model->NumInputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 input channel, but has " + std::to_string(model->NumInputChannels()));
    }
    if (model->NumOutputChannels() < 1 || model->NumOutputChannels() > 2)
    {
      throw std::runtime_error("Model must have 1 or 2 output channels, but has "
                               + std::to_string(model->NumOutputChannels()));
    }

    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), kMaxInternalBlockSize);
    if (nam::SlimmableModel* slimmable = temp->GetSlimmableModel())
    {
      slimmable->SetSlimmableSize(GetParam(kSlim)->Value());
    }
    // Optional metadata fields. Missing or non-string values are treated as unknown.
    // Check the types first: nlohmann::json throws type_error (not a std::runtime_error) on mismatches.
    auto GetMetadataString = [&modelData](const char* key) {
      const nlohmann::json& metadata = modelData.metadata;
      if (!metadata.is_object())
        return std::string();
      const auto it = metadata.find(key);
      if (it == metadata.end() || !it->is_string())
        return std::string();
      return it->get<std::string>();
    };

    mStagedModel = std::move(temp);
    mNAMPath = modelPath;
    // Set last, so they're left untouched if loading fails.
    mNAMGearMake = GetMetadataString("gear_make");
    mNAMGearModel = GetMetadataString("gear_model");
    mNAMModeledBy = GetMetadataString("modeled_by");
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

std::string NeuralAmpModeler::_StageCab(const WDL_String& cabPath)
{
  try
  {
    // Finish loading before publishing the model, so a failed load preserves the current cab and path.
    const auto path = std::filesystem::u8path(cabPath.Get());
    nam::dspData config;
    auto model = nam::get_dsp(path, config);
    if (model->NumInputChannels() != 1 || model->NumOutputChannels() < 1 || model->NumOutputChannels() > 2)
      throw std::runtime_error("Cab model must have 1 input and 1 or 2 output channels.");
    auto cab = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    cab->Reset(GetSampleRate(), kMaxInternalBlockSize);
    std::unique_ptr<ResamplingNAM> right;
    if (cab->NumOutputChannels() == 1)
    {
      // Use the same decoded data so both instances match even if the file changes.
      // Prepare the right instance here so changing amp width needs no model loading in ProcessBlock.
      right = std::make_unique<ResamplingNAM>(nam::get_dsp(config), GetSampleRate());
      right->Reset(GetSampleRate(), kMaxInternalBlockSize);
    }
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    mStagedCabIsWav = extension == ".wav";
    mStagedCabRight = std::move(right);
    mStagedCab = std::move(cab);
    mCabPath = cabPath;
    SendControlMsgFromDelegate(kCtrlTagCabFileBrowser, kMsgTagLoadedCab, mCabPath.GetLength(), mCabPath.Get());
  }
  catch (const std::exception& error)
  {
    SendControlMsgFromDelegate(kCtrlTagCabFileBrowser, kMsgTagLoadFailed);
    std::cerr << "Failed to load cab: " << error.what() << std::endl;
    return error.what();
  }
  return "";
}

void NeuralAmpModeler::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
}
void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn)
{
  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = mInputGain;
#ifndef APP_API
  if (nChansIn > 0)
    gain /= static_cast<double>(nChansIn);
#endif
  // Also handle a disconnected input without replaying the previous block.
  std::fill_n(mInputBuffer.data(), nFrames, 0.0);
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputBuffer[s] = gain * inputs[c][s];
      else
        mInputBuffer[s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  for (size_t cout = 0; cout < nChansOut; ++cout)
    for (size_t s = 0; s < nFrames; ++s)
    {
      const double signal =
        nChansOut == 1 && nChansIn == 2 ? 0.5 * (inputs[0][s] + inputs[1][s]) : inputs[cout % nChansIn][s];
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * signal, -1.0, 1.0);
#else
      outputs[cout][s] = gain * signal;
#endif
    }
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (mModel == nullptr)
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = mModel->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = mModel->HasInputLevel();
    modelInfo.inputCalibrationLevel.value = mModel->HasInputLevel() ? mModel->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = mModel->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value = mModel->HasOutputLevel() ? mModel->GetOutputLevel() : 0.0;

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !mModel->HasInputLevel();
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!mModel->HasLoudness());
      c->SetCalibratedDisable(!mModel->HasOutputLevel());
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      const bool show = mModel->GetSlimmableModel() != nullptr;
      pSlimIcon->Hide(!show);
    }

    if (auto* pInfoIcon = pGraphics->GetControlWithTag(kCtrlTagModelInfoIcon))
    {
      auto KnownOrUnknown = [](const std::string& s) { return s.empty() ? std::string("(Unknown)") : s; };
      const std::string tooltip = "Gear make: " + KnownOrUnknown(mNAMGearMake) + "\nGear model: "
                                  + KnownOrUnknown(mNAMGearModel) + "\nModeled by: " + KnownOrUnknown(mNAMModeledBy);
      pInfoIcon->SetTooltip(tooltip.c_str());
      pInfoIcon->Hide(false);
    }

    // Needed on macOS so that the tooltip regions match the visible controls
    pGraphics->UpdateTooltips();
  }
}

void NeuralAmpModeler::_UpdateLatencyAndTail()
{
  int latency = 0;
  if (mModel)
  {
    latency += mModel->GetLatency();
  }
  const bool cabActive = mCab != nullptr && GetParam(kCabToggle)->Bool();
  if (cabActive)
    latency += mCab->GetLatency();

  // Both wrappers report their models' history lengths in host-rate samples.
  // Allow another two seconds (ten cycles at 5 Hz) for the DC blocker.
  const double tail = 2.0 * GetSampleRate() + (mModel ? mModel->GetPrewarmSamples() : 0)
                      + (cabActive ? mCab->GetPrewarmSamples() : 0);
  SetTailSize(static_cast<int>(std::min(tail, static_cast<double>(std::numeric_limits<int>::max()))));

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansOut)
{
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, 1);
  if (nChansOut > 0)
  {
    // Mirror the host's mono output on both bars.
    sample* meterOutput[]{outputPointer[0], outputPointer[nChansOut == 1 ? 0 : 1]};
    mOutputSender.ProcessBlock(meterOutput, (int)nFrames, kCtrlTagOutputMeter);
  }
}

// HACK
#include "Unserialization.cpp"
