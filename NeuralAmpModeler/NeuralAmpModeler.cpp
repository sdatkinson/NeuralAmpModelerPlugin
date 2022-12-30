#include <filesystem>
#include <iostream>
#include <utility>
#include <cmath>

#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"
#include "Colors.h"

using namespace iplug;
using namespace igraphics;

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
      g.FillRect(PluginColors::MOUSEOVER, mRECT);

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
      g.FillEllipse(PluginColors::MOUSEOVER, mRECT);

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
: Plugin(info, MakeConfig(kNumParams, kNumPresets)),
  mInputPointers(nullptr),
  mOutputPointers(nullptr),
  mDSP(nullptr),
  mStagedDSP(nullptr),
  mToneBass(),
  mToneMid(),
  mToneTreble()
{
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -20.0, 20.0, 0.1);

//  try {
//     this->mDSP = get_hard_dsp();
//  }
//  catch (std::exception& e) {
//    std::cerr << "Failed to read hard coded DSP" << std::endl;
//    std::cerr << e.what() << std::endl;
//  }
  
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
    
    const float knobHalfPad = 10.0f;
    const float knobPad = 2.0f * knobHalfPad;
    const float knobHalfHeight = 70.0f;
    const auto knobs = content.GetReducedFromLeft(knobPad).GetReducedFromRight(knobPad).GetMidVPadded(knobHalfHeight);
    IRECT inputKnobArea = knobs.GetGridCell(0, kInputLevel, 1, kNumParams).GetPadded(-10);
    IRECT bassKnobArea = knobs.GetGridCell(0, kToneBass, 1, kNumParams).GetPadded(-10);
    IRECT middleKnobArea = knobs.GetGridCell(0, kToneMid, 1, kNumParams).GetPadded(-10);
    IRECT trebleKnobArea = knobs.GetGridCell(0, kToneTreble, 1, kNumParams).GetPadded(-10);
    IRECT outputKnobArea =knobs.GetGridCell(0, kOutputLevel, 1, kNumParams).GetPadded(-10);
    
    const auto modelArea = content.GetFromBottom(30).GetMidHPadded(150);
    const float meterHalfHeight = 0.5f * 250.0f;
    const auto inputMeterArea = inputKnobArea.GetFromLeft(knobHalfPad).GetMidHPadded(knobHalfPad).GetMidVPadded(meterHalfHeight).GetTranslated(-knobPad, 0.0f);
    const auto outputMeterArea = outputKnobArea.GetFromRight(knobHalfPad).GetMidHPadded(knobHalfPad).GetMidVPadded(meterHalfHeight).GetTranslated(knobPad, 0.0f);

    const IVStyle style {
      true, // Show label
      true, // Show value
      {
        DEFAULT_BGCOLOR,  //PluginColors::NAM_1,  //DEFAULT_BGCOLOR, // Background
        PluginColors::NAM_1,  // .WithOpacity(0.5), // Foreground
        PluginColors::NAM_2.WithOpacity(0.4),  // .WithOpacity(0.4), // Pressed
        PluginColors::NAM_3, // Frame
        PluginColors::MOUSEOVER, // Highlight
        DEFAULT_SHCOLOR, // Shadow
        PluginColors::NAM_2 , // Extra 1
        COLOR_RED, // Extra 2
        DEFAULT_X3COLOR  // Extra 3
      }, // Colors
      {DEFAULT_TEXT_SIZE + 5.f, EVAlign::Middle, PluginColors::NAM_3},  // Knob label text
      {DEFAULT_TEXT_SIZE + 5.f, EVAlign::Bottom, PluginColors::NAM_3},  // Knob value text
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
    
    //auto tolexPNG = pGraphics->LoadBitmap(TOLEX_FN);
    //pGraphics->AttachControl(new IBitmapControl(pGraphics->GetBounds(), tolexPNG, kNoParameter))->SetBlend(IBlend(EBlend::Default, 0.5));
    // The background inside the outermost border
    pGraphics->AttachControl(new IVPanelControl(mainArea, "", style.WithColor(kFG, PluginColors::NAM_1)));  // .WithContrast(-0.75)
    pGraphics->AttachControl(new IVLabelControl(titleLabel, "Neural Amp Modeler", style
                                                .WithDrawFrame(false)
                                                .WithValueText({30, EAlign::Center, PluginColors::NAM_3})));

    //    pGraphics->AttachControl(new IVBakedPresetManagerControl(modelArea, style.WithValueText({DEFAULT_TEXT_SIZE, EVAlign::Middle, COLOR_WHITE})));

    // Model loader button
    auto loadModel = [&, pGraphics](IControl* pCaller) {
      WDL_String dir;
      pGraphics->PromptForDirectory(dir, [&](const WDL_String& fileName, const WDL_String& path){
        if (path.GetLength())
          _GetDSP(path);
      });
    };
    
    // Tells us what model is loaded
    pGraphics->AttachControl(new IVPanelControl(modelArea, "", style.WithColor(kFG, PluginColors::NAM_1)));  // .WithContrast(-0.75)
    pGraphics->AttachControl(new IRolloverSVGButtonControl(modelArea.GetFromLeft(30).GetPadded(-2.f), loadModel, folderSVG));
    pGraphics->AttachControl(new IVUpdateableLabelControl(modelArea.GetReducedFromLeft(30), "Select model...", style.WithDrawFrame(false).WithValueText(style.valueText.WithVAlign(EVAlign::Middle))), kCtrlTagModelName);
    
    // The knobs
    pGraphics->AttachControl(new IVKnobControl(inputKnobArea, kInputLevel, "", style));
    pGraphics->AttachControl(new IVKnobControl(bassKnobArea, kToneBass, "", style));
    pGraphics->AttachControl(new IVKnobControl(middleKnobArea, kToneMid, "", style));
    pGraphics->AttachControl(new IVKnobControl(trebleKnobArea, kToneTreble, "", style));
    pGraphics->AttachControl(new IVKnobControl(outputKnobArea, kOutputLevel, "", style));

    // The meters
    const float meterMin = -60.0f;
    const float meterMax = 12.0f;
    pGraphics->AttachControl(new IVPeakAvgMeterControl(inputMeterArea, "", style.WithWidgetFrac(0.5)
                                                       .WithShowValue(false)
                                                       .WithColor(kFG, PluginColors::NAM_2), EDirection::Vertical, {}, 0, meterMin, meterMax, {}), kCtrlTagInputMeter)
    ->As<IVPeakAvgMeterControl<>>()->SetPeakSize(2.0f);
    pGraphics->AttachControl(new IVPeakAvgMeterControl(outputMeterArea, "", style.WithWidgetFrac(0.5)
                                                       .WithShowValue(false)
                                                       .WithColor(kFG, PluginColors::NAM_2), EDirection::Vertical, {}, 0, meterMin, meterMax, {}), kCtrlTagOutputMeter)
    ->As<IVPeakAvgMeterControl<>>()->SetPeakSize(2.0f);

    // Help/about box
    pGraphics->AttachControl(new IVAboutBoxControl(
    new IRolloverCircleSVGButtonControl(mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20), DefaultClickActionFunc, helpSVG),
    new IPanelControl(IRECT(),
//    COLOR_LIGHT_GRAY,
    IPattern::CreateLinearGradient(b, EDirection::Vertical, { {PluginColors::NAM_3, 0.f}, {PluginColors::NAM_1, 1.f} }),
    false, // draw frame
    // AttachFunc
    [style](IContainerBase* pParent, const IRECT& r) {
      pParent->AddChildControl(new IVPanelControl(IRECT(), "", style.WithColor(kFR, PluginColors::NAM_3.WithOpacity(0.1)).WithColor(kFG, PluginColors::NAM_1.WithOpacity(0.1))));

      pParent->AddChildControl(new IVLabelControl(IRECT(), "Neural Amp Modeler", style
                                                  .WithDrawFrame(false)
                                                  .WithValueText({30, EAlign::Center, PluginColors::HELP_TEXT})));
      
      WDL_String versionStr {"Version "};
      versionStr.Append(PLUG_VERSION_STR);
      pParent->AddChildControl(new IVLabelControl(IRECT(), versionStr.Get(), style
                                                  .WithDrawFrame(false)
                                                  .WithValueText({DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT })));
      pParent->AddChildControl(new IVLabelControl(IRECT(), "By Steven Atkinson", style
                                                  .WithDrawFrame(false)
                                                  .WithValueText({DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT})));
      pParent->AddChildControl(new IURLControl(IRECT(),
                                                "Train your own model",
                                                "https://github.com/sdatkinson/neural-amp-modeler", {DEFAULT_TEXT_SIZE, PluginColors::HELP_TEXT }));
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
  const int nChans = this->NOutChansConnected();
  this->_PrepareBuffers(nFrames);
  this->_ProcessInput(inputs, nFrames);
  if (mStagedDSP != nullptr)
  {
    // Move from staged to active DSP
    mDSP = std::move(mStagedDSP);
    mStagedDSP = nullptr;
  }

  if (mDSP != nullptr)
  {
    // TODO remove input / output gains from here.
    const double inputGain = 1.0;
    const double outputGain = 1.0;
    mDSP->process(this->mInputPointers, this->mOutputPointers, nChans, nFrames, inputGain, outputGain, mDSPParams);
    mDSP->finalize_(nFrames);
  }
  else {
    this->_FallbackDSP(nFrames);
  }
  // Tone stack
  const double sampleRate = this->GetSampleRate();
  const double bassFrequency = 150.0;
  const double midFrequency = 450.0;
  const double trebleFrequency = 1800.0;
  const double bassQuality = 0.707;
  const double midQuality = 1.5;
  const double trebleQuality = 0.707;
  
  // Translate params from knob 0-10 to +/-20dB.
  const double bassGainDB = 4.0 * (this->GetParam(kToneBass)->Value() - 5.0);
  const double midGainDB = 4.0 * (this->GetParam(kToneMid)->Value() - 5.0);
  const double trebleGainDB = 4.0 * (this->GetParam(kToneTreble)->Value() - 5.0);
  // Define filter parameters
  recursive_linear_filter::LowShelfParams bassParams(sampleRate, bassFrequency, bassQuality, bassGainDB);
  recursive_linear_filter::PeakingParams midParams(sampleRate, midFrequency, midQuality, midGainDB);
  recursive_linear_filter::HighShelfParams trebleParams(sampleRate, trebleFrequency, trebleQuality, trebleGainDB);
  // Apply tone stack
  sample** bassPointers = this->mToneBass.Process(this->mOutputPointers, nChans, nFrames, &bassParams);
  sample** midPointers = this->mToneMid.Process(bassPointers, nChans, nFrames, &midParams);
  sample** treblePointers = this->mToneTreble.Process(midPointers, nChans, nFrames, &trebleParams);
  
  // Let's get outta here
  this->_ProcessOutput(treblePointers, outputs, nFrames);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  this->_UpdateMeters(this->mInputPointers, outputs, nFrames);
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
  int retcode = UnserializeParams(chunk, startPos);
  if (this->mModelPath.GetLength())
    this->_GetDSP(this->mModelPath);
  return retcode;
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();
  if (this->mModelPath.GetLength())
    this->_SetModelMsg(this->mModelPath);
}

// Private methods ============================================================

void NeuralAmpModeler::_FallbackDSP(const int nFrames)
{
  const int nChans = this->NOutChansConnected();
  for (int c=0; c<nChans; c++)
    for (int s=0; s<nFrames; s++)
      this->mOutputArray[c][s] = this->mInputArray[c][s];
}

void NeuralAmpModeler::_GetDSP(const WDL_String& modelPath)
{
  WDL_String previousModelPath;
  try {
    previousModelPath = mModelPath;
    auto dspPath = std::filesystem::path(modelPath.Get());
    mStagedDSP = get_dsp(dspPath);
    this->_SetModelMsg(modelPath);
  }
  catch (std::exception& e) {
    std::stringstream ss;
    ss << "FAILED to load model";
    SendControlMsgFromDelegate(kCtrlTagModelName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
    if (mStagedDSP != nullptr) {
      mStagedDSP = nullptr;
    }
    mModelPath = previousModelPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
  }
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  return this->mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (this->_GetBufferNumChannels() == 0)
    return 0;
  return this->mInputArray[0].size();
}

void NeuralAmpModeler::_PrepareBuffers(const int nFrames)
{
  const size_t nChans = this->NOutChansConnected();
  const bool updateChannels = nChans != this->_GetBufferNumChannels();
  const bool updateFrames = updateChannels || (this->_GetBufferNumFrames() != nFrames);
//  if (!updateChannels && !updateFrames)
//    return;
  
  if (updateChannels) {  // Does channels *and* frames just to be safe.
    this->_PrepareIOPointers(nChans);
    this->mInputArray.resize(nChans);
    this->mOutputArray.resize(nChans);
  }
  if (updateFrames) { // and not update channels
    for (int c=0; c<nChans; c++) {
      this->mInputArray[c].resize(nFrames);
      this->mOutputArray[c].resize(nFrames);
    }
  }
  // Would these ever change?
  for (auto c=0; c<this->mInputArray.size(); c++) {
    this->mInputPointers[c] = this->mInputArray[c].data();
    this->mOutputPointers[c] = this->mOutputArray[c].data();
  }
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t nChans)
{
  if (this->mInputPointers != nullptr)
    delete[] this->mInputPointers;
  if (this->mOutputPointers != nullptr)
    delete[] this->mOutputPointers;
  this->mInputPointers = new sample*[nChans];
  if (this->mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  this->mOutputPointers = new sample*[nChans];
  if (this->mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ProcessInput(iplug::sample **inputs, const int nFrames)
{
  const double gain = pow(10.0, GetParam(kInputLevel)->Value() / 10.0);
  const size_t nChans = this->NOutChansConnected();
  // Assume _PrepareBuffers() was already called
  for (int c=0; c<nChans; c++)
    for (int s=0; s<nFrames; s++)
      this->mInputArray[c][s] = gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample **outputs, const int nFrames)
{
  const double gain = pow(10.0, GetParam(kOutputLevel)->Value() / 10.0);
  const size_t nChans = this->NOutChansConnected();
  // Assume _PrepareBuffers() was already called
  for (int c=0; c<nChans; c++)
    for (int s=0; s<nFrames; s++)
      outputs[c][s] = gain * inputs[c][s];
}

void NeuralAmpModeler::_SetModelMsg(const WDL_String& modelPath)
{
    auto dspPath = std::filesystem::path(modelPath.Get());
    mModelPath = modelPath;
    std::stringstream ss;
    ss << "Loaded " << dspPath.parent_path().filename();
    SendControlMsgFromDelegate(kCtrlTagModelName, 0, int(strlen(ss.str().c_str())), ss.str().c_str());
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const int nFrames)
{
  this->mInputSender.ProcessBlock(inputPointer, nFrames, kCtrlTagInputMeter);
  this->mOutputSender.ProcessBlock(outputPointer, nFrames, kCtrlTagOutputMeter);
}
