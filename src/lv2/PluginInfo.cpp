// What the LV2 wrapper and its Turtle need to know about this plug-in. The URI and the symbols are saved in hosts'
// projects, so never change them, and a new parameter adds a symbol at the end.

#include "ParamIds.h"

#include "hadronyche/lv2/PluginInfo.h"
#include "hadronyche/vst/RustPlugin.h"

namespace {

const char* const kSymbols[] = {
    "time",
    "feedback",
    "diffusion",
    "damper",
    "dry_wet",
    "volume",
    "sync",
    "division",
};

static_assert (sizeof (kSymbols) / sizeof (kSymbols[0]) == DiffusionDelay::kParamCount, "One symbol for each parameter");

} // namespace

const Hadronyche::Lv2::PluginInfo& Hadronyche::Lv2::getPluginInfo ()
{
    static const PluginInfo info {
        "https://github.com/HadronycheAudio/DiffusionDelay",
        "DiffusionDelay",
        "DelayPlugin",
        Hadronyche::RustPlugin::getSpec,
        DiffusionDelay::kParamCount,
        kSymbols,
        2, // LV2 treats an odd minor version as a development build.
        0,
        true, // Audio input
        false, // MIDI
        true, // Transport
        false, // Latency
    };
    return info;
}
