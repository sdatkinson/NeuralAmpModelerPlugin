# Design: Keyboard Shortcuts for NAMFileBrowserControl

**Date:** 2026-06-01  
**Status:** Approved

## Overview

Add global CTRL-based keyboard shortcuts to the two `NAMFileBrowserControl` rows (Model and IR) so that all four button actions — load, previous, next, and clear/get — can be triggered from the keyboard without clicking.

## Key Mapping

| Action | Model row | IR row |
|--------|-----------|--------|
| Load (open file dialog) | CTRL+O | CTRL+SHIFT+O |
| Previous file | CTRL+← | CTRL+SHIFT+← |
| Next file | CTRL+→ | CTRL+SHIFT+→ |
| Clear (loaded) / Get URL (empty) | CTRL+⌫ | CTRL+SHIFT+⌫ |

- CTRL = `key.C` in iPlug2's `IKeyPress`
- SHIFT = `key.S`; its presence selects the IR row vs the Model row
- ⌫ = Backspace (`kVK_BACK`, `0x08`)
- Shortcuts fire on key-down only (`isUp == false`)

## Architecture

The feature touches exactly two files:

### 1. `NeuralAmpModeler/NeuralAmpModelerControls.h` — `NAMFileBrowserControl`

Extract the logic currently inside the four `OnAttached` lambdas into four public methods:

- `LoadFile()` — triggers the file dialog (wraps `loadFileFunc` logic)
- `PrevFile()` — navigates to the previous file (wraps `prevFileFunc` logic)
- `NextFile()` — navigates to the next file (wraps `nextFileFunc` logic)
- `ClearOrGet()` — clears the loaded file if one is loaded, or opens the Get URL if in the empty state (wraps `clearFileFunc` / Get URL open logic)

The lambdas in `OnAttached` become one-line calls to these methods. No logic is duplicated.

`ClearOrGet()` checks `mBrowserState` to determine which action to take. Opening the URL requires `GetUI()->OpenURL(...)`, which is available inside the control.

### 2. `NeuralAmpModeler/NeuralAmpModeler.cpp` — `LayoutUI`

After both browser controls are attached and tagged, register a global key handler:

```cpp
auto* modelBrowser = static_cast<NAMFileBrowserControl*>(
    pGraphics->GetControlWithTag(kCtrlTagModelFileBrowser));
auto* irBrowser = static_cast<NAMFileBrowserControl*>(
    pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser));

pGraphics->SetKeyHandlerFunc([modelBrowser, irBrowser](const IKeyPress& key, bool isUp) -> bool {
    if (isUp || !key.C) return false;
    NAMFileBrowserControl* target = key.S ? irBrowser : modelBrowser;
    switch (key.VK) {
        case kVK_O:     target->LoadFile();    return true;
        case kVK_LEFT:  target->PrevFile();    return true;
        case kVK_RIGHT: target->NextFile();    return true;
        case kVK_BACK:  target->ClearOrGet(); return true;
        default:        return false;
    }
});
```

The handler returns `false` for any key it does not claim, leaving all other global key events unaffected.

## Edge Cases

- **No directory / no files** — `PrevFile` and `NextFile` already guard `NItems() == 0` and are no-ops; no change needed.
- **Settings panel open** — the settings panel overrides `OnKeyDown` for `Escape`. iPlug2's global `SetKeyHandlerFunc` only fires when no control under the cursor claimed the event, so there is no conflict.
- **`ClearOrGet` in Get state** — opens the NAM/IR URL in the system browser, same as clicking the globe button. Uses `GetUI()->OpenURL(...)`.

## Out of Scope

- No visual shortcut hints or on-screen overlay
- No new plugin parameters or preset state
- No changes to the visual appearance of the controls
