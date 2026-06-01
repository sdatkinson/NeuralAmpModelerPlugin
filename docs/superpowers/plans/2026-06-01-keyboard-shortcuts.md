# Keyboard Shortcuts for NAMFileBrowserControl Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add global CTRL-based keyboard shortcuts to the Model and IR file browser rows so all four button actions (load, previous, next, clear/get) can be triggered without using the mouse.

**Architecture:** Extract the four action lambdas inside `NAMFileBrowserControl::OnAttached` into public methods, then register a global `SetKeyHandlerFunc` in `LayoutUI` that dispatches to those methods based on the pressed key and whether SHIFT is held (SHIFT = IR row, no SHIFT = Model row).

**Tech Stack:** C++17, iPlug2 (`IDirBrowseControlBase`, `IKeyPress`, `kVK_*` constants from `IPlugConstants.h`), Windows Visual Studio / MSBuild.

---

## Files Modified

- `NeuralAmpModeler/NeuralAmpModelerControls.h` — Add four public methods to `NAMFileBrowserControl`; simplify the four action lambdas in `OnAttached` to call those methods.
- `NeuralAmpModeler/NeuralAmpModeler.cpp` — Register `SetKeyHandlerFunc` at the end of `LayoutUI`.

---

## Task 1: Add public action methods to `NAMFileBrowserControl`

**Files:**
- Modify: `NeuralAmpModeler/NeuralAmpModelerControls.h:305-413`

There is no automated test suite for this project. Verification is a successful build followed by manual testing in a plugin host.

- [ ] **Step 1: Add four public methods above the existing `private:` section**

In [NeuralAmpModelerControls.h](NeuralAmpModeler/NeuralAmpModelerControls.h), find the `private:` section that begins at line 458. Insert the following four public methods immediately above it (between the closing `}` of `OnAttached` and `private:`):

```cpp
  void LoadFile()
  {
    WDL_String fileName;
    WDL_String path;
    GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
    GetUI()->PromptForDirectory(path, [&](const WDL_String& fileName, const WDL_String& path) {
      if (path.GetLength())
      {
        ClearPathList();
        AddPath(path.Get(), "");
        SetupMenu();
        SelectFirstFile();
        LoadFileAtCurrentIndex();
      }
    });
#else
    GetUI()->PromptForFile(
      fileName, path, EFileAction::Open, mExtension.Get(),
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
#endif
  }

  void PrevFile()
  {
    const auto nItems = NItems();
    if (nItems == 0)
      return;
    mSelectedItemIndex--;
    if (mSelectedItemIndex < 0)
      mSelectedItemIndex = nItems - 1;
    LoadFileAtCurrentIndex();
  }

  void NextFile()
  {
    const auto nItems = NItems();
    if (nItems == 0)
      return;
    mSelectedItemIndex++;
    if (mSelectedItemIndex >= nItems)
      mSelectedItemIndex = 0;
    LoadFileAtCurrentIndex();
  }

  void ClearOrGet()
  {
    if (mBrowserState == NAMBrowserState::Loaded)
    {
      GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
    }
    else
    {
      WDL_String url(mGetButtonURL);
      GetUI()->OpenURL(url.Get());
    }
  }
```

- [ ] **Step 2: Simplify the four action lambdas in `OnAttached`**

In the same file, find `OnAttached` (around line 305). Replace the bodies of the four lambdas as follows. Each lambda's body becomes a single call to the corresponding public method.

Replace:
```cpp
    auto prevFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex--;

      if (mSelectedItemIndex < 0)
        mSelectedItemIndex = nItems - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex++;

      if (mSelectedItemIndex >= nItems)
        mSelectedItemIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [&](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
      pCaller->GetUI()->PromptForDirectory(path, [&](const WDL_String& fileName, const WDL_String& path) {
        if (path.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SelectFirstFile();
          LoadFileAtCurrentIndex();
        }
      });
#else
      pCaller->GetUI()->PromptForFile(
        fileName, path, EFileAction::Open, mExtension.Get(), [&](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength())
          {
            ClearPathList();
            AddPath(path.Get(), "");
            SetupMenu();
            SetSelectedFile(fileName.Get());
            LoadFileAtCurrentIndex();
          }
        });
#endif
    };

    auto clearFileFunc = [&](IControl* pCaller) {
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
      // FIXME disabling output mode...
      //      pCaller->GetUI()->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
    };
```

With:
```cpp
    auto prevFileFunc = [&](IControl* pCaller) { PrevFile(); };

    auto nextFileFunc = [&](IControl* pCaller) { NextFile(); };

    auto loadFileFunc = [&](IControl* pCaller) { LoadFile(); };

    auto clearFileFunc = [&](IControl* pCaller) { ClearOrGet(); };
```

- [ ] **Step 3: Build to verify no compile errors**

From a Developer Command Prompt (or PowerShell with MSBuild on PATH):
```
msbuild NeuralAmpModeler\NeuralAmpModeler.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m
```

Expected: `Build succeeded.` with 0 errors. Warnings are OK.

- [ ] **Step 4: Commit**

```bash
git add NeuralAmpModeler/NeuralAmpModelerControls.h
git commit -m "refactor: extract NAMFileBrowserControl actions into public methods"
```

---

## Task 2: Register global keyboard shortcut handler in `LayoutUI`

**Files:**
- Modify: `NeuralAmpModeler/NeuralAmpModeler.cpp:307-314`

- [ ] **Step 1: Add the key handler at the end of `LayoutUI`**

In [NeuralAmpModeler.cpp](NeuralAmpModeler/NeuralAmpModeler.cpp), find the `ForAllControlsFunc` block near the end of `LayoutUI` (around line 307). Insert the key handler immediately after it, before the closing `};` of the `LayoutUI` lambda:

```cpp
    auto* modelBrowser =
      static_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagModelFileBrowser));
    auto* irBrowser =
      static_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser));

    pGraphics->SetKeyHandlerFunc([modelBrowser, irBrowser](const IKeyPress& key, bool isUp) -> bool {
      if (isUp || !key.C)
        return false;
      NAMFileBrowserControl* target = key.S ? irBrowser : modelBrowser;
      switch (key.VK)
      {
        case kVK_O: target->LoadFile(); return true;
        case kVK_LEFT: target->PrevFile(); return true;
        case kVK_RIGHT: target->NextFile(); return true;
        case kVK_BACK: target->ClearOrGet(); return true;
        default: return false;
      }
    });
```

After this insertion the end of `LayoutUI` should look like:

```cpp
    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    auto* modelBrowser =
      static_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagModelFileBrowser));
    auto* irBrowser =
      static_cast<NAMFileBrowserControl*>(pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser));

    pGraphics->SetKeyHandlerFunc([modelBrowser, irBrowser](const IKeyPress& key, bool isUp) -> bool {
      if (isUp || !key.C)
        return false;
      NAMFileBrowserControl* target = key.S ? irBrowser : modelBrowser;
      switch (key.VK)
      {
        case kVK_O: target->LoadFile(); return true;
        case kVK_LEFT: target->PrevFile(); return true;
        case kVK_RIGHT: target->NextFile(); return true;
        case kVK_BACK: target->ClearOrGet(); return true;
        default: return false;
      }
    });
  };
}
```

- [ ] **Step 2: Build to verify no compile errors**

```
msbuild NeuralAmpModeler\NeuralAmpModeler.sln /p:Configuration=Debug /p:Platform=x64 /t:Build /m
```

Expected: `Build succeeded.` with 0 errors.

- [ ] **Step 3: Manual smoke test in a host**

Load the built VST3/plugin in a DAW or the iPlug2 standalone app. With the plugin window focused:

| Action | Keys | Expected result |
|--------|------|-----------------|
| Load model file dialog | CTRL+O | File open dialog appears for `.nam` files |
| Next model | CTRL+→ | Plugin loads the next `.nam` file in the directory |
| Previous model | CTRL+← | Plugin loads the previous `.nam` file |
| Clear model (when loaded) | CTRL+⌫ | Model is cleared; label resets to "Select model..." |
| Get NAM models URL (when empty) | CTRL+⌫ | System browser opens to NAM models page |
| Load IR file dialog | CTRL+SHIFT+O | File open dialog appears for `.wav` files |
| Next IR | CTRL+SHIFT+→ | Plugin loads the next `.wav` file |
| Previous IR | CTRL+SHIFT+← | Plugin loads the previous `.wav` file |
| Clear IR (when loaded) | CTRL+SHIFT+⌫ | IR is cleared; label resets to "Select IR..." |
| Unrelated keys (e.g. CTRL+Z) | CTRL+Z | Nothing happens in the plugin; host may handle it |
| Settings panel Escape | ESC (while panel open) | Panel closes; model/IR shortcuts still work after |

- [ ] **Step 4: Commit**

```bash
git add NeuralAmpModeler/NeuralAmpModeler.cpp
git commit -m "feat: add global CTRL keyboard shortcuts to file browser controls"
```
