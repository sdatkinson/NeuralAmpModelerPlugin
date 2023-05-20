#pragma once

#include "IControls.h"

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
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 2.f);

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

class NamKnobControl : public IVKnobControl, public IBitmapBase
{
public:
  NamKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVKnobControl(bounds, paramIdx, label, style, true)
  , IBitmapBase(bitmap)
  {
    mInnerPointerFrac = 0.6;
  }

  void DrawWidget(IGraphics& g) override
  {
    float widgetRadius = GetRadius();
    const float cx = mWidgetBounds.MW(), cy = mWidgetBounds.MH();
    IRECT knobHandleBounds = mWidgetBounds.GetCentredInside((widgetRadius - mTrackToHandleDistance) * 2.5f);
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    DrawIndicatorTrack(g, angle, cx, cy, widgetRadius);
    g.DrawBitmap(mBitmap, knobHandleBounds.GetTranslated(4, 3), 0, 0);
    float data[2][2];
    RadialPoints(angle, cx + 1, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3);
    g.PathFill(IPattern::CreateRadialGradient(data[1][0], data[1][1], 4.0f, {{GetColor(mMouseIsOver ? kX3 : kX1), 0.f}, {GetColor(mMouseIsOver ? kX3 : kX1), 0.8f}, {COLOR_TRANSPARENT, 1.0f}}), {}, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.5f), data[1][0], data[1][1], 3, &mBlend);
  }
};

class NamSwitchControl : public IVSlideSwitchControl, public IBitmapBase
{
public:
  NamSwitchControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap,
                   IBitmap handleBitmap)
  : IVSlideSwitchControl(
    {bounds.L, bounds.T, bitmap}, paramIdx, label, style.WithRoundness(5.f).WithShowLabel(false).WithShowValue(false))
  , IBitmapBase(bitmap)
  , mHandleBitmap(handleBitmap)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    // OL: arg, pixels :-(
    if (GetValue() > 0.5f)
      g.FillRoundRect(GetColor(kFG), mRECT.GetPadded(-2.7f).GetTranslated(0.0, 1.f), 9.f, &mBlend);
    else
      g.FillRoundRect(COLOR_BLACK, mRECT.GetPadded(-2.7f).GetTranslated(0.0, 1.f), 9.f, &mBlend);

    DrawTrack(g, mWidgetBounds);
    DrawHandle(g, mHandleBounds);
  }

  void DrawTrack(IGraphics& g, const IRECT& filledArea) override { g.DrawBitmap(mBitmap, mRECT, 0, &mBlend); }

  void DrawHandle(IGraphics& g, const IRECT& filledArea) override
  {
    g.DrawBitmap(mHandleBitmap, filledArea.GetTranslated(2.0, 3.0), 0, &mBlend);
  }

private:
  IBitmap mHandleBitmap;
};
