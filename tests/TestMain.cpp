// Usage: DiffusionDelayTests <path to the plug-in's DLL> [test name]
//        DiffusionDelayTests --list
//
// With no test name, everything runs except "cpu", which only reports a measurement.

#include "Harness.h"
#include "Tests.h"

#include <cstdio>
#include <cwchar>

using namespace DiffusionDelay::Tests;

namespace {

struct Test
{
    const wchar_t* name;
    void (*function) ();
};

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

    { L"panel", testPanel },
};

} // namespace

int wmain (int argc, wchar_t** argv)
{
    if (argc >= 2 && std::wcscmp (argv[1], L"--list") == 0)
    {
        for (const auto& test : kTests)
            std::wprintf (L"%ls\n", test.name);
        std::wprintf (L"cpu\n");
        return 0;
    }

    if (argc < 2)
    {
        std::printf ("Usage: DiffusionDelayTests <path to the plug-in's DLL> [test name]\n");
        return 2;
    }

    if (!loadPlugin (argv[1]))
    {
        std::wprintf (L"Could not load the plug-in: %ls\n", argv[1]);
        return 2;
    }

    const wchar_t* filter = argc >= 3 ? argv[2] : nullptr;
    bool foundTest = false;

    if (filter && std::wcscmp (filter, L"cpu") == 0)
    {
        foundTest = true;
        measureCpu ();
    }

    for (const auto& test : kTests)
    {
        if (filter && std::wcscmp (filter, test.name) != 0)
            continue;

        foundTest = true;
        std::wprintf (L"[%ls]\n", test.name);
        std::fflush (stdout);
        test.function ();
    }

    unloadPlugin ();

    if (!foundTest)
    {
        std::wprintf (L"No test named %ls. Use --list to see them.\n", filter);
        return 2;
    }

    std::printf ("\n%d failure(s)\n", getFailureCount ());
    return getFailureCount () == 0 ? 0 : 1;
}
