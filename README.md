# DiffusionDelay

<img src="docs/images/VST_Compatible_Logo_Steinberg_with_TM.png" alt="VST Compatible" width="110">

A VST® 3 effect by Hadronyche: an echo with a reverb inside its feedback loop. Diffusion adds early reflections before each repeat and a tail after it, scattered across the stereo image. With the feedback at 100%, the repeats never end, and each pass dissolves them a little further until there's only a mass of diffused sound. It's built directly on the Steinberg VST 3 SDK and VSTGUI.

`docs/signal-path.md` describes the design and how each part behaves.

## Download

**On Windows,** get the installer from the [latest release](https://github.com/HadronycheAudio/DiffusionDelay/releases/latest). It needs 64-bit Windows 10 or later, and installs to `C:\Program Files\Common Files\VST3`. The installer isn't code-signed, so Windows will probably warn you: choose **More info**, then **Run anyway**.

**On a Mac,** get `DiffusionDelay-<version>-macOS.pkg` from the same page. It's a VST 3 and an Audio Unit, for Intel and Apple Silicon, on macOS 10.15 or later. It installs to `/Library/Audio/Plug-Ins`. The installer isn't signed by Apple, so macOS will refuse it the first time: the release notes explain how to open it. See [macOS: untested](#macos-untested) first.

**On Linux,** get `DiffusionDelay-<version>-Linux-x86_64.tar.gz` from the same page. It's a VST 3 for x86_64, and it needs glibc 2.34 or later, as on Ubuntu 22.04, Debian 12 or newer. Extract it where hosts look:

```bash
mkdir -p ~/.vst3
tar -xzf DiffusionDelay-<version>-Linux-x86_64.tar.gz -C ~/.vst3
```

It needs a few common libraries, which you probably have. On Debian or Ubuntu:

```bash
sudo apt install libxcb-cursor0 libxcb-util1 libxkbcommon-x11-0 libcairo2 libpango-1.0-0 libpangocairo-1.0-0
```

There's also an LV2 version, in `DiffusionDelay-<version>-Linux-x86_64-LV2.tar.gz`, with the same panel. Extract it into `~/.lv2`. See [Linux: lightly tested](#linux-lightly-tested) first.

## macOS: untested

> [!WARNING]
> **The Mac versions are untested.** They pass automated tests and Apple's Audio Unit validation on build machines, on both Intel and Apple Silicon, but nobody has used them in a real host on a real Mac yet. Nothing has checked how they sound, or how the panel looks.

**Are you a Mac user who's ready to commit to testing? You're more than welcome!** Please [open an issue](https://github.com/HadronycheAudio/DiffusionDelay/issues/new) saying which Mac, macOS version and host you use.

To build it yourself, you need Xcode, CMake 3.25 or newer, and an internet connection the first time, which fetches Apple's AudioUnitSDK:

```bash
git clone --recurse-submodules https://github.com/HadronycheAudio/DiffusionDelay.git
cd DiffusionDelay
cmake -S . -B build -G Xcode
cmake --build build --config Release --target DiffusionDelay DiffusionDelay-au
```

This produces `build/VST3/Release/DiffusionDelay.vst3` and `build/VST3/Release/DiffusionDelay.component`, for Intel and Apple Silicon, and runs the Steinberg validator on the plug-in. The build also copies the Audio Unit to `~/Library/Audio/Plug-Ins/Components`, where it keeps pointing at your build folder. To use the VST 3, copy it to `~/Library/Audio/Plug-Ins/VST3`.

To run the tests, which leave out the Windows-only panel test:

```bash
cmake --build build --config Release --target DiffusionDelayTests
ctest --test-dir build -C Release --output-on-failure
```

## Linux: lightly tested

> [!NOTE]
> **The Linux version is new.** It passes the automated tests on Ubuntu 22.04 and 24.04 and Debian 12 and 13, and it has been tried briefly by hand in REAPER on Debian 13. Other hosts and distributions haven't been tried. Please [open an issue](https://github.com/HadronycheAudio/DiffusionDelay/issues/new) saying which distribution and host you use if something goes wrong.

To build it yourself, you need CMake 3.25 or newer, and the packages that this script installs on Debian or Ubuntu:

```bash
git clone --recurse-submodules https://github.com/HadronycheAudio/DiffusionDelay.git
cd DiffusionDelay
bash external/hadronychecore/tools/linux/install-build-packages.sh
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release --target DiffusionDelay
```

This produces `build/Release/VST3/Release/DiffusionDelay.vst3`, and runs the Steinberg validator on the plug-in. To use it, copy it to `~/.vst3`.

To build the LV2 too, install `lv2-dev`, and build the target `DiffusionDelay-lv2`. This produces `build/Release/LV2/DiffusionDelay.lv2`, and to use it, copy it to `~/.lv2`. The tests that compare it with the VST 3 also need the target `DiffusionDelayLv2Compare`.

To run the tests, which include one that opens the panel and so needs a display or `xvfb-run`:

```bash
cmake --build build/Release --target DiffusionDelayTests
xvfb-run -a ctest --test-dir build/Release --output-on-failure
```

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
| `installer/` | The Windows installer, and the script that tests it. The macOS installer and the Linux tarball are built by HadronycheCore's `tools/macos/` and `tools/linux/` |
| `docs/` | The behaviour specification and release notes |

## Contributing

DiffusionDelay is free software that follows its author's own workflow, and there's no company or support promise behind it. If you find it handy and want it to work better with your host, your distribution or your way of working, you're welcome to take it further. Please [open an issue](https://github.com/HadronycheAudio/DiffusionDelay/issues/new) or a pull request. The LV2 version is new, and help testing in other hosts is especially useful. `AGENTS.md` lists the rules that keep people's saved projects safe, so please read it before changing anything.

## Licence

DiffusionDelay is released under the MIT licence: see `LICENSE`.

It's built with the following, which keep their own licences:

- VST3 SDK: MIT (`external/vst3sdk/LICENSE.txt`)
- VSTGUI (`external/vst3sdk/vstgui4/LICENSE`)
- HadronycheCore: MIT (`external/hadronychecore/LICENSE`)
- Apple's AudioUnitSDK, on macOS only: Apache 2.0

VST is a registered trademark of Steinberg Media Technologies GmbH.
