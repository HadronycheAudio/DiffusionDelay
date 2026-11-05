#pragma once

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

enum class Curve
{
    Linear,
    Log,
    List
};

struct ParamSpec
{
    const char* title;
    const char* units;
    double min;
    double max;
    double defaultValue; // In plain units, like min and max.
    Curve curve;
    int precision;
    const char* const* labels; // Curve::List only.
    int labelCount;
};

const ParamSpec& getParamSpec (Steinberg::Vst::ParamID id);

// Conversions between the host's 0..1 values and the units in ParamSpec.
double normalizedToPlain (const ParamSpec& spec, double normalized);
double plainToNormalized (const ParamSpec& spec, double plain);

// At or below this, kVolume is silent.
constexpr double kVolumeSilenceDb = -60.0;

// The range of kTime. A synced time is held to it as well, and the longest sets the size of the delay lines.
constexpr double kMinTimeMs = 10.0;
constexpr double kMaxTimeMs = 10000.0;

// The length of an entry in kDivision's list, in quarter notes.
double getDivisionBeats (int division);

using ParamValues = std::array<double, kParamCount>;

// Parameters are saved as ID and value pairs, so that newer versions can add parameters and still
// read older projects.
bool writeParamState (Steinberg::IBStream* stream, const ParamValues& normalizedValues);

// Calls onValue (id, normalized) for every known parameter in the stream.
bool readParamState (Steinberg::IBStream* stream, const std::function<void (Steinberg::Vst::ParamID, double)>& onValue);

} // namespace DiffusionDelay
