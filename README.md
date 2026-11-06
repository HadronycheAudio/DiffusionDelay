# DiffusionDelay

<img src="docs/images/VST_Compatible_Logo_Steinberg_with_TM.png" alt="VST Compatible" width="110">

A VST® 3 effect by Hadronyche: an echo with a reverb inside its feedback loop. Diffusion adds early reflections before each repeat and a tail after it, scattered across the stereo image. With the feedback at 100%, the repeats never end, and each pass dissolves them a little further until there's only a mass of diffused sound. It's built directly on the Steinberg VST 3 SDK and VSTGUI.

`docs/signal-path.md` describes the design and how each part behaves.

## Download

**On Windows,** get the installer from the [latest release](https://github.com/HadronycheAudio/DiffusionDelay/releases/latest). It needs 64-bit Windows 10 or later, and installs to `C:\Program Files\Common Files\VST3`. The installer isn't code-signed, so Windows will probably warn you: choose **More info**, then **Run anyway**.

**On a Mac,** get `DiffusionDelay-<version>-macOS.pkg` from the same page. It's a VST 3 and an Audio Unit, for Intel and Apple Silicon, on macOS 10.15 or later. It installs to `/Library/Audio/Plug-Ins`. The installer isn't signed by Apple, so macOS will refuse it the first time: the release notes explain how to open it. The Mac version hasn't been used in a real host yet.

## Using it

**Echo**

- Time is the time to the repeat, from 10 ms to 10 s. Moving it glides there like a tape head, and bends the pitch of what's in the delay.
- Sync follows the host's tempo instead of Time, which dims. Division picks the note value, from 1/32 to 4/1, with dotted (D) and triplet (T) values. It can't go past Time's range, so 4/1 stops at 10 s below 96 BPM.
- Feedback is how much of each repeat goes round again. At 100%, the repeats go on until you turn it down.
- Diffusion is how much of the repeat scatters on each pass. At 0, it's a clean echo. At 100%, a clear repeat is still there, with reflections either side of it, and it's gone within a few passes. The reflections' timing scales with Time.
- Damper cuts the highs above about 4 kHz on each pass, by up to 18 dB. The first repeat keeps all its top end, and the lows go on for ever at 100% feedback.

**Output**

- Dry/wet at 50% plays both at full level.
- Volume is last.

A soft limiter in the feedback holds back what builds up when you keep playing into 100% feedback. That can still reach about +9 dB, and there's no limiter on the output.

The plug-in has no latency.

## Requirements for building

- Visual Studio 2022 with the "Desktop development with C++" workload
- CMake 3.25 or newer
- Git

## Getting the code

The SDK and HadronycheCore, the code shared by the Hadronyche plug-ins, are pinned git submodules in `external/`:

```powershell
git -c core.longpaths=true clone --recurse-submodules <repo-url>
# or, in an existing clone:
git -c core.longpaths=true submodule update --init --recursive
```

Long paths are needed if you clone into a deep folder, because some of the SDK's sample files would otherwise exceed the Windows path limit.

## Building

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target DiffusionDelay
```

This produces `build/VST3/Release/DiffusionDelay.vst3`. Each build also runs the Steinberg validator on the plug-in, and a validation failure fails the build.

To open the project in the Visual Studio IDE, use `build/DiffusionDelay.sln`.

## Testing

```powershell
cmake --build build --config Release --target DiffusionDelayTests
ctest --test-dir build -C Release --output-on-failure -j 4
```

The tests load the built plug-in and drive it like a host, including its panel. `AGENTS.md` explains how they work.

## Installing your own build

Close your host, then run this from an elevated PowerShell in the project folder:

```powershell
Copy-Item -Recurse -Force build/VST3/Release/DiffusionDelay.vst3 "C:\Program Files\Common Files\VST3\"
```

## Project layout

| Path | Purpose |
| --- | --- |
| `src/PluginConfig.h` | Plug-in name, vendor details and class IDs |
| `src/PluginEntry.cpp` | VST3 module entry, registers the classes below |
| `src/Parameters.*` | Parameter IDs, ranges and text, plus saving and loading their values |
| `src/Processor.*` | Runs the loop, the dry path and the controls |
| `src/Controller.*` | Registers the parameters and creates the editor |
| `src/Editor.*` | The plug-in window |
| `src/dsp/DiffusionLoop.h` | The diffusion network and the feedback path, which are the heart of it |
| `src/dsp/` | Also the delay line and parameter smoothing |
| `tests/` | Tests that drive the built plug-in like a host |
| `installer/` | The Windows installer, and the script that tests it. The macOS installer is built by HadronycheCore's `tools/macos/` |
| `docs/` | The behaviour specification and release notes |

## Licence

DiffusionDelay is released under the MIT licence: see `LICENSE`.

It's built with the following, which keep their own licences:

- VST3 SDK: MIT (`external/vst3sdk/LICENSE.txt`)
- VSTGUI (`external/vst3sdk/vstgui4/LICENSE`)
- HadronycheCore: MIT (`external/hadronychecore/LICENSE`)
- Apple's AudioUnitSDK, on macOS only: Apache 2.0

VST is a registered trademark of Steinberg Media Technologies GmbH.
