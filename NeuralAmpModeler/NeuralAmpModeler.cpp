#include <filesystem>
#include <iostream>
#include <utility>
#include <cmath>

#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

using namespace iplug;
using namespace igraphics;

const IColor COLOR_MOUSEOVER = COLOR_GREEN.WithOpacity(0.3);

class IRolloverSVGButtonControl : public ISVGButtonControl
{
public:
  IRolloverSVGButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }
  
  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRect(COLOR_MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

class IRolloverCircleSVGButtonControl : public ISVGButtonControl
{
public:
  IRolloverCircleSVGButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }
  
  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillEllipse(COLOR_MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

class IVUpdateableLabelControl : public IVLabelControl
{
public:
  IVUpdateableLabelControl(const IRECT& bounds, const char* str, const IVStyle& style)
  : IVLabelControl(bounds, str, style)
  {
  }
  
  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData)
  {
    SetStr(reinterpret_cast<const char*>(pData));
  }
};

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kInputGain)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kOutputGain)->InitGain("Output", 0.0, -20.0, 20.0, 0.1);
  GetParam(kParametricDrive)->InitDouble("Drive", 0.5, 0.0, 1.0, 0.01);
  GetParam(kParametricLevel)->InitDouble("Level", 0.5, 0.0, 1.0, 0.01);
  GetParam(kParametricTone)->InitDouble("Tone", 0.5, 0.0, 1.0, 0.01);

  try {
     mDSP = get_hard_dsp();
  }
  catch (std::exception& e) {
    std::cerr << "Failed to read hard coded DSP" << std::endl;
    std::cerr << e.what() << std::endl;
  }
  
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_BLACK);
    pGraphics->EnableMouseOver(true);
    auto helpSVG = pGraphics->LoadSVG(HELP_FN);
    auto folderSVG = pGraphics->LoadSVG(FOLDER_FN);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    const IRECT b = pGraphics->GetBounds();
    const IRECT mainArea = b.GetPadded(-20);
    const auto content = mainArea.GetPadded(-10);
    const auto titleLabel = content.GetFromTop(50);
    const auto knobs = content.GetReducedFromLeft(10).GetReducedFromRight(10).GetMidVPadded(70);
    const auto modelArea = content.GetFromBottom(30).GetMidHPadded(150);
    const auto meterArea = content.GetFromBottom(50).GetFromTop(20);

    const IVStyle style {
      true, // Show label
      true, // Show value
      {
        DEFAULT_BGCOLOR, // Background
        COLOR_GREEN.WithOpacity(0.2), // Foreground
        COLOR_GREEN.WithOpacity(0.4), // Pressed
        COLOR_LIGHT_GRAY, // Frame
        COLOR_MOUSEOVER, // Highlight
        DEFAULT_SHCOLOR, // Shadow
        COLOR_GREEN, // Extra 1
        COLOR_RED, // Extra 2
        DEFAULT_X3COLOR  // Extra 3
      }, // Colors
      {DEFAULT_TEXT_SIZE + 5.f, EVAlign::Middle, COLOR_WHITE},
      {DEFAULT_TEXT_SIZE + 5.f, EVAlign::Bottom, COLOR_WHITE},
      DEFAULT_HIDE_CURSOR,
      DEFAULT_DRAW_FRAME,
      false,
      DEFAULT_EMBOSS,
      0.2f,
      2.f,
      DEFAULT_SHADOW_OFFSET,
      DEFAULT_WIDGET_FRAC,
      DEFAULT_WIDGET_ANGLE
    };
    
    auto tolexPNG = pGraphics->LoadBitmap(TOLEX_FN);
    pGraphics->AttachControl(new IBitmapControl(pGraphics->GetBounds(), tolexPNG, kNoParameter))->SetBlend(IBlend(EBlend::Default, 0.5));
    pGraphics->AttachControl(new IVPanelControl(mainArea, "", style.WithColor(kFG, COLOR_GREEN.WithContrast(-0.75))));
    pGraphics->AttachControl(new IVLabelControl(titleLabel, "Neural Amp Modeler", style
                                                .WithDrawFrame(false)
                                                .WithValueText({30, EAlign::Center, COLOR_WHITE})));

//    pGraphics->AttachControl(new IVBakedPresetManagerControl(modelArea, style.WithValueText({DEFAULT_TEXT_SIZE, EVAlign::Middle, COLOR_WHITE})));

    // Model loader button
    auto loadModel = [&, pGraphics](IControl* pCaller) {
      WDL_String dir;
      pGraphics->PromptForDirectory(dir, [&](const WDL_String& fileName, const WDL_String& path){
        if (path.GetLength())
          GetDSP(path);
      });
    };
    
    pGraphics->AttachControl(new IVPanelControl(modelArea, "", style.WithColor(kFG, COLOR_YELLOW.WithContrast(-0.75))));
    pGraphics->AttachControl(new IRolloverSVGButtonControl(modelArea.GetFromLeft(30).GetPadded(-2.f), loadModel, folderSVG));
    pGraphics->AttachControl(new IVUpdateableLabelControl(modelArea.GetReducedFromLeft(30), "Default model", style.WithDrawFrame(false).WithValueText(style.valueText.WithVAlign(EVAlign::Middle))), kCtrlTagModelName);
    
    pGraphics->AttachControl(new IVKnobControl(knobs.GetGridCell(0, 0, 1, kNumParams).GetPadded(-10), kInputGain, "", style));
    pGraphics->AttachControl(new IVKnobControl(knobs.GetGridCell(0, 1, 1, kNumParams).GetPadded(-10), kOutputGain, "", style));
    pGraphics->AttachControl(new IVKnobControl(knobs.GetGridCell(0, 2, 1, kNumParams).GetPadded(-10), kParametricTone, "", style))->SetDisabled(true);
    pGraphics->AttachControl(new IVKnobControl(knobs.GetGridCell(0, 3, 1, kNumParams).GetPadded(-10), kParametricDrive, "", style))->SetDisabled(true);
    pGraphics->AttachControl(new IVKnobControl(knobs.GetGridCell(0, 4, 1, kNumParams).GetPadded(-10), kParametricLevel, "", style))->SetDisabled(true);

    pGraphics->AttachControl(new IVPeakAvgMeterControl(meterArea, "", style.WithWidgetFrac(0.5)
                                                       .WithShowValue(false)
                                                       .WithColor(kFG, COLOR_GREEN), EDirection::Horizontal, {}, 0, -60.f, 12.f, {}), kCtrlTagMeter)
    ->As<IVPeakAvgMeterControl<>>()->SetPeakSize(2.0f);

    pGraphics->AttachControl(new IVAboutBoxControl(
    new IRolloverCircleSVGButtonControl(mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20), DefaultClickActionFunc, helpSVG),
    new IPanelControl(IRECT(),
//    COLOR_LIGHT_GRAY,
    IPattern::CreateLinearGradient(b, EDirection::Vertical, { {COLOR_WHITE, 0.f}, {COLOR_BLACK, 1.f} }),
    false, // draw frame
    // AttachFunc
    [style](IContainerBase* pParent, const IRECT& r) {
      pParent->AddChildControl(new IVPanelControl(IRECT(), "", style.WithColor(kFR, COLOR_BLACK).WithColor(kFG, COLOR_LIGHT_GRAY.WithOpacity(-0.75))));

      pParent->AddChildControl(new IVLabelControl(IRECT(), "Neural Amp Modeler", style
                                                  .WithDrawFrame(false)
                                                  .WithValueText({30, EAlign::Center, COLOR_BLACK})));
      
      WDL_String versionStr {"Version "};
      versionStr.Append(PLUG_VERSION_STR);
      pParent->AddChildControl(new IVLabelControl(IRECT(), versionStr.Get(), style
                                                  .WithDrawFrame(false)
                                                  .WithValueText({DEFAULT_TEXT_SIZE, EAlign::Center, COLOR_BLACK})));
      pParent->AddChildControl(new IVLabelControl(IRECT(), "By Steven Atkinson", style
                                                  .WithDrawFrame(false)
                                                  .WithValueText({DEFAULT_TEXT_SIZE, EAlign::Center, COLOR_BLACK})));
      pParent->AddChildControl(new IURLControl(IRECT(),
                                                "Train your own model",
                                                "https://github.com/sdatkinson/neural-amp-modeler", {DEFAULT_TEXT_SIZE, COLOR_BLACK}));
    },
    // ResizeFunc
    [](IContainerBase* pParent, const IRECT& r) {
      const IRECT mainArea = r.GetPadded(-20);
      const auto content = mainArea.GetPadded(-10);
      const auto titleLabel = content.GetFromTop(50);
      pParent->GetChild(0)->SetTargetAndDrawRECTs(mainArea);
      pParent->GetChild(1)->SetTargetAndDrawRECTs(titleLabel);
      pParent->GetChild(2)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H()));
      pParent->GetChild(3)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H() + 20));
      pParent->GetChild(4)->SetTargetAndDrawRECTs(titleLabel.GetVShifted(titleLabel.H() + 40));
    })
    ));
  };
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const int nChans = NOutChansConnected();
  const double inputGain = pow(10.0, GetParam(kInputGain)->Value() / 10.0);
  const double outputGain = pow(10.0, GetParam(kOutputGain)->Value() / 10.0);
  
//  mDSPParams["Drive"] = GetParam(kParametricDrive)->Value();
//  mDSPParams["Level"] = GetParam(kParametricLevel)->Value();
//  mDSPParams["Tone"] = GetParam(kParametricTone)->Value();
  
  if (mStagedDSP)
  {
    // Move from staged to active DSP
    mDSP = std::move(mStagedDSP);
    mStagedDSP = nullptr;
  }

  if (mDSP)
  {
    mDSP->process(inputs, outputs, nChans, nFrames, inputGain, outputGain, mDSPParams);
    mDSP->finalize_(nFrames);
  }
  else
  {
    for (auto c = 0; c < nChans; c++)
    {
      for (auto s = 0; s < nFrames; s++)
      {
        outputs[c][s] = inputs[c][s];
      }
    }
  }
  
  mSender.ProcessBlock(outputs, nFrames, kCtrlTagMeter);
}

void NeuralAmpModeler::GetDSP(const WDL_String& modelPath)
{
  WDL_String previousModelPath;
  try {
    previousModelPath = mModelPath;
    auto dspPath = std::filesystem::path(modelPath.Get());
    mStagedDSP = get_dsp(dspPath);
    mModelPath = modelPath;
    std::stringstream ss;
    ss << "Loaded " << dspPath.parent_path().filename();
    SendControlMsgFromDelegate(kCtrlTagModelName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
  }
  catch (std::exception& e) {
    std::stringstream ss;
    ss << "FAILED to load model";
    SendControlMsgFromDelegate(kCtrlTagModelName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
    if (mStagedDSP != nullptr) {
      mStagedDSP = NULL;
    }
    mModelPath = previousModelPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // Model directory (don't serialize the model itself; we'll just load it again when we unserialize)
  chunk.PutStr(mModelPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String dir;
  startPos = chunk.GetStr(mModelPath, startPos);
  mDSP = nullptr;
  return UnserializeParams(chunk, startPos);
}
