# Neural Amp Modeler Plug-in

[![Build](https://github.com/sdatkinson/NeuralAmpModelerPlugin/actions/workflows/build-native.yml/badge.svg)](https://github.com/sdatkinson/NeuralAmpModelerPlugin/actions/workflows/build-native.yml)

A VST3/AudioUnit plug-in\* for [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler), built with [iPlug2](https://iplug2.github.io).

- https://www.youtube.com/user/RunawayThumbtack
- https://github.com/sdatkinson/neural-amp-modeler

## Building and Installation

To build the app or plugin, there are build scripts in [NeuralAmpModeler/scripts/](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/NeuralAmpModeler/scripts).
The [workflows](https://github.com/sdatkinson/NeuralAmpModelerPlugin/tree/main/.github/workflows) can show you how to do this.

### Pre-built installers

If you want a pre-built installer from this repo without having to , I've made "Gateway", a fork of this repo, availble at https://neuralampmodeler.com/users!

## Supported Platforms

The Neural Amp Modeler plugin currently supports Windows 10 (64bit) or later, and macOS 10.15 (Catalina) or later.

For Linux support, there is an LV2 plugin available: https://github.com/mikeoliphant/neural-amp-modeler-lv2.

## About

This is a cleaned up version of [the original iPlug2-based NAM plugin](https://github.com/sdatkinson/iPlug2) with some refactoring to adopt better practices recommended by the developers of iPlug2.
(Thanks [Oli](https://github.com/olilarkin) for your generous suggestions!)

\*could also support AAX, CLAP, Linux, iOS soon.

## Model blending

Up to three models can be loaded at once and summed, which is useful when you have several captures of the same amp
through different microphones and want to balance them without running the plugin three times.

Click the blend icon to the right of the model row to open the blend page. It holds one row per slot: a model browser,
a level, and three toggles — **Ø** (polarity), **M** (mute) and **S** (solo). Slot 1 is the same model as the one on
the main page.

- **Levels sum like mixer channels.** Two slots at 0 dB are about 6 dB louder than one; trim the total with the Output
  knob. Pulling a level to its minimum mutes that slot.
- **Polarity invert** flips a slot's sign. Reach for it when two captures of the same cab thin each other out — mics at
  different distances rarely sum cleanly.
- **Mute and solo** behave the way they do on a console: with any solo engaged, only soloed slots are heard, and an
  explicit mute still wins over solo. A solo left on an empty slot is ignored rather than silencing everything.
- Level, polarity, mute and solo changes are ramped over a block, so toggling them mid-note doesn't click.

**You only have to find the folder once.** Loading a model hands its folder to any slot that's still empty, so those
rows' arrows work straight away — they stay blank and silent until you press one. Each row parks on the most recently
loaded capture, so for a folder of mic captures the whole blend is one file dialog and one arrow press per row:

    pick "Mesa Crunch 421.nam" on slot 1   ->  1: 421   2: (empty)   3: (empty)
    press > on row 2                       ->  1: 421   2: 545       3: (empty)
    press > on row 3                       ->  1: 421   2: 545       3: M160

A slot that already has a model is never touched, and the folder button still overrides any row.
- Slots are automatically time-aligned with each other when their models run at different sample rates.
- Levelling (input calibration, Normalized and Calibrated output modes) is referenced to the first loaded slot, and the
  others are matched to it, so the blend balances the models rather than their metadata. Normalized and Calibrated are
  only offered when *every* loaded model supports them.
- The noise gate, tone stack and IR are shared, and run after the blend.
- Each loaded model costs its own CPU: three models is roughly three times the load of one.

## Rough edges

### Standalone I/O
The I/O for the standalone doesn't inherit the stability of most plugin hosts (DAWs), so it's a bit sparser on features. For complex routing, the plugin (VST3/AU) inside a plugin host is still the most reliable option.

### Graphics backend
If you're having trouble with NAM crashing before the GUI comes up, then you might have an unsupported graphics configuration. Usually, this is when you have a dedicated graphics card (like an nVIDIA GPU) and you're using the integrated (CPU) graphics on a Windows system. To fix this, Go to the control panel, pick NAM (or your DAW), and make sure that it uses your graphics card. (If you know more and can help fix this, please make an Issue and let me know more!)
