# AGENTS.md

Guidance for coding agents working in this repository. Read [Rules that protect users' work](#rules-that-protect-users-work) before changing anything.

DiffusionDelay is a VST® 3 effect for Windows: an echo with a lossless diffusion network inside its feedback loop. It uses the Steinberg VST3 SDK and VSTGUI directly. JUCE was deliberately rejected, so don't introduce it.

`docs/signal-path.md` is the behaviour specification. `README.md` covers building and usage.

Code shared with the other Hadronyche plug-ins is in the `HadronycheCore` submodule, at `external/hadronychecore`: the parameters' curves and saved format, `PanelEditor`, `Smoother`, the test host and the build setup. Its `AGENTS.md` has its own rules.

## Commands

Windows x64 with Visual Studio 2022 and CMake 3.25 or newer is where this is developed. macOS is built and tested by CI, on Intel and Apple Silicon, so see [macOS](#macos).

```powershell
# Clone. Without long paths, cloning into a folder deeper than about 100 characters fails partway
# through the SDK, because some of its sample files exceed the Windows path limit.
git -c core.longpaths=true clone --recurse-submodules <repo-url>

# In an existing clone, if submodules are missing
git -c core.longpaths=true submodule update --init --recursive

# Configure
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# Build. This also runs the Steinberg validator, and a failure fails the build.
cmake --build build --config Release --target DiffusionDelay
cmake --build build --config Debug --target DiffusionDelay

# Run the validator on its own, for example with extra output
build/bin/Release/validator.exe build/VST3/Release/DiffusionDelay.vst3

# Build and run all the tests
cmake --build build --config Release --target DiffusionDelayTests
ctest --test-dir build -C Release --output-on-failure -j 4

# Run a single test, and list them
ctest --test-dir build -C Release --output-on-failure -R "^endless-diffused$"
ctest --test-dir build -C Release -N

# Install for testing in Cubase. Close Cubase first, and use an elevated shell.
Copy-Item -Recurse -Force build/VST3/Release/DiffusionDelay.vst3 "C:\Program Files\Common Files\VST3\"
```

There's no linter or formatter.

Follow the existing style, which comes from the SDK: a space before call parentheses (`foo (x)`), `_member` for private members, `kName` for constants, and comments that explain why rather than what.

## Rules that protect users' work

People use this plug-in in real projects. Breaking these rules damages saved projects, and usually nothing fails at build time.

### Saved projects

- **Never change the Audio Unit codes,** `aufx` `DfDl` here and the manufacturer `Hdrn` in HadronycheCore. Hosts use them to find the plug-in in saved projects.
- **Never change the class IDs in `src/PluginConfig.h`.** Hosts use them to find the plug-in in saved projects.
- **Never renumber, reorder or remove entries in `ParamId`** (`src/Parameters.h`). Their values are saved in projects and automation. Add new parameters just before `kParamCount`, and add the matching row to `kSpecs` in `src/Parameters.cpp`, which is listed in the same order.
- **Don't change the range or curve of an existing parameter.** Hosts store values normalized to 0..1, so this silently changes what saved projects sound like. If it's unavoidable, migrate old values and bump the state version.
- **Only ever add to the end of `kSyncModes` and `kDivisions`,** and keep `kDivisionBeats` in step with `kDivisions`. Projects save a position in the list, so a new note value goes at the end even though the list is otherwise in order of length.
- **Changing the saved format requires bumping `kStateVersion` and still reading every older version.** The format is `int32` version, `int32` count, then `int32` ID and `double` normalized value pairs (`writeParamState`). A state without a parameter leaves it at its default, and an unknown one is skipped.
- **Loading must tolerate missing or damaged data.**
- **Changing the constants in `DiffusionLoop` changes how saved projects sound,** though nothing fails. That goes for the share tables, the angles, the output gain and the damper's corner.

### The loop

The owner's main requirement is that at 100% feedback the repeats never end, while they dissolve. That only works if the loop has a gain of exactly 1 at every frequency, so:

- **Everything between the input and the output of the network has to be lossless.** That means delays and rotations only. A gain, a filter, a convolution or a wet/dry blend in there will either fade the repeats, make them ring at some pitches, or make the loop run away, and it may only show after a minute at 100%.
- **Every row of `kStageShares` has to add up to 1000,** and `setTime` has to keep each channel's stages adding up to exactly T. Otherwise, a clean echo smears, and it's not exactly on time.
- **What's in the feedback path may never have a gain above 1 at any frequency.** That's why the damper is a first-order shelf, the DC blocker has its gain, and interpolation is linear.
- **Don't add a sample to the trip around the loop.** Returning signal goes in on the same sample that it comes out, which is safe because every line has been read by then, and `feedback-decay` fails otherwise.
- **The damper stays after the output tap,** so that the first repeat isn't damped.
- **At 0% Diffusion, the plug-in is an exact clean delay.** `clean-echo` holds it to that.

### The audio thread

`Processor::process` and everything it calls runs on the audio thread, including `src/dsp/`, except `DiffusionLoop::prepare` and `DelayLine::prepare`.

- **No allocation, locks, file access, system calls or anything else that can block.** This includes `std::vector` growth, `std::string` and `std::function` copies. The failure is audio dropouts that may not show up in testing.

### Sound and timing

- **The output doesn't depend on the block size,** and `block-sizes` checks for exact equality. Anything that changes with time has to count samples, not blocks.
- **`Hadronyche::Smoother` must stay double precision.** In single precision it stalls just short of its target. The same goes for the DC blocker, whose pole is very close to 1.

### Decisions made by the project owner

These were chosen deliberately, so don't "fix" them without the owner's agreement:

- Diffusion has reflections before the repeat and a tail after it, scattered across the stereo image, all inside the feedback loop and scaled by the delay time.
- At 100% feedback, the repeats never end, and they dissolve until there's only a mass of sound.
- Time goes up to 10 s, with tempo sync as an alternative to it.
- The damper is a shelf above about 4 kHz, in the feedback loop.

In `docs/signal-path.md`, **Requirement** marks the owner's decisions and **Proposed** marks suggestions that were accepted by default. Update it when behaviour changes, and don't turn a Proposed item into a Requirement yourself.

### The repository

- **Don't edit anything in `external/vst3sdk`.** It's a submodule pinned to `v3.8.1_build_84`. `git submodule status` shows a misleading `v3.7.3...` label for that same commit, so check with `git -C external/vst3sdk describe --tags`.
- **Don't edit `external/hadronychecore` from here.** Change it in its own repository, and move the submodule here afterwards, once the new version has been compared with the old as its `AGENTS.md` describes.
- Don't change git configuration. Commit or push only when asked.

## Architecture

### Signal path

`Processor::render` runs this for every sample:

```
             ┌──────────────────────────────────────────────────────┐
In (stereo) ─┤                                                      ├→ Dry/wet → Volume → Out
             └→ + → Diffusion network, T long ─┬→ Output gain ──────┘
                ↑                              │
                └─ Limiter ← Feedback ← DC blocker ← Damper
```

### Threads

| Thread | Runs | Talks to the others through |
| --- | --- | --- |
| Audio | `Processor::process` | Atomics only |
| UI | `Editor`, on a 33 ms timer | The controller's parameters |
| Host | `setState`, `getState`, `setActive` | Atomics |

The processor and controller share no memory, so the plug-in is marked as distributable. `getParamSpec` is the one definition of every parameter's range and text, shared by `Processor`, `Controller` and `Editor`. The last parameter value per block is used, followed by smoothing.

### The network

`docs/signal-path.md` explains how `DiffusionLoop` works, and why it isn't a generated impulse response.

- **Control rate:** Diffusion and Damper update every 16 samples, because of their sines and cosines. Feedback and the gains run per sample.
- **Memory:** the lines are sized for the longest Time when the plug-in is activated, which is about 24 MB at 48 kHz and 94 MB at 192 kHz. They're not resized, so the audio thread never allocates.
- **Tempo:** read at the start of each block, and `Processor::getTimeMs` turns it and Division into a time. Sync is otherwise just another way of setting Time.
- **Time:** each line glides to its own whole number of samples with the same time constant, so that the channels stay equal all the way there, and `DelayLine::read` only interpolates while moving.
- **Sleep:** with a silent input, and a silent loop for longer than the longest path through it, the plug-in stops working and flags the output as silent. Parameter changes then apply without a ramp.

### The editor

`Editor.cpp` builds the panel in `buildPanel`, on `Hadronyche::PanelEditor`, with no `.uidesc` file. A 33 ms timer in `PanelEditor` pushes parameter values to the controls, and `onSync` dims whichever of Time and Division isn't in use.

- A control's tag is its `ParamId`.

## Verifying a change

Before calling a change done:

1. **Build Debug and Release, and run the tests on both.** The validator runs as part of the build, but it only proves that the plug-in follows the VST3 rules.
2. **Add a test for any new behaviour,** and check that it fails without your change.
3. **Test it in Cubase** if you can, or say clearly that you couldn't. The tests measure the output, and nothing about how it sounds. Don't report behaviour you haven't observed as working.

### The tests

`.github/workflows/ci.yml` builds Debug and Release and runs all of this on every push and pull request. Don't weaken or skip it to get a change through.

`tests/` builds one program, `DiffusionDelayTests`, which loads the built plug-in and drives it through the VST3 interfaces, the way a host does.

| File | Covers |
| --- | --- |
| `CompatibilityTests.cpp` | Class IDs, parameter IDs and ranges, the saved format, the buses and the latency |
| `EchoTests.cpp` | The echo, the reflections and tail, the endless repeats, the damper, moving the controls, the output, state and parameter text |
| `PanelTest.cpp` | The real editor, driven with mouse messages |

- **Never change an expected value in `CompatibilityTests.cpp` to make a test pass.** Those values are written out by hand on purpose, and a failure there means the change would damage saved projects. Adding new parameters to them is fine, and expected.
- **Don't loosen a limit elsewhere to get a test passing** without working out why it failed. The `endless` tests in particular are what stands between a change and a loop that fades or runs away a minute in.
- **To add a test,** add it to `Tests.h`, `kTests` in `TestMain.cpp` and `DIFFUSIONDELAY_TESTS` in `tests/CMakeLists.txt`.
- **`PanelTest.cpp` clicks fixed positions,** so moving the panel's controls means updating the positions at the top of that file. It can't open the menus, whose pop-ups would block it.
- **To see the panel,** set `DIFFUSIONDELAY_SCREENSHOTS` to a folder before running the `panel` test, which saves a BMP at each step.
- **To measure CPU use,** run `build/bin/Release/DiffusionDelayTests.exe <plug-in DLL> cpu`, in a Release build. The DLL is at `build/VST3/Release/DiffusionDelay.vst3/Contents/x86_64-win/DiffusionDelay.vst3`.

## Releasing

Pushing a tag `v<version>` publishes a release, once everything has passed:

1. Set the version in `project()` in `CMakeLists.txt`. CI fails if the tag doesn't match it.
2. Add `docs/release-notes/v<version>.md`, which becomes the release's text.
3. Commit, push, and wait for CI to pass.
4. Tag that commit and push the tag.

CI builds the installer on every push, installs it on its own machine, runs the validator and all the tests on the installed copy, and uninstalls it. `installer/test-installer.ps1` writes to Program Files, so don't run it on a machine you care about.

Rules for releases:

- **Don't change `AppId` in `installer/DiffusionDelay.iss`.** It's how an upgrade finds the previous version.
- **Keep `SMTG_USE_STATIC_CRT` on,** which HadronycheCore's `cmake/HadronychePlugin.cmake` does. Without it, the plug-in fails to load on PCs without Microsoft's runtime, and neither the tests nor CI would notice, because that runtime is on those machines. If you configured a build before this option existed, its cached value overrides the default.
- **The installer must keep shipping the licence files.** The VST 3 SDK's, VSTGUI's and HadronycheCore's licences require their notices to accompany binaries.
- **Follow Steinberg's usage guidelines** (`external/vst3sdk/VST3_Usage_Guidelines.pdf`) wherever "VST" appears in product-related material: use ® on first mention, include the sentence "VST is a registered trademark of Steinberg Media Technologies GmbH.", show the logo from `docs/images/` unaltered, and don't put "VST" in product or company names.

## macOS

- **Build with the Xcode generator,** which the SDK's Audio Unit wrapper needs: `cmake -S . -B build -G Xcode`, then `cmake --build build --config Release --target DiffusionDelayTests DiffusionDelay DiffusionDelay-au`.
- **`CMAKE_OSX_DEPLOYMENT_TARGET` has to stay before `project ()`,** or the plug-in only loads on macOS as new as the build machine, and only an older Mac would notice. `test-installer.sh` checks for this, because a host on the build machine wouldn't.
- **The SDK's `-ffast-math` stays off,** as HadronycheCore's AGENTS.md explains.
- **The installer is built and tested by HadronycheCore's `tools/macos/`.** `test-installer.sh` writes to `/Library`, so it's meant for CI machines.
- **The panel test only exists on Windows,** because it uses Windows' mouse messages. Nothing automated opens the panel on macOS, so its look needs checking by eye.

## Linux

- **Use one build folder per configuration, with Ninja:** `cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release`, then `cmake --build build/Release --target DiffusionDelayTests DiffusionDelay`. The SDK's Linux output paths break with a multi-configuration generator.
- **`external/hadronychecore/tools/linux/install-build-packages.sh` installs the build's packages.** gtkmm and SQLite are only needed to configure, and the plug-in doesn't link them.
- **`panel-opens` needs an X server,** so run `xvfb-run -a ctest ...` where there's no display. It opens and closes the real editor, and nothing more: `panel`, which drives it, only exists on Windows, so the panel's look and its controls need checking by eye.
- **The release is built in an `ubuntu:22.04` container on CI, not on the runner,** because a binary only loads on a glibc as new as the one it's built against. HadronycheCore's `tools/linux/test-tarball.sh` checks this, and CI runs the tarball on other distributions too.
- **There's no LV2 version yet.** Linux releases are VST 3 only.

## Known limits

- It has only been used on Windows, and other hosts than the owner's haven't been tried.
- The installer isn't code-signed, so Windows warns users when they run it. The macOS one isn't signed by Apple or notarized, so macOS refuses it until the user allows it.
- The Linux version has only been tried briefly, in REAPER on Debian 13, by the owner.
- The test that opens the panel crashed once on CI as the test program exited, after all its checks had passed. It happened once in about 33 runs, and about 260 further runs didn't reproduce it. The cause isn't known, and it hasn't been seen in a host.
- The Mac version hasn't been used in a real host on a real Mac. The tests measure its output, and Apple's `auval` validates it, on CI only.
- Holding a loud note into 100% feedback can reach about +9 dB, which is where the limiter in the feedback stops it. There's no limiter on the output.
- At 100% feedback, the mass settles below the first repeat, by 1.2 dB at 100% Diffusion and 4.4 dB at 30%. The output is a stereo tap of eight channels, so it can't hear all the energy once it has scattered, and the output gain can't make up for all of that without the first repeat becoming louder than the input.
- The mass keeps a faint pulse at T, with a correlation of about 0.4 from one repeat to the next, because the straight path never quite goes.
- The DC blocker very slowly takes the lowest notes away at short times: a 40 Hz note at 100 ms loses 1.6 dB per minute at 100% feedback.
