#include "Editor.h"

#include "Controller.h"
#include "Parameters.h"

#include "vstgui/vstgui.h"

using namespace VSTGUI;

namespace DiffusionDelay {

namespace {

constexpr Hadronyche::PanelLayout kLayout {};

// For whichever of Time and Division isn't in use.
constexpr float kInactiveAlpha = 0.35f;

const Hadronyche::PanelTheme kTheme {
    CColor (26, 32, 36),
    CColor (40, 49, 54),
    CColor (68, 82, 90),
    CColor (92, 190, 176),
    CColor (225, 230, 232),
    CColor (158, 170, 175),
};

} // namespace

Editor::Editor (Controller* controller)
: PanelEditor (controller, kLayout.rowWidth ({ 6, 2 }), kLayout.rowHeight (), getParamSpec, kParamCount, kTheme, kLayout), _controller (controller)
{
}

void Editor::buildPanel ()
{
    const int knobY = kLayout.knobRow ();

    {
        const int left = addColumnSection (6, "Echo");
        addKnob (left, knobY, kTime, "Time");
        addMenu (left + kLayout.columnWidth, knobY, kSync, "Sync");
        addMenu (left + 2 * kLayout.columnWidth, knobY, kDivision, "Division");
        addKnob (left + 3 * kLayout.columnWidth, knobY, kFeedback, "Feedback");
        addKnob (left + 4 * kLayout.columnWidth, knobY, kDiffusion, "Diffusion");
        addKnob (left + 5 * kLayout.columnWidth, knobY, kDamper, "Damper");
    }
    {
        const int left = addColumnSection (2, "Output");
        addKnob (left, knobY, kDryWet, "Dry/wet");
        addKnob (left + kLayout.columnWidth, knobY, kVolume, "Volume");
    }
}

void Editor::onSync ()
{
    const bool isSynced = _controller->getParamNormalized (kSync) >= 0.5;

    for (const auto& binding : getBindings ())
    {
        const auto id = static_cast<Steinberg::Vst::ParamID> (binding.control->getTag ());

        const bool isInactive = (id == kTime && isSynced) || (id == kDivision && !isSynced);
        const float alpha = isInactive ? kInactiveAlpha : 1.0f;
        if (binding.control->getAlphaValue () != alpha)
        {
            binding.control->setAlphaValue (alpha);
            if (binding.valueLabel)
                binding.valueLabel->setAlphaValue (alpha);
        }
    }
}

} // namespace DiffusionDelay
