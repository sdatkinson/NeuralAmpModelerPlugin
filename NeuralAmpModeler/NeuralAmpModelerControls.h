#pragma once

#include "IControls.h"
#include "NeuralAmpModelerStyles.h"

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

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) { SetStr(reinterpret_cast<const char*>(pData)); }
};
