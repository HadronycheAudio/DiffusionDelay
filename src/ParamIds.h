#pragma once

// Generated from the parameter table in rust/, by cargo test there. Don't edit it here: change the table,
// and run HADRONYCHE_UPDATE_HEADERS=1 cargo test in rust/ to bring this up to date.

#include "pluginterfaces/vst/vsttypes.h"

namespace DiffusionDelay {

// Parameter IDs are saved in projects and automation: never renumber them, only append.
enum ParamId : Steinberg::Vst::ParamID
{
    kTime,
    kFeedback,
    kDiffusion,
    kDamper,
    kDryWet,
    kVolume,
    kSync,
    kDivision,
    kParamCount
};

} // namespace DiffusionDelay
