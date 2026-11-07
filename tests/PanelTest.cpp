// The real editor in an off-screen window, driven with mouse messages on its real controls, while audio
// runs. It doesn't touch the real mouse. Set the environment variable DIFFUSIONDELAY_SCREENSHOTS to a folder
// to also save the panel as a BMP file.

#include "Harness.h"
#include "Tests.h"

#include "hadronyche/testhost/Panel.h"

#include <cmath>
#include <cstdio>

namespace DiffusionDelay {
namespace Tests {

namespace {

// The centre of the Time knob, a little above it, which follows the layout in src/Editor.cpp.
constexpr int kTimeKnobX = 47;
constexpr int kTimeKnobY = 64;

} // namespace

void testPanel ()
{
    Plug plug;
    PanelWindow panel (plug.controller, L"DiffusionDelayPanelTest");
    check ("the controller creates an editor", panel.view != nullptr);
    if (!panel.view)
        return;

    check ("the editor opens", panel.open ());
    check ("it has a window", panel.editorWindow != nullptr);

    const auto tone = makeTones ({ { 440.0, 0.5 } }, kBlockSize);
    const auto runWithUi = [&] (double seconds) { panel.runFor (seconds, [&] { plug.process (tone); }); };

    runWithUi (0.3);
    panel.saveScreenshot ("DIFFUSIONDELAY_SCREENSHOTS", "panel-default");

    // A knob drag changes its parameter. The drag ends 1/6 of the way in, and a little more because the mouse is
    // slightly above the knob's centre.
    const double before = plug.controller->getParamNormalized (kTime);
    panel.dragLeft (kTimeKnobX, kTimeKnobY);
    const double after = plug.controller->getParamNormalized (kTime);
    std::printf ("  Time went from %.3f to %.3f\n", before, after);
    check ("dragging the Time knob moves Time to where the mouse is", std::abs (after - 0.184) < 0.02);

    // Automation reaches the panel: the controls aren't the source of these changes.
    plug.set (kTime, 800.0);
    plug.set (kFeedback, 85.0);
    plug.set (kDiffusion, 100.0);
    plug.set (kDamper, 9.0);
    plug.set (kDryWet, 30.0);
    plug.set (kVolume, -12.0);
    plug.set (kSync, 1);
    plug.set (kDivision, 9);
    runWithUi (0.3);
    panel.saveScreenshot ("DIFFUSIONDELAY_SCREENSHOTS", "panel-edited");

    check ("the editor closes", panel.close ());
}

} // namespace Tests
} // namespace DiffusionDelay
