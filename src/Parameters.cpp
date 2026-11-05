#include "Parameters.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;

namespace DiffusionDelay {

namespace {

constexpr int32 kStateVersion = 1;

// Saved in projects by position, so only ever add to the end of these. Note values rather than bars, so that
// they mean the same whatever the time signature.
const char* const kSyncModes[] = { "Off", "On" };
constexpr int kSyncModeCount = 2;

const char* const kDivisions[] = { "1/32", "1/16T", "1/32D", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4",
                                   "1/2T", "1/4D", "1/2", "1/1T", "1/2D", "1/1", "1/1D", "2/1", "4/1" };
constexpr int kDivisionCount = 19;
constexpr double kDivisionBeats[kDivisionCount] = { 1.0 / 8.0, 1.0 / 6.0, 3.0 / 16.0, 1.0 / 4.0, 1.0 / 3.0, 3.0 / 8.0, 1.0 / 2.0, 2.0 / 3.0, 3.0 / 4.0, 1.0,
                                                    4.0 / 3.0, 3.0 / 2.0, 2.0, 8.0 / 3.0, 3.0, 4.0, 6.0, 8.0, 16.0 };

// Listed in ParamId order.
const std::array<ParamSpec, kParamCount> kSpecs { {
    { "Time", "ms", kMinTimeMs, kMaxTimeMs, 375.0, Curve::Log, 0 },
    { "Feedback", "%", 0.0, 100.0, 50.0, Curve::Linear, 0 },
    { "Diffusion", "%", 0.0, 100.0, 50.0, Curve::Linear, 0 },
    { "Damper", "dB", 0.0, 18.0, 3.0, Curve::Linear, 1 },
    { "Dry/wet", "%", 0.0, 100.0, 50.0, Curve::Linear, 0 },
    { "Volume", "dB", kVolumeSilenceDb, 6.0, 0.0, Curve::Linear, 1 },
    { "Sync", "", 0.0, kSyncModeCount - 1, 0.0, Curve::List, 0, kSyncModes, kSyncModeCount },
    { "Division", "", 0.0, kDivisionCount - 1, 8.0, Curve::List, 0, kDivisions, kDivisionCount },
} };

} // namespace

const ParamSpec& getParamSpec (Vst::ParamID id)
{
    return kSpecs[id];
}

double getDivisionBeats (int division)
{
    return kDivisionBeats[std::clamp (division, 0, kDivisionCount - 1)];
}

double normalizedToPlain (const ParamSpec& spec, double normalized)
{
    const double n = std::clamp (normalized, 0.0, 1.0);
    if (spec.curve == Curve::List)
        return std::round (n * (spec.labelCount - 1));
    if (spec.curve == Curve::Log)
        return spec.min * std::pow (spec.max / spec.min, n);
    return spec.min + (spec.max - spec.min) * n;
}

double plainToNormalized (const ParamSpec& spec, double plain)
{
    const double p = std::clamp (plain, spec.min, spec.max);
    if (spec.curve == Curve::Log)
        return std::log (p / spec.min) / std::log (spec.max / spec.min);
    return (p - spec.min) / (spec.max - spec.min);
}

bool writeParamState (IBStream* stream, const ParamValues& normalizedValues)
{
    IBStreamer streamer (stream, kLittleEndian);
    if (!streamer.writeInt32 (kStateVersion) || !streamer.writeInt32 (kParamCount))
        return false;

    for (int32 id = 0; id < kParamCount; ++id)
    {
        if (!streamer.writeInt32 (id) || !streamer.writeDouble (normalizedValues[static_cast<size_t> (id)]))
            return false;
    }
    return true;
}

bool readParamState (IBStream* stream, const std::function<void (Vst::ParamID, double)>& onValue)
{
    IBStreamer streamer (stream, kLittleEndian);

    int32 version = 0;
    int32 count = 0;
    if (!streamer.readInt32 (version) || version < 1 || version > kStateVersion || !streamer.readInt32 (count))
        return false;

    for (int32 i = 0; i < count; ++i)
    {
        int32 id = 0;
        double value = 0.0;
        if (!streamer.readInt32 (id) || !streamer.readDouble (value))
            return false;

        if (id >= 0 && id < kParamCount && std::isfinite (value))
            onValue (static_cast<Vst::ParamID> (id), std::clamp (value, 0.0, 1.0));
    }
    return true;
}

} // namespace DiffusionDelay
