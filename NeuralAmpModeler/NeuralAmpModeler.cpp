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

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
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
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    auto helpSVG = pGraphics->LoadSVG(HELP_FN);
    auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    auto closeButtonSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const IBitmap irSwitchBitmap = pGraphics->LoadBitmap((TOGGLEIR_FN), 2, true);
    const IBitmap switchBitmap = pGraphics->LoadBitmap((TOGGLE_FN), true);
    const IBitmap switchHandleBitmap = pGraphics->LoadBitmap((TOGGLE_HANDLE_FN), true);
    const IBitmap knobRotateBitmap = pGraphics->LoadBitmap(KNOB_FN);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const IRECT b = pGraphics->GetBounds();
    const IRECT mainArea = b.GetPadded(-20);
    const auto content = mainArea.GetPadded(-10);
    const float titleHeight = 50.0f;

    // Area for the Noise gate knob
    const float allKnobsHalfPad = 10.0f;
    const float allKnobsPad = 2.0f * allKnobsHalfPad;

    // Areas for knobs
    const float knobsExtraSpaceBelowTitle = 25.0f;
    const float knobHalfHeight = 70.0f;
    const float knobHeight = 2.0f * knobHalfHeight;
    const float singleKnobPad = 12.0f;
    const auto knobs = content.GetFromTop(knobHeight)
                         .GetReducedFromLeft(allKnobsPad)
                         .GetReducedFromRight(allKnobsPad)
                         .GetTranslated(0.0f, titleHeight + knobsExtraSpaceBelowTitle);
    const IRECT inputKnobArea = knobs.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT noiseGateArea = knobs.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT bassKnobArea = knobs.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT middleKnobArea = knobs.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT trebleKnobArea = knobs.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const IRECT outputKnobArea = knobs.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const float toggleHeight = 40.0f;
    // Area for noise gate toggle
    const float ngAreaHeight = toggleHeight;
    const IRECT ngToggleArea =
      noiseGateArea.GetFromBottom(ngAreaHeight).GetTranslated(-10.f, ngAreaHeight + singleKnobPad - 14.f);
    // Area for EQ toggle
    const float eqAreaHeight = toggleHeight;
    const float eqAreaHalfWidth = 0.5f * middleKnobArea.W();
    const IRECT eqToggleArea = middleKnobArea.GetFromBottom(eqAreaHeight)
                                 .GetTranslated(-10.f, eqAreaHeight + singleKnobPad - 14.f)
                                 .GetMidHPadded(eqAreaHalfWidth);

    // Area for output normalization toggle
    const float outNormAreaHeight = toggleHeight;
    const float outNormAreaHalfWidth = 0.5f * outputKnobArea.W();
    const IRECT outNormToggleArea = outputKnobArea.GetFromBottom(outNormAreaHeight)
                                      .GetTranslated(-10.f, outNormAreaHeight + singleKnobPad - 14.f)
                                      .GetMidHPadded(outNormAreaHalfWidth);

    // Area for IR bypass toggle
    const float irBypassToggleX = 46.f;
    const float irBypassToggleY = 343.f;
    const IRECT irBypassToggleArea = IRECT(irBypassToggleX, irBypassToggleY, irSwitchBitmap);

    // Areas for model and IR
    const float fileWidth = 200.0f;
    const float fileHeight = 30.0f;
    const float fileYSpace = 8.0f;
    const float irYOffset = 38.0f;
    const IRECT modelArea = content.GetFromBottom((2.0f * fileHeight) + fileYSpace)
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

    auto themeBG = pGraphics->LoadBitmap(EH_SKIN_FN);
    pGraphics->AttachControl(new IBitmapControl(pGraphics->GetBounds(), themeBG, kNoParameter))
      ->SetBlend(IBlend(EBlend::Default, 1.0));

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mNAMPath and mStagedNAM
        const std::string msg = this->_StageModel(fileName);
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
        const dsp::wav::LoadReturnCode retCode = this->_StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          GetUI()->ShowMessageBox(message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    pGraphics->AttachControl(new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(),
                                                       "nam", loadModelCompletionHandler, style, fileSVG,
                                                       closeButtonSVG, leftArrowSVG, rightArrowSVG),
                             kCtrlTagModelFileBrowser);
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, closeButtonSVG, leftArrowSVG, rightArrowSVG),
      kCtrlTagIRFileBrowser);

    // TODO all these magic numbers
    pGraphics->AttachControl(new NAMSwitchControl(
      ngToggleArea.GetFromTop(60.f).GetPadded(-20.f), kNoiseGateActive, "", style, switchBitmap, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(
      eqToggleArea.GetFromTop(60.f).GetPadded(-20.f), kEQActive, "", style, switchBitmap, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(outNormToggleArea.GetFromTop(32.f).GetPadded(-20.f), kOutNorm, "",
                                                  style, switchBitmap, switchHandleBitmap),
                             kCtrlTagOutNorm);
    // Get those labels on
    {
      const float labelNudgeX = 11.f;
      const float labelNudgeY = 15.f;
      pGraphics->AttachControl(
        new ITextControl(eqToggleArea.GetFromTop(70.f).GetTranslated(labelNudgeX, labelNudgeY), "EQ", style.labelText));
      pGraphics->AttachControl(new ITextControl(
        outNormToggleArea.GetFromTop(70.f).GetTranslated(labelNudgeX, labelNudgeY), "Normalize", style.labelText));
    }

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobRotateBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobRotateBitmap));
    pGraphics->AttachControl(new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobRotateBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(middleKnobArea, kToneMid, "", style, knobRotateBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobRotateBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobRotateBitmap));

    // toggle IR on / off
    pGraphics->AttachControl(new IBSwitchControl(irBypassToggleArea, irSwitchBitmap, kIRToggle));

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
        pGraphics->GetControlWithTag(kCtrlTagAboutBox)->As<IAboutBoxControl>()->HideAnimated(false);
      },
      helpSVG));

    pGraphics
      ->AttachControl(
        new IAboutBoxControl(
          b, COLOR_GRAY,
          // AttachFunc
          [](IContainerBase* pParent, const IRECT& r) {
            pParent->AddChildControl(new IPanelControl(
              IRECT(),
              IPattern::CreateLinearGradient(
                r, EDirection::Vertical, {{PluginColors::NAM_THEMEFONTCOLOR, 0.f}, {PluginColors::NAM_0, 1.f}})));

            pParent->AddChildControl(new IVPanelControl(IRECT(), "",
                                                        style.WithColor(kFR, PluginColors::NAM_1.WithOpacity(0.9f))
                                                          .WithColor(kFG, PluginColors::NAM_1.WithOpacity(0.9f))));

            pParent->AddChildControl(new IVLabelControl(
              IRECT(), "Neural Amp Modeler",
              style.WithDrawFrame(false).WithValueText({30, EAlign::Center, PluginColors::HELP_TEXT})));

            WDL_String versionStr{"Version "};
            versionStr.Append(PLUG_VERSION_STR);
            pParent->AddChildControl(new IVLabelControl(
              IRECT(), versionStr.Get(),
              style.WithDrawFrame(false).WithValueText({DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT})));
            pParent->AddChildControl(new IVLabelControl(
              IRECT(), "By Steven Atkinson",
              style.WithDrawFrame(false).WithValueText({DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT})));
            pParent->AddChildControl(new IURLControl(IRECT(), "Train your own model",
                                                     "https://github.com/sdatkinson/neural-amp-modeler",
                                                     {DEFAULT_TEXT_SIZE, PluginColors::HELP_TEXT}));
          },
          // ResizeFunc
          [](IContainerBase* pParent, const IRECT& r) {
            const IRECT mainArea = r.GetPadded(-20);
            const auto content = mainArea.GetPadded(-10);
            const auto titleLabel = content.GetFromTop(50);
            pParent->GetChild(0)->SetTargetAndDrawRECTs(r);
            pParent->GetChild(1)->SetTargetAndDrawRECTs(mainArea);
            pParent->GetChild(2)->SetTargetAndDrawRECTs(titleLabel);
            pParent->GetChild(3)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H()));
            pParent->GetChild(4)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H() + 20));
            pParent->GetChild(5)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H() + 40));
          },
          // Animation Time
          0),
        kCtrlTagAboutBox)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
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
  const size_t numChannelsInternal = kNumChannelsInternal;
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

  if (mModel != nullptr)
  {
    mModel->SetNormalize(this->GetParam(kOutNorm)->Value());
    // TODO remove input / output gains from here.
    const double inputGain = 1.0;
    const double outputGain = 1.0;
    const int nChans = (int)numChannelsInternal;
    mModel->process(triggerOutput, this->mOutputPointers, nChans, nFrames, inputGain, outputGain, mNAMParams);
    mModel->finalize_(nFrames);
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

  if (this->mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
      pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!this->mModel->HasLoudness());

    this->mNewModelLoadedInDSP = false;
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
    this->_StageModel(this->mNAMPath);
  if (this->mIRPath.GetLength())
    this->_StageIR(this->mIRPath);
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
  if (this->mModel != nullptr)
    this->GetUI()->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(!this->mModel->HasLoudness());
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
  if (this->mStagedModel != nullptr)
  {
    // Move from staged to active DSP
    this->mModel = std::move(this->mStagedModel);
    this->mStagedModel = nullptr;
    this->mNewModelLoadedInDSP = true;
  }
  if (this->mStagedIR != nullptr)
  {
    this->mIR = std::move(this->mStagedIR);
    this->mStagedIR = nullptr;
  }
  // Remove marked modules
  if (this->mShouldRemoveModel)
  {
    this->mModel = nullptr;
    this->mNAMPath.Set("");
    this->mShouldRemoveModel = false;
  }
  if (this->mShouldRemoveIR)
  {
    this->mIR = nullptr;
    this->mIRPath.Set("");
    this->mShouldRemoveIR = false;
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

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = this->mNAMPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    mStagedModel = get_dsp(dspPath);
    this->mNAMPath = modelPath;
    SendControlMsgFromDelegate(
      kCtrlTagModelFileBrowser, kMsgTagLoadedModel, this->mNAMPath.GetLength(), this->mNAMPath.Get());
  }
  catch (std::exception& e)
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);

    if (this->mStagedModel != nullptr)
    {
      this->mStagedModel = nullptr;
    }
    this->mNAMPath = previousNAMPath;
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
