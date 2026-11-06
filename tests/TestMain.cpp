// Usage: DiffusionDelayTests <path to the plug-in's DLL> [test name]
//        DiffusionDelayTests --list
//
// With no test name, everything runs except "cpu", which only reports a measurement.

#include "Harness.h"
#include "Tests.h"

#include "hadronyche/testhost/Runner.h"

#if defined(__linux__)
#include "hadronyche/testhost/PanelOpens.h"
#endif

#include <iterator>

using namespace DiffusionDelay::Tests;

namespace {

const Test kTests[] = {
    { L"class-ids", testClassIds },
    { L"parameter-ids", testParameterIds },
    { L"parameter-ranges", testParameterRanges },
    { L"saved-format", testSavedFormat },
    { L"buses", testBuses },
    { L"latency-report", testLatencyReport },

    { L"clean-echo", testCleanEcho },
    { L"echo-time-accuracy", testEchoTimeAccuracy },
    { L"feedback-decay", testFeedbackDecay },
    { L"endless-clean", testEndlessClean },
    { L"reflections-around-the-repeat", testReflectionsAroundTheRepeat },
    { L"tail-longer-than-reflections", testTailLongerThanReflections },
    { L"reflections-follow-time", testReflectionsFollowTime },
    { L"stereo-scatter", testStereoScatter },
    { L"mono-balance", testMonoBalance },
    { L"endless-diffused", testEndlessDiffused },
    { L"mass-builds-up", testMassBuildsUp },
    { L"damper-shelf", testDamperShelf },
    { L"damper-first-repeat-clean", testDamperFirstRepeatClean },
    { L"damper-endless", testDamperEndless },
    { L"sync-time", testSyncTime },
    { L"sync-follows-tempo", testSyncFollowsTempo },
    { L"sync-off-ignores-tempo", testSyncOffIgnoresTempo },
    { L"sync-without-tempo", testSyncWithoutTempo },
    { L"sync-stays-in-range", testSyncStaysInRange },
    { L"time-glide", testTimeGlide },
    { L"held-note-stays-bounded", testHeldNoteStaysBounded },
    { L"dc-offset", testDcOffset },
    { L"broken-input", testBrokenInput },
    { L"extreme-settings", testExtremeSettings },
    { L"sample-rates", testSampleRates },
    { L"dry-wet", testDryWet },
    { L"volume", testVolume },
    { L"block-sizes", testBlockSizes },
    { L"determinism", testDeterminism },
    { L"silence-flags", testSilenceFlags },
    { L"parameter-state", testParameterState },
    { L"parameter-text", testParameterText },

#if defined(_WIN32)
    { L"panel", testPanel },
#elif defined(__linux__)
    { L"panel-opens", Hadronyche::TestHost::testPanelOpens },
#endif
};

} // namespace

HADRONYCHE_TEST_MAIN
{
    return runTests (argc, argv, { "DiffusionDelayTests", kTests, std::size (kTests), measureCpu });
}
