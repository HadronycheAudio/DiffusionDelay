#include "Parameters.h"

#include "hadronyche/vst/ParamState.h"

#include <algorithm>

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
    { "Volume", "dB", kVolumeSilenceDb, 6.0, 0.0, Curve::Linear, 1, nullptr, 0, true },
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

bool writeParamState (IBStream* stream, const ParamValues& normalizedValues)
{
    return Hadronyche::writeParamState (stream, kStateVersion, normalizedValues.data (), kParamCount);
}

bool readParamState (IBStream* stream, const std::function<void (Vst::ParamID, double)>& onValue)
{
    return Hadronyche::readParamState (stream, kStateVersion, kParamCount,
                                       [&onValue] (Vst::ParamID id, double value, int32) { onValue (id, value); });
}

} // namespace DiffusionDelay
