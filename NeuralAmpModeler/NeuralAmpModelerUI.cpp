#include "NeuralAmpModeler.h"

#include "NeuralAmpModelerControls.h"
#include "IWebViewControl.h"

#define AC pGraphics->AttachControl

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();
    
    switch (paramIdx)
    {
      case kNoiseGateActive:
        GetUI()->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active);
        break;
      case kEQActive:
        GetUI()->GetControlWithParamIdx(kToneBass)->SetDisabled(!active);
        GetUI()->GetControlWithParamIdx(kToneMid)->SetDisabled(!active);
        GetUI()->GetControlWithParamIdx(kToneTreble)->SetDisabled(!active);
        break;
      default:
        break;
    }
  }
}

void NeuralAmpModeler::LayoutUI(IGraphics* pGraphics)
{
  pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
  pGraphics->AttachPanelBackground(COLOR_BLACK);
  pGraphics->EnableMouseOver(true);
  auto helpSVG = pGraphics->LoadSVG(HELP_FN);
  auto fileSVG = pGraphics->LoadSVG(FILE_FN);
  auto closeButtonSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
  pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
  const IRECT b = pGraphics->GetBounds();
  const IRECT mainArea = b.GetPadded(-20);
  const auto content = mainArea.GetPadded(-10);
  const float titleHeight = 50.0f;
  const auto titleLabel = content.GetFromTop(titleHeight);

  // Area for the Noise gate knob
  const float allKnobsHalfPad = 10.0f;
  const float allKnobsPad = 2.0f * allKnobsHalfPad;

  // Areas for knobs
  const float knobsExtraSpaceBelowTitle = 25.0f;
  const float knobHalfHeight = 70.0f;
  const float knobHeight = 2.0f * knobHalfHeight;
  const float singleKnobPad = 10.0f;
  const auto knobs = content.GetFromTop(knobHeight)
                       .GetReducedFromLeft(allKnobsPad)
                       .GetReducedFromRight(allKnobsPad)
                       .GetTranslated(0.0f, titleHeight + knobsExtraSpaceBelowTitle);
  const IRECT inputKnobArea = knobs.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
  const IRECT noiseGateArea = knobs.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-10);
  const IRECT bassKnobArea = knobs.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
  const IRECT middleKnobArea = knobs.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
  const IRECT trebleKnobArea = knobs.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
  const IRECT outputKnobArea = knobs.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

  // Area for EQ toggle
  const float ngAreaHeight = 20.0f;
  const float ngAreaHalfWidth = 0.5f * noiseGateArea.W();
  const IRECT ngToggleArea = noiseGateArea.GetFromBottom(ngAreaHeight)
                               .GetTranslated(0.0f, ngAreaHeight + singleKnobPad)
                               .GetMidHPadded(ngAreaHalfWidth);

  // Area for EQ toggle
  const float eqAreaHeight = 20.0f;
  const float eqAreaHalfWidth = 0.5f * middleKnobArea.W();
  const IRECT eqToggleArea = middleKnobArea.GetFromBottom(eqAreaHeight)
                               .GetTranslated(0.0f, eqAreaHeight + singleKnobPad)
                               .GetMidHPadded(eqAreaHalfWidth);

  // Areas for model and IR
  const float fileWidth = 250.0f;
  const float fileHeight = 30.0f;
  const float fileSpace = 10.0f;
  const IRECT modelArea =
    content.GetFromBottom(2.0f * fileHeight + fileSpace).GetFromTop(fileHeight).GetMidHPadded(fileWidth);
  const IRECT irArea = content.GetFromBottom(fileHeight).GetMidHPadded(fileWidth);

  // Areas for meters
  const float meterHalfHeight = 0.5f * 250.0f;
  const IRECT inputMeterArea = inputKnobArea.GetFromLeft(allKnobsHalfPad)
                                 .GetMidHPadded(allKnobsHalfPad)
                                 .GetMidVPadded(meterHalfHeight)
                                 .GetTranslated(-allKnobsPad, 0.0f);
  const IRECT outputMeterArea = outputKnobArea.GetFromRight(allKnobsHalfPad)
                                  .GetMidHPadded(allKnobsHalfPad)
                                  .GetMidVPadded(meterHalfHeight)
                                  .GetTranslated(allKnobsPad, 0.0f);

  const IRECT webViewArea =
    content.GetReducedFromTop(knobs.B).GetReducedFromBottom(fileHeight * 2 + fileSpace).GetVPadded(-10.0f);

  // auto tolexPNG = pGraphics->LoadBitmap(TOLEX_FN);
  // AC(new IBitmapControl(pGraphics->GetBounds(),
  // tolexPNG, kNoParameter))->SetBlend(IBlend(EBlend::Default, 0.5));
  //  The background inside the outermost border
  AC(new IVPanelControl(mainArea, "", style.WithColor(kFG, PluginColors::NAM_1))); // .WithContrast(-0.75)
  AC(new IVLabelControl(titleLabel, "Neural Amp Modeler",
                        style.WithDrawFrame(false).WithValueText({30, EAlign::Center, PluginColors::NAM_3})));

  // Model loader button
  auto loadNAM = [&, pGraphics](IControl* pCaller) {
    WDL_String initFileName;
    WDL_String initPath(mNAMPath.Get());
    initPath.remove_filepart();
    pGraphics->PromptForFile(
      initFileName, initPath, EFileAction::Open, "nam", [&](const WDL_String& fileName, const WDL_String& path) {
        if (fileName.GetLength())
        {
          // Sets mNAMPath and mStagedNAM
          const std::string msg = GetNAM(fileName);
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
            pGraphics->ShowMessageBox(ss.str().c_str(), "Failed to load model!", kMB_OK);
          }
        }
      });
  };
  // IR loader button
  auto loadIR = [&, pGraphics](IControl* pCaller) {
    WDL_String initFileName;
    WDL_String initPath(mIRPath.Get());
    initPath.remove_filepart();
    pGraphics->PromptForFile(
      initFileName, initPath, EFileAction::Open, "wav", [&](const WDL_String& fileName, const WDL_String& path) {
        if (fileName.GetLength())
        {
          mIRPath = fileName;
          const dsp::wav::LoadReturnCode retCode = GetIR(fileName);
          if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
          {
            std::stringstream message;
            message << "Failed to load IR file " << fileName.Get() << ":\n";
            switch (retCode)
            {
              case (dsp::wav::LoadReturnCode::ERROR_OPENING):
                message << "Failed to open file (is it being used by another "
                           "program?)";
                break;
              case (dsp::wav::LoadReturnCode::ERROR_NOT_RIFF): message << "File is not a WAV file."; break;
              case (dsp::wav::LoadReturnCode::ERROR_NOT_WAVE): message << "File is not a WAV file."; break;
              case (dsp::wav::LoadReturnCode::ERROR_MISSING_FMT):
                message << "File is missing expected format chunk.";
                break;
              case (dsp::wav::LoadReturnCode::ERROR_INVALID_FILE): message << "WAV file contents are invalid."; break;
              case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_ALAW):
                message << "Unsupported file format \"A-law\"";
                break;
              case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_MULAW):
                message << "Unsupported file format \"mu-law\"";
                break;
              case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_FORMAT_EXTENSIBLE):
                message << "Unsupported file format \"extensible\"";
                break;
              case (dsp::wav::LoadReturnCode::ERROR_NOT_MONO): message << "File is not mono."; break;
              case (dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_BITS_PER_SAMPLE):
                message << "Unsupported bits per sample";
                break;
              case (dsp::wav::LoadReturnCode::ERROR_OTHER): message << "???"; break;
              default: message << "???"; break;
            }
            pGraphics->ShowMessageBox(message.str().c_str(), "Failed to load IR!", kMB_OK);
          }
        }
      });
  };
  // Model-clearing function
  auto ClearNAM = [&, pGraphics](IControl* pCaller) { mFlagRemoveNAM = true; };
  // IR-clearing function
  auto ClearIR = [&, pGraphics](IControl* pCaller) { mFlagRemoveIR = true; };

  // Graphics objects for what NAM is loaded
  const float iconWidth = fileHeight; // Square icon
  AC(new IVPanelControl(modelArea, "", style.WithColor(kFG, PluginColors::NAM_1)));
  AC(new IRolloverSVGButtonControl(modelArea.GetFromLeft(iconWidth).GetPadded(-2.f), loadNAM, fileSVG));
  AC(new IRolloverSVGButtonControl(modelArea.GetFromRight(iconWidth).GetPadded(-2.f), ClearNAM, closeButtonSVG));
  AC(new IVUpdateableLabelControl(
       modelArea.GetReducedFromLeft(iconWidth).GetReducedFromRight(iconWidth), mDefaultNAMString.Get(),
       style.WithDrawFrame(false).WithValueText(style.valueText.WithVAlign(EVAlign::Middle))),
     kCtrlTagModelName);
  // IR
  AC(new IVPanelControl(irArea, "", style.WithColor(kFG, PluginColors::NAM_1)));
  AC(new IRolloverSVGButtonControl(irArea.GetFromLeft(iconWidth).GetPadded(-2.f), loadIR, fileSVG));
  AC(new IRolloverSVGButtonControl(irArea.GetFromRight(iconWidth).GetPadded(-2.f), ClearIR, closeButtonSVG));
  AC(new IVUpdateableLabelControl(
       irArea.GetReducedFromLeft(iconWidth).GetReducedFromRight(iconWidth), mDefaultIRString.Get(),
       style.WithDrawFrame(false).WithValueText(style.valueText.WithVAlign(EVAlign::Middle))),
     kCtrlTagIRName);

  // NG toggle
  AC(new IVSlideSwitchControl(ngToggleArea, kNoiseGateActive, "Gate",
                              style.WithShowLabel(false).WithValueText(style.valueText.WithSize(13.0f)),
                              true, // valueInButton
                              EDirection::Horizontal));
  // Tone stack toggle
  AC(new IVSlideSwitchControl(eqToggleArea, kEQActive, "EQ", style.WithShowLabel(false).WithValueText(style.valueText.WithSize(13.0f)),
    true, // valueInButton
    EDirection::Horizontal));

  // The knobs
  // Input
  AC(new IVKnobControl(inputKnobArea, kInputLevel, "", style));
  // Noise gate
  const bool noiseGateIsActive = GetParam(kNoiseGateActive)->Value();
  const IVStyle noiseGateInitialStyle = noiseGateIsActive ? style : styleInactive;
  AC(new IVKnobControl(noiseGateArea, kNoiseGateThreshold, "", noiseGateInitialStyle));

  // Tone stack
  const bool toneStackIsActive = GetParam(kEQActive)->Value();
  const IVStyle toneStackInitialStyle = toneStackIsActive ? style : styleInactive;
  AC(new IVKnobControl(bassKnobArea, kToneBass, "", toneStackInitialStyle));
  AC(new IVKnobControl(middleKnobArea, kToneMid, "", toneStackInitialStyle));
  AC(new IVKnobControl(trebleKnobArea, kToneTreble, "", toneStackInitialStyle));
  // Output
  AC(new IVKnobControl(outputKnobArea, kOutputLevel, "", style));

  // The meters
  const float meterMin = -90.0f;
  const float meterMax = -0.01f;
  pGraphics
    ->AttachControl(
      new IVPeakAvgMeterControl(inputMeterArea, "",
                                style.WithWidgetFrac(0.5).WithShowValue(false).WithColor(kFG, PluginColors::NAM_2),
                                EDirection::Vertical, {}, 0, meterMin, meterMax, {}),
      kCtrlTagInputMeter)
    ->As<IVPeakAvgMeterControl<>>()
    ->SetPeakSize(2.0f);
  pGraphics
    ->AttachControl(
      new IVPeakAvgMeterControl(outputMeterArea, "",
                                style.WithWidgetFrac(0.5).WithShowValue(false).WithColor(kFG, PluginColors::NAM_2),
                                EDirection::Vertical, {}, 0, meterMin, meterMax, {}),
      kCtrlTagOutputMeter)
    ->As<IVPeakAvgMeterControl<>>()
    ->SetPeakSize(2.0f);

  //  AC(new IWebViewControl(webViewArea, true, [](IWebViewControl* pWebView) { pWebView->LoadHTML("I am a web view");
  //  }), kCtrlTagWebView);
  //     Help/about box
  AC(new IRolloverCircleSVGButtonControl(
    mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20),
    [pGraphics](IControl* pCaller) {
      //        pGraphics->GetControlWithTag(kCtrlTagWebView)->Hide(true);
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
            IRECT(), IPattern::CreateLinearGradient(
                       r, EDirection::Vertical, {{PluginColors::NAM_3, 0.f}, {PluginColors::NAM_1, 1.f}})));

          pParent->AddChildControl(new IVPanelControl(IRECT(), "",
                                                      style.WithColor(kFR, PluginColors::NAM_3.WithOpacity(0.1f))
                                                        .WithColor(kFG, PluginColors::NAM_1.WithOpacity(0.1f))));

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
          pParent->AddChildControl(new IURLControl(
            IRECT(), "Built with iPlug2", "https://iPlug2.github.io", {DEFAULT_TEXT_SIZE, PluginColors::HELP_TEXT}));
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
          pParent->GetChild(5)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H() + 50));
          pParent->GetChild(6)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H() + 100));
        },
        // Animation Time
        0),
      kCtrlTagAboutBox)
    ->Hide(true);
}
