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

class NAMFileNameControl : public IVButtonControl
{
public:
  NAMFileNameControl(const IRECT& bounds, const char* label, const IVStyle& style)
  : IVButtonControl(bounds, DefaultClickActionFunc, label, style)
  {
  }
  
  void SetLabelAndTooltip(const char* str)
  {
    SetLabelStr(str);
    SetTooltip(str);
  }
  
  void SetLabelAndTooltipEllipsizing(const WDL_String& fileName)
  {
    auto EllipsizeFilePath = [](const char* filePath, size_t prefixLength, size_t suffixLength, size_t maxLength) {
      const std::string ellipses = "...";
      assert (maxLength <= (prefixLength + suffixLength + ellipses.size()));
      std::string str {filePath};
      
      if (str.length() <= maxLength)
      {
        return str;
      }
      else
      {
        return str.substr(0, prefixLength) + ellipses + str.substr(str.length() - suffixLength);
      }
    };
    
    auto ellipsizedFileName = EllipsizeFilePath(fileName.get_filepart(), 22, 22, 45);
    SetLabelStr(ellipsizedFileName.c_str());
    SetTooltip(fileName.get_filepart());
  }
};

class NAMFileBrowserControl : public IDirBrowseControlBase
{
public:
  NAMFileBrowserControl(const IRECT& bounds, int clearMsgTag, const char* labelStr, const char* fileExtension,
                        IFileDialogCompletionHandlerFunc ch, const IVStyle& style,
                        const ISVG& loadSVG, const ISVG& clearSVG, const ISVG& leftSVG, const ISVG& rightSVG)
  : IDirBrowseControlBase(bounds, fileExtension, true)
  , mClearMsgTag(clearMsgTag)
  , mDefaultLabelStr(labelStr)
  , mCompletionHandlerFunc(ch)
  , mStyle(style.WithColor(kFG, COLOR_TRANSPARENT).WithDrawFrame(false))
  , mLoadSVG(loadSVG)
  , mClearSVG(clearSVG)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  {
    mIgnoreMouse = true;
    mShowFileExtensions = false;
  }
  
  void Draw(IGraphics& g) override { /* NO-OP */ }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (pSelectedMenu)
    {
      IPopupMenu::Item* pItem = pSelectedMenu->GetChosenItem();

      if (pItem)
      {
        mSelectedIndex = mItems.Find(pItem);
        LoadFileAtCurrentIndex();
      }
    }
  }

  void OnAttached() override
  {
    auto prevFileFunc = [&](IControl* pCaller) {
      mSelectedIndex--;

      if (mSelectedIndex < 0)
        mSelectedIndex = NItems() - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [&](IControl* pCaller) {
      mSelectedIndex++;

      if (mSelectedIndex >= NItems())
        mSelectedIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [&](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      pCaller->GetUI()->PromptForFile(fileName, path, EFileAction::Open, mExtension.Get(),
                                      [&](const WDL_String& fileName, const WDL_String& path) {
      
        if (fileName.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SetSelectedFile(fileName.Get());
          LoadFileAtCurrentIndex();
        }
      });
    };
    
    auto clearFileFunc = [&](IControl* pCaller) {
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      pCaller->GetUI()->GetControlWithTag(kCtrlTagOutNorm)->SetDisabled(false);
    };

    auto chooseFileFunc = [&, loadFileFunc](IControl* pCaller) {
      if (std::string_view(pCaller->As<IVButtonControl>()->GetLabelStr()) == mDefaultLabelStr.Get())
      {
        loadFileFunc(pCaller);
      }
      else
      {
        CheckSelectedItem();
        pCaller->GetUI()->CreatePopupMenu(*this, mMainMenu, pCaller->GetRECT());
      }
    };

    IRECT padded = mRECT.GetPadded(-5.f);
    const auto buttonWidth = padded.H();
    const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto clearButtonBounds = padded.ReduceFromRight(buttonWidth);
    const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto fileNameButtonBounds = padded;
    
    AddChildControl(new IRolloverSVGButtonControl(loadFileButtonBounds, DefaultClickActionFunc, mLoadSVG))
    ->SetAnimationEndActionFunction(loadFileFunc);
    AddChildControl(new IRolloverSVGButtonControl(leftButtonBounds, DefaultClickActionFunc, mLeftSVG))
    ->SetAnimationEndActionFunction(prevFileFunc);
    AddChildControl(new IRolloverSVGButtonControl(rightButtonBounds, DefaultClickActionFunc, mRightSVG))
    ->SetAnimationEndActionFunction(nextFileFunc);
    AddChildControl(mFileNameControl = new NAMFileNameControl(fileNameButtonBounds, mDefaultLabelStr.Get(), mStyle))
    ->SetAnimationEndActionFunction(chooseFileFunc);
    AddChildControl(new IRolloverSVGButtonControl(clearButtonBounds, DefaultClickActionFunc, mClearSVG))
    ->SetAnimationEndActionFunction(clearFileFunc);
    
    mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
  }

  void LoadFileAtCurrentIndex()
  {
    if (mSelectedIndex > -1 && mSelectedIndex < mItems.GetSize())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
      mCompletionHandlerFunc(fileName, path);
    }
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    switch (msgTag)
    {
      case kMsgTagLoadFailed:
        ClearPathList();
        SetupMenu();
        mFileNameControl->SetLabelAndTooltip("Load Failed");
        break;
      case kMsgTagLoadedModel:
      case kMsgTagLoadedIR:
      {
        WDL_String fileName;
        fileName.Set(reinterpret_cast<const char*>(pData));
        ClearPathList();
        fileName.remove_filepart(true);
        AddPath(fileName.Get(), "");
        SetupMenu();

        //reset
        fileName.Set(reinterpret_cast<const char*>(pData));
        SetSelectedFile(fileName.Get());
        mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
        break;
      }
      default:
        break;
    }
  }
private:
  WDL_String mDefaultLabelStr;
  IFileDialogCompletionHandlerFunc mCompletionHandlerFunc;
  NAMFileNameControl* mFileNameControl = nullptr;
  IVStyle mStyle;
  ISVG mLoadSVG, mClearSVG, mLeftSVG, mRightSVG;
  int mClearMsgTag;
};
