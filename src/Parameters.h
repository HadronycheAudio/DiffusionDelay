#pragma once

#include "hadronyche/dsp/Gain.h"
#include "hadronyche/vst/ParamSpec.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <array>
#include <functional>

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

using Hadronyche::Curve;
using Hadronyche::ParamSpec;
using Hadronyche::kVolumeSilenceDb;
using Hadronyche::normalizedToPlain;
using Hadronyche::plainToNormalized;

const ParamSpec& getParamSpec (Steinberg::Vst::ParamID id);

// The range of kTime. A synced time is held to it as well, and the longest sets the size of the delay lines.
constexpr double kMinTimeMs = 10.0;
constexpr double kMaxTimeMs = 10000.0;

// The length of an entry in kDivision's list, in quarter notes.
double getDivisionBeats (int division);

using ParamValues = std::array<double, kParamCount>;

bool writeParamState (Steinberg::IBStream* stream, const ParamValues& normalizedValues);

// Calls onValue (id, normalized) for every known parameter in the stream.
bool readParamState (Steinberg::IBStream* stream, const std::function<void (Steinberg::Vst::ParamID, double)>& onValue);

} // namespace DiffusionDelay
