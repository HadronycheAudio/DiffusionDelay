#pragma once

namespace DiffusionDelay {
namespace Tests {

// CompatibilityTests.cpp
void testClassIds ();
void testParameterIds ();
void testParameterRanges ();
void testSavedFormat ();
void testBuses ();
void testLatencyReport ();

// EchoTests.cpp
void testCleanEcho ();
void testEchoTimeAccuracy ();
void testFeedbackDecay ();
void testEndlessClean ();
void testReflectionsAroundTheRepeat ();
void testTailLongerThanReflections ();
void testReflectionsFollowTime ();
void testStereoScatter ();
void testMonoBalance ();
void testEndlessDiffused ();
void testMassBuildsUp ();
void testDamperShelf ();
void testDamperFirstRepeatClean ();
void testDamperEndless ();
void testSyncTime ();
void testSyncFollowsTempo ();
void testSyncOffIgnoresTempo ();
void testSyncWithoutTempo ();
void testSyncStaysInRange ();
void testTimeGlide ();
void testHeldNoteStaysBounded ();
void testDcOffset ();
void testBrokenInput ();
void testExtremeSettings ();
void testSampleRates ();
void testDryWet ();
void testVolume ();
void testBlockSizes ();
void testDeterminism ();
void testSilenceFlags ();
void testParameterState ();
void testParameterText ();
void measureCpu ();

// PanelTest.cpp
void testPanel ();

} // namespace Tests
} // namespace DiffusionDelay
