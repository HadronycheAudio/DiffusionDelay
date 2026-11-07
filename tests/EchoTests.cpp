// The behaviour in docs/signal-path.md, measured on the output of the real plug-in.

#include "Harness.h"
#include "Tests.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

using namespace Steinberg;

namespace DiffusionDelay {
namespace Tests {

namespace {

// Samples of a short burst of noise, which is what the energy measurements here listen to.
constexpr size_t kBurst = 480;

size_t samplesFor (double ms, double sampleRate = kSampleRate)
{
    return static_cast<size_t> (std::lround (ms / 1000.0 * sampleRate));
}

// Wet only, full level, no damping: what's left is what the loop does.
void setWetOnly (Plug& plug, double timeMs, double feedback, double diffusion, double damper = 0.0)
{
    plug.set (kTime, timeMs);
    plug.set (kFeedback, feedback);
    plug.set (kDiffusion, diffusion);
    plug.set (kDamper, damper);
    plug.set (kDryWet, 100.0);
    plug.set (kVolume, 0.0);
}

Samples makeImpulse (size_t count, double amplitude = 1.0)
{
    Samples samples (count, 0.0f);
    samples[0] = static_cast<float> (amplitude);
    return samples;
}

// The input first, then silence.
Samples withSilence (const Samples& input, size_t total)
{
    Samples samples = input;
    samples.resize (total, 0.0f);
    return samples;
}

double energy (const Samples& samples, size_t from, size_t count)
{
    double sum = 0.0;
    for (size_t i = from; i < std::min (samples.size (), from + count); ++i)
        sum += static_cast<double> (samples[i]) * samples[i];
    return sum;
}

double stereoEnergy (const Stereo& output, size_t from, size_t count)
{
    return energy (output.left, from, count) + energy (output.right, from, count);
}

double correlation (const Samples& a, const Samples& b, size_t from, size_t count)
{
    double ab = 0.0;
    double aa = 0.0;
    double bb = 0.0;
    for (size_t i = from; i < std::min (a.size (), from + count); ++i)
    {
        ab += static_cast<double> (a[i]) * b[i];
        aa += static_cast<double> (a[i]) * a[i];
        bb += static_cast<double> (b[i]) * b[i];
    }
    return ab / std::sqrt (std::max (aa * bb, 1e-30));
}

// How spiky a signal is. A clean repeat of a burst, separated by silence, is very spiky, and a steady mass isn't.
double crestDb (const Samples& samples, size_t from, size_t count)
{
    const auto part = slice (samples, from, count);
    return db (peak (part) / std::max (rms (part), 1e-12));
}

size_t firstAbove (const Samples& samples, double threshold)
{
    for (size_t i = 0; i < samples.size (); ++i)
    {
        if (std::abs (samples[i]) > threshold)
            return i;
    }
    return samples.size ();
}

} // namespace

void testCleanEcho ()
{
    Plug plug;
    setWetOnly (plug, 100.0, 0.0, 0.0);

    const size_t delay = samplesFor (100.0);
    const auto noise = makeNoise (0.5, kBurst);
    const auto output = plug.process (withSilence (noise, delay * 3));

    double worst = 0.0;
    for (size_t i = 0; i < kBurst; ++i)
        worst = std::max (worst, static_cast<double> (std::abs (output.left[delay + i] - noise[i])));
    std::printf ("  worst difference from the input: %.1f dB\n", db (worst));
    check ("with no diffusion, the repeat is the input, exactly", worst < 1e-6);
    check ("nothing comes out before it", peak (slice (output.left, 0, delay)) == 0.0);
    check ("nothing comes after it", peak (output.left, delay + kBurst) == 0.0);
}

void testEchoTimeAccuracy ()
{
    for (const double ms : { 10.0, 37.0, 375.0, 2000.0, 10000.0 })
    {
        Plug plug;
        setWetOnly (plug, ms, 0.0, 0.0);
        const size_t expected = samplesFor (ms);
        const auto output = plug.process (makeImpulse (expected + 100));
        const size_t found = firstAbove (output.left, 0.5);
        std::printf ("  %.0f ms: expected at %zu, found at %zu\n", ms, expected, found);
        check ("the repeat lands on the sample", found == expected);
    }
}

void testFeedbackDecay ()
{
    Plug plug;
    setWetOnly (plug, 50.0, 50.0, 0.0);

    const size_t delay = samplesFor (50.0);
    const auto output = plug.process (makeImpulse (delay * 5 + 10));
    for (int repeat = 1; repeat <= 4; ++repeat)
    {
        const double level = std::abs (output.left[delay * static_cast<size_t> (repeat)]);
        std::printf ("  repeat %d: %.1f dB\n", repeat, db (level));
        check ("each repeat is at the feedback's level", std::abs (level - std::pow (0.5, repeat - 1)) < 0.01);
    }
}

void testEndlessClean ()
{
    Plug plug;
    setWetOnly (plug, 100.0, 100.0, 0.0);

    const size_t delay = samplesFor (100.0);
    const auto noise = makeNoise (0.5, kBurst);
    const auto output = plug.process (withSilence (noise, delay * 601));

    // The first pass, and six hundred passes, or a minute, later.
    const double first = energy (output.left, delay, kBurst);
    const double last = energy (output.left, delay * 600, kBurst);
    std::printf ("  after a minute, the repeat has changed by %.3f dB\n", 10.0 * std::log10 (last / first));
    check ("at 100%, the repeats don't fade", std::abs (10.0 * std::log10 (last / first)) < 0.5);

    // On the change from sample to sample, because the only thing allowed between repeats is the very slow
    // movement left by the DC blocker, which is well below what can be heard.
    const auto changes = [&] (size_t from, size_t count) {
        double sum = 0.0;
        for (size_t i = from; i < from + count; ++i)
        {
            const double change = static_cast<double> (output.left[i + 1]) - output.left[i];
            sum += change * change;
        }
        return sum;
    };
    const double between = changes (delay * 600 + kBurst + 500, delay - kBurst - 1000);
    const double repeat = changes (delay * 600, kBurst);
    std::printf ("  between repeats: %.1f dB below them\n", 10.0 * std::log10 (std::max (between, 1e-30) / repeat));
    check ("and they stay separate, with nothing building up between them", between < 1e-6 * repeat);

    // The lows matter most here, since the blocker is what could take them away.
    Plug bass;
    setWetOnly (bass, 100.0, 100.0, 0.0);
    const auto note = bass.process (withSilence (makeTones ({ { 40.0, 0.5 } }, delay), delay * 600));
    const double bassChange = db (rms (slice (note.left, delay * 598, delay)) / rms (slice (note.left, delay, delay)));
    std::printf ("  a 40 Hz note changes by %.2f dB over a minute\n", bassChange);
    check ("the lows stay too", std::abs (bassChange) < 3.0);
}

void testReflectionsAroundTheRepeat ()
{
    const double timeMs = 400.0;
    const size_t delay = samplesFor (timeMs);

    for (const double diffusion : { 25.0, 50.0, 100.0 })
    {
        Plug plug;
        setWetOnly (plug, timeMs, 0.0, diffusion);
        const auto output = plug.process (withSilence (makeImpulse (1), delay * 6));

        const double repeat = std::pow (static_cast<double> (output.left[delay]), 2) + std::pow (static_cast<double> (output.right[delay]), 2);
        const double before = stereoEnergy (output, 0, delay);
        const double after = stereoEnergy (output, delay + 1, output.left.size ());
        const size_t earliest = std::min (firstAbove (output.left, 1e-4), firstAbove (output.right, 1e-4));

        std::printf ("  at %.0f%%: repeat %.1f dB, before it %.1f dB, after it %.1f dB, earliest at %.0f%% of the time\n", diffusion,
                     10.0 * std::log10 (repeat), 10.0 * std::log10 (before), 10.0 * std::log10 (after), 100.0 * static_cast<double> (earliest) / static_cast<double> (delay));

        check ("the repeat is still the loudest single moment", std::abs (output.left[delay]) > 3.0 * peak (slice (output.left, 0, delay)));
        check ("there are reflections before it", before > 1e-3);
        check ("there's more after it", after > before);
        check ("nothing arrives straight away", earliest > delay / 3);

        // The same impulse went into both channels, so the input's energy is 2. What scatters comes out a little
        // quieter than it went in: see the output gain in DiffusionLoop.
        const double total = 10.0 * std::log10 ((repeat + before + after) / 2.0);
        std::printf ("  the whole of the first pass is %.2f dB relative to the input\n", total);
        check ("diffusion spreads the input out, without changing its level much", std::abs (total) < 1.5);
    }

    Plug plug;
    setWetOnly (plug, timeMs, 0.0, 100.0);
    const auto full = plug.process (withSilence (makeImpulse (1), delay + 1));
    const double straight = db (std::abs (full.left[delay]));
    std::printf ("  the straight path at 100%%: %.1f dB\n", straight);
    check ("at 100%, a clear repeat is left", straight > -8.0 && straight < -3.0);
}

void testTailLongerThanReflections ()
{
    const double timeMs = 400.0;
    const size_t delay = samplesFor (timeMs);

    Plug plug;
    setWetOnly (plug, timeMs, 0.0, 100.0);
    const auto output = plug.process (withSilence (makeNoise (0.5, 1), delay * 6));

    // How far it reaches each way from the repeat, to 60 dB below the loudest sample.
    const double threshold = (peak (output.left)) * 1e-3;
    const size_t earliest = firstAbove (output.left, threshold);
    size_t latest = output.left.size () - 1;
    while (latest > 0 && std::abs (output.left[latest]) < threshold)
        --latest;

    const double earlyPart = static_cast<double> (delay - earliest) / static_cast<double> (delay);
    const double latePart = static_cast<double> (latest - delay) / static_cast<double> (delay);
    std::printf ("  it starts %.2f of the time before the repeat, and ends %.2f after it\n", earlyPart, latePart);
    check ("the tail reaches further than the early reflections", latePart > 1.5 * earlyPart);
}

void testReflectionsFollowTime ()
{
    const auto earliestShare = [] (double ms) {
        Plug plug;
        setWetOnly (plug, ms, 0.0, 100.0);
        const size_t delay = samplesFor (ms);
        const auto output = plug.process (withSilence (makeImpulse (1), delay * 2));
        return static_cast<double> (firstAbove (output.left, 1e-4)) / static_cast<double> (delay);
    };

    const double shortShare = earliestShare (100.0);
    const double longShare = earliestShare (1600.0);
    std::printf ("  the earliest reflection is at %.3f of 100 ms, and %.3f of 1600 ms\n", shortShare, longShare);
    check ("reflections scale with the echo time", std::abs (shortShare - longShare) < 0.02);
}

void testStereoScatter ()
{
    const double timeMs = 300.0;
    const size_t delay = samplesFor (timeMs);

    {
        // A mono input stays mono with no diffusion.
        Plug plug;
        setWetOnly (plug, timeMs, 0.0, 0.0);
        const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), delay * 2));
        check ("with no diffusion, mono stays mono", correlation (output.left, output.right, 0, output.left.size ()) > 0.9999);
    }
    {
        // The reflections spread out across the image, even from a mono input.
        Plug plug;
        setWetOnly (plug, timeMs, 0.0, 100.0);
        const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), delay * 3));
        const double around = correlation (output.left, output.right, delay / 2, delay / 2 - kBurst);
        std::printf ("  left and right correlation of the reflections before the repeat: %.2f\n", around);
        check ("reflections are different in each channel", std::abs (around) < 0.5);
    }
    {
        // A signal on one side stays there with no diffusion, and reaches both with it.
        const auto noise = withSilence (makeNoise (0.5, kBurst), delay * 3);
        const Samples silence (noise.size (), 0.0f);

        Plug clean;
        setWetOnly (clean, timeMs, 0.0, 0.0);
        const auto dry = clean.process (noise, silence);
        check ("with no diffusion, left stays on the left", peak (dry.right) == 0.0);

        Plug diffused;
        setWetOnly (diffused, timeMs, 0.0, 100.0);
        const auto wet = diffused.process (noise, silence);
        const double balance = 10.0 * std::log10 (energy (wet.right, 0, wet.right.size ()) / energy (wet.left, 0, wet.left.size ()));
        std::printf ("  with diffusion, the right has %.1f dB relative to the left\n", balance);
        check ("with diffusion, some of it reaches the other side", balance > -12.0);
        check ("while the repeat stays where it was", balance < -1.0);
    }
}

void testMonoBalance ()
{
    Plug plug;
    setWetOnly (plug, 200.0, 90.0, 100.0);
    const size_t delay = samplesFor (200.0);
    const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), delay * 40));

    const double balance = 10.0 * std::log10 (energy (output.left, 0, output.left.size ()) / energy (output.right, 0, output.right.size ()));
    std::printf ("  left is %+.2f dB relative to right\n", balance);
    check ("a mono input stays in the middle", std::abs (balance) < 1.5);
}

void testEndlessDiffused ()
{
    // The whole point: repeats are never lost, however far they've dissolved.
    for (const double diffusion : { 30.0, 100.0 })
    {
        Plug plug;
        setWetOnly (plug, 100.0, 100.0, diffusion);
        const size_t delay = samplesFor (100.0);
        const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), delay * 400));

        // Energy per repeat. The first one is measured over two repeats' length, which covers all of its tail.
        const size_t window = delay * 5;
        const double first = stereoEnergy (output, 0, delay * 2) * 5.0;
        const double settled = stereoEnergy (output, delay * 100, window);
        const double late = stereoEnergy (output, delay * 390, window);

        const double drift = 10.0 * std::log10 (late / settled);
        const double drop = 10.0 * std::log10 (late / first);
        std::printf ("  at %.0f%%: from 10 s to 39 s the level changes by %.2f dB, and it's %.2f dB relative to the first repeat\n", diffusion, drift, drop);
        check ("once it's settled, it doesn't fade or grow", std::abs (drift) < 0.25);

        // Fully scattered sound comes out 6 dB below a clean repeat, less the output gain. See DiffusionLoop.
        const double makeup = 3.0 * diffusion / 100.0;
        check ("and it's no quieter than the way out of the loop can explain", drop > -(6.0 - makeup) && drop < 1.0);
        check ("and doesn't run away", peak (output.left) < 2.0 && isFinite (output.left));
    }
}

void testMassBuildsUp ()
{
    Plug plug;
    setWetOnly (plug, 250.0, 100.0, 100.0);
    const size_t delay = samplesFor (250.0);
    const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), delay * 50));

    // The repeat's straight path falls on each pass, leaving something steadier.
    const auto crestAt = [&] (size_t repeat) { return crestDb (output.left, delay * repeat - delay / 2, delay); };
    const double first = crestAt (1);
    const double third = crestAt (3);
    const double last = crestAt (45);
    std::printf ("  crest factor at repeat 1: %.1f dB, repeat 3: %.1f dB, repeat 45: %.1f dB\n", first, third, last);
    check ("the first repeat stands out", first > 18.0);
    check ("by the last it's a mass, as steady as noise", last < 14.0);
    check ("and it gets there gradually", third < first);

    const double overlap = correlation (slice (output.left, delay * 40, delay), slice (output.left, delay * 41, delay), 0, delay);
    std::printf ("  one repeat against the next, at repeat 40: correlation %.2f\n", overlap);
    check ("repeats no longer look like each other", std::abs (overlap) < 0.5);
}

void testDamperShelf ()
{
    const size_t delay = samplesFor (200.0);

    // Compares the second repeat with the first, which is exactly one trip through the damper.
    const auto cutAt = [&] (double hz, double damper) {
        Plug plug;
        setWetOnly (plug, 200.0, 100.0, 0.0, damper);
        const size_t length = delay - 200;
        const auto output = plug.process (withSilence (makeTones ({ { hz, 0.25 } }, length), delay * 3));
        const auto first = toneLevel (slice (output.left, delay + 100, length - 200), hz);
        const auto second = toneLevel (slice (output.left, 2 * delay + 100, length - 200), hz);
        return db (second / first);
    };

    for (const double hz : { 200.0, 2000.0, 4000.0, 8000.0, 16000.0 })
        std::printf ("  at %5.0f Hz: %5.1f dB per trip with 12 dB of damper, %5.1f dB with none\n", hz, cutAt (hz, 12.0), cutAt (hz, 0.0));

    check ("with no damper, the top end isn't touched", std::abs (cutAt (16000.0, 0.0)) < 0.01);
    check ("the lows aren't touched", std::abs (cutAt (200.0, 12.0)) < 0.1);
    check ("the shelf is on its way down at 4 kHz", cutAt (4000.0, 12.0) < -1.5 && cutAt (4000.0, 12.0) > -6.0);
    check ("it's nearly at its depth at 16 kHz", cutAt (16000.0, 12.0) < -9.0 && cutAt (16000.0, 12.0) > -12.5);
    check ("more damper cuts more", cutAt (16000.0, 18.0) < cutAt (16000.0, 6.0) - 6.0);
}

void testDamperFirstRepeatClean ()
{
    Plug plug;
    setWetOnly (plug, 100.0, 50.0, 0.0, 18.0);

    const size_t delay = samplesFor (100.0);
    const auto noise = makeNoise (0.5, kBurst);
    const auto output = plug.process (withSilence (noise, delay + kBurst + 1));

    double worst = 0.0;
    for (size_t i = 0; i < kBurst; ++i)
        worst = std::max (worst, static_cast<double> (std::abs (output.left[delay + i] - noise[i])));
    check ("the damper is only in the feedback, so the first repeat is unchanged", worst < 1e-6);
}

void testDamperEndless ()
{
    // With a damper, the highs die away and the lows go on for ever.
    Plug plug;
    setWetOnly (plug, 100.0, 100.0, 50.0, 18.0);
    const size_t delay = samplesFor (100.0);
    const auto tone = makeTones ({ { 200.0, 0.5 }, { 12000.0, 0.5 } }, kBurst * 4);
    const auto output = plug.process (withSilence (tone, delay * 200));

    const auto levelNear = [&] (double hz, size_t repeat) {
        return toneLevel (slice (output.left, delay * repeat, delay * 10), hz);
    };

    const double lowChange = db (levelNear (200.0, 190) / levelNear (200.0, 2));
    const double highChange = db (levelNear (12000.0, 190) / levelNear (12000.0, 2));
    std::printf ("  over 19 s: 200 Hz changes by %.1f dB and 12 kHz by %.1f dB\n", lowChange, highChange);
    check ("the highs are gone", highChange < -40.0);
    check ("well before the lows", lowChange > -10.0);
}

namespace {

// Where an impulse's repeat lands, on a clean echo.
size_t measureRepeat (Plug& plug, size_t expected)
{
    const auto output = plug.process (withSilence (makeImpulse (1), expected + 100));
    return firstAbove (output.left, 0.5);
}

void setSynced (Plug& plug, double division)
{
    setWetOnly (plug, 100.0, 0.0, 0.0);
    plug.set (kSync, 1.0);
    plug.set (kDivision, division);
}

} // namespace

void testSyncTime ()
{
    // Positions in the list are saved in projects, so these are written out by hand. At 120 BPM, a quarter
    // note is 24000 samples at 48 kHz.
    const struct
    {
        int division;
        const char* name;
        size_t samples;
    } kCases[] = {
        { 0, "1/32", 3000 }, { 1, "1/16T", 4000 }, { 2, "1/32D", 4500 }, { 3, "1/16", 6000 }, { 4, "1/8T", 8000 },
        { 5, "1/16D", 9000 }, { 6, "1/8", 12000 }, { 7, "1/4T", 16000 }, { 8, "1/8D", 18000 }, { 9, "1/4", 24000 },
        { 10, "1/2T", 32000 }, { 11, "1/4D", 36000 }, { 12, "1/2", 48000 }, { 13, "1/1T", 64000 }, { 14, "1/2D", 72000 },
        { 15, "1/1", 96000 }, { 16, "1/1D", 144000 }, { 17, "2/1", 192000 }, { 18, "4/1", 384000 },
    };

    for (const auto& testCase : kCases)
    {
        Plug plug;
        plug.setTempo (120.0);
        setSynced (plug, testCase.division);

        const std::string name = paramText (plug.controller, kDivision, plug.controller->plainParamToNormalized (kDivision, testCase.division));

        const size_t found = measureRepeat (plug, testCase.samples);
        std::printf ("  %-5s expected at %zu, found at %zu\n", name.c_str (), testCase.samples, found);
        check ("the repeat lands on the note, and the list is in the same order", found == testCase.samples && name == testCase.name);
    }
}

void testSyncFollowsTempo ()
{
    Plug plug;
    plug.setTempo (120.0);
    setSynced (plug, 9.0);
    check ("a quarter note at 120 BPM", measureRepeat (plug, 24000) == 24000);

    plug.setTempo (60.0);
    plug.process (Samples (samplesFor (2000.0), 0.0f));
    check ("a change of tempo moves the delay", measureRepeat (plug, 48000) == 48000);

    // A tempo that isn't a whole number of samples per beat rounds to the nearest one.
    plug.setTempo (133.0);
    plug.process (Samples (samplesFor (2000.0), 0.0f));
    const size_t found = measureRepeat (plug, 21654);
    std::printf ("  at 133 BPM: %zu\n", found);
    check ("and keeps it on the beat", found == 21654);
}

void testSyncOffIgnoresTempo ()
{
    Plug plug;
    plug.setTempo (60.0);
    setWetOnly (plug, 100.0, 0.0, 0.0);
    plug.set (kDivision, 9.0);
    check ("with Sync off, Time decides", measureRepeat (plug, samplesFor (100.0)) == samplesFor (100.0));

    plug.set (kSync, 1.0);
    plug.process (Samples (samplesFor (2000.0), 0.0f));
    check ("with Sync on, the division does", measureRepeat (plug, 48000) == 48000);

    plug.set (kSync, 0.0);
    plug.process (Samples (samplesFor (2000.0), 0.0f));
    check ("and turning it off goes back to Time", measureRepeat (plug, samplesFor (100.0)) == samplesFor (100.0));
}

void testSyncWithoutTempo ()
{
    Plug plug;
    setSynced (plug, 9.0);
    check ("a host that gives no tempo gets 120 BPM", measureRepeat (plug, 24000) == 24000);

    plug.setTempo (90.0);
    plug.process (Samples (samplesFor (2000.0), 0.0f));
    check ("a host's tempo applies", measureRepeat (plug, 32000) == 32000);

    plug.setTempo (0.0);
    plug.process (Samples (samplesFor (2000.0), 0.0f));
    check ("and it's kept if the host stops giving one", measureRepeat (plug, 32000) == 32000);
}

void testSyncStaysInRange ()
{
    {
        Plug plug;
        plug.setTempo (30.0);
        setSynced (plug, 18.0);
        const auto output = plug.process (withSilence (makeImpulse (1), samplesFor (10000.0) + 100));
        check ("a division too long for the lines stops at 10 s", firstAbove (output.left, 0.5) == samplesFor (10000.0));
        check ("and stays finite", isFinite (output.left));
    }
    {
        Plug plug;
        plug.setTempo (999.0);
        setSynced (plug, 0.0);
        check ("one too short stops at 10 ms", measureRepeat (plug, samplesFor (10.0)) == samplesFor (10.0));
    }
}

void testTimeGlide ()
{
    Plug plug;
    setWetOnly (plug, 100.0, 80.0, 50.0);
    const auto tone = makeTones ({ { 440.0, 0.3 } }, samplesFor (2000.0));
    plug.process (tone);

    // Big jumps, with signal in the lines.
    plug.set (kTime, 900.0);
    const auto rising = plug.process (tone);
    plug.set (kTime, 12.0);
    const auto falling = plug.process (tone);
    plug.set (kTime, 2000.0);
    const auto top = plug.process (tone);

    check ("it stays finite", isFinite (rising.left) && isFinite (falling.left) && isFinite (top.left));
    check ("it doesn't get far louder", peak (rising.left) < 4.0 && peak (falling.left) < 4.0 && peak (top.left) < 4.0);

    double worstStep = 0.0;
    for (size_t i = 1; i < falling.left.size (); ++i)
        worstStep = std::max (worstStep, static_cast<double> (std::abs (falling.left[i] - falling.left[i - 1])));
    std::printf ("  worst sample to sample step, gliding from 900 to 12 ms: %.3f\n", worstStep);
    check ("a change of time glides, without clicks", worstStep < 0.8);

    // Everything moving at once, at 100% feedback, which is when a lossless loop has the least to save it. Each
    // change lands while the last is still gliding.
    {
        Plug moving;
        setWetOnly (moving, 100.0, 100.0, 100.0);
        moving.process (makeNoise (0.5, samplesFor (500.0)));

        const Samples silence (samplesFor (50.0), 0.0f);
        double loudest = 0.0;
        bool finite = true;
        double phase = 0.0;
        for (int step = 0; step < 600; ++step)
        {
            phase += 0.37;
            moving.set (kTime, 150.0 + 130.0 * std::sin (phase));
            moving.set (kDiffusion, 50.0 + 50.0 * std::sin (phase * 1.7));
            const auto part = moving.process (silence);
            loudest = std::max ({ loudest, peak (part.left), peak (part.right) });
            finite = finite && isFinite (part.left) && isFinite (part.right);
        }
        std::printf ("  30 s of moving Time and Diffusion at 100%% feedback peaks at %.2f\n", loudest);
        check ("moving the controls can't make the loop run away", finite && loudest < 2.0);
    }

    // It has to end up exactly in tune, on whole samples, for the repeats to stay unchanged again.
    Plug settled;
    setWetOnly (settled, 100.0, 0.0, 0.0);
    settled.process (Samples (samplesFor (100.0), 0.0f));
    settled.set (kTime, 200.0);
    settled.process (Samples (samplesFor (2000.0), 0.0f));
    const auto output = settled.process (withSilence (makeImpulse (1), samplesFor (200.0) + 10));
    check ("after gliding, the time is exact", firstAbove (output.left, 0.99) == samplesFor (200.0));
}

void testHeldNoteStaysBounded ()
{
    for (const double diffusion : { 0.0, 100.0 })
    {
        Plug plug;
        setWetOnly (plug, 100.0, 100.0, diffusion);

        // Exactly in step with the loop, so that every pass adds to the last: the worst case.
        const auto tone = makeTones ({ { 100.0, 0.8 } }, samplesFor (10000.0));
        const auto output = plug.process (tone);
        std::printf ("  a full-scale note held for 10 s at %.0f%% diffusion peaks at %.2f\n", diffusion, peak (output.left));
        check ("the limiter holds it back", peak (output.left) < 3.0 && isFinite (output.left));
    }

    // And it's clean at ordinary levels: a single loud hit isn't touched by it.
    Plug plug;
    setWetOnly (plug, 100.0, 100.0, 0.0);
    const size_t delay = samplesFor (100.0);
    const auto noise = makeNoise (0.95, kBurst);
    const auto output = plug.process (withSilence (noise, delay * 4));
    double worst = 0.0;
    for (size_t i = 0; i < kBurst; ++i)
        worst = std::max (worst, static_cast<double> (std::abs (output.left[3 * delay + i] - noise[i])));
    std::printf ("  a hit near full scale changes by %.1f dB in three passes\n", db (worst));
    check ("a single hit is below the limiter", worst < 1e-2);
}

void testDcOffset ()
{
    Plug plug;
    setWetOnly (plug, 20.0, 100.0, 0.0);

    const Samples offset (samplesFor (10000.0), 0.3f);
    const auto output = plug.process (offset);
    const auto settled = slice (output.left, output.left.size () - samplesFor (200.0), samplesFor (200.0));
    double mean = 0.0;
    for (const float sample : settled)
        mean += sample;
    mean /= static_cast<double> (settled.size ());
    std::printf ("  a steady offset of 0.3, held for 10 s, comes out as %.3f\n", mean);
    check ("an offset doesn't fill up the loop", std::abs (mean) < 0.8);
}

void testBrokenInput ()
{
    Plug plug;
    setWetOnly (plug, 50.0, 100.0, 100.0);

    auto input = makeNoise (0.2, samplesFor (200.0));
    input[100] = std::numeric_limits<float>::quiet_NaN ();
    input[200] = std::numeric_limits<float>::infinity ();
    const auto output = plug.process (input);
    check ("a broken sample doesn't get into the loop", isFinite (output.left) && isFinite (output.right));
    const auto later = plug.process (makeNoise (0.2, samplesFor (200.0)));
    check ("and it carries on working", isFinite (later.left) && rms (later.left) > 0.001);
}

void testExtremeSettings ()
{
    const auto noise = makeNoise (0.9, samplesFor (3000.0));
    for (const double timeMs : { 10.0, 10000.0 })
    {
        for (const double feedback : { 0.0, 100.0 })
        {
            for (const double diffusion : { 0.0, 100.0 })
            {
                for (const double damper : { 0.0, 18.0 })
                {
                    Plug plug;
                    setWetOnly (plug, timeMs, feedback, diffusion, damper);
                    plug.set (kVolume, 6.0);
                    const auto output = plug.process (noise);
                    if (!isFinite (output.left) || !isFinite (output.right) || peak (output.left) > 12.0)
                    {
                        std::printf ("  %.0f ms, %.0f%% feedback, %.0f%% diffusion, %.0f dB damper: peak %.2f\n", timeMs, feedback, diffusion, damper, peak (output.left));
                        check ("every combination stays finite and bounded", false);
                        return;
                    }
                }
            }
        }
    }
    check ("every combination stays finite and bounded", true);
}

void testSampleRates ()
{
    for (const double sampleRate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Plug plug (sampleRate);
        setWetOnly (plug, 100.0, 100.0, 100.0, 6.0);
        const size_t delay = samplesFor (100.0, sampleRate);
        const auto output = plug.process (withSilence (makeImpulse (1), delay * 30));
        check ("it runs at this sample rate", isFinite (output.left) && peak (output.left) > 0.0);

        Plug clean (sampleRate);
        setWetOnly (clean, 100.0, 0.0, 0.0);
        const auto repeat = clean.process (withSilence (makeImpulse (1), delay + 10));
        std::printf ("  at %.0f Hz, a 100 ms repeat lands on sample %zu of an expected %zu\n", sampleRate, firstAbove (repeat.left, 0.5), delay);
        check ("and keeps its time", firstAbove (repeat.left, 0.5) == delay);
    }
}

void testDryWet ()
{
    const size_t delay = samplesFor (100.0);
    const auto noise = withSilence (makeNoise (0.5, kBurst), delay * 2);

    const auto levels = [&] (double dryWet) {
        Plug plug;
        setWetOnly (plug, 100.0, 0.0, 0.0);
        plug.set (kDryWet, dryWet);
        const auto output = plug.process (noise);
        return std::pair<double, double> (rms (slice (output.left, 0, kBurst)), rms (slice (output.left, delay, kBurst)));
    };

    const double input = rms (slice (noise, 0, kBurst));
    const auto dry = levels (0.0);
    const auto even = levels (50.0);
    const auto wet = levels (100.0);
    check ("at 0%, only the dry signal", std::abs (dry.first - input) < 1e-4 && dry.second < 1e-6);
    check ("at 50%, both at full level", std::abs (even.first - input) < 1e-4 && std::abs (even.second - input) < 1e-4);
    check ("at 100%, only the repeat", wet.first < 1e-6 && std::abs (wet.second - input) < 1e-4);
}

void testVolume ()
{
    const auto noise = makeNoise (0.25, samplesFor (200.0));
    const auto levelAt = [&] (double volume) {
        Plug plug;
        plug.set (kDryWet, 0.0);
        plug.set (kVolume, volume);
        return rms (plug.process (noise).left);
    };

    const double unity = levelAt (0.0);
    check ("+6 dB", std::abs (db (levelAt (6.0) / unity) - 6.0) < 0.01);
    check ("-12 dB", std::abs (db (levelAt (-12.0) / unity) + 12.0) < 0.01);
    check ("the bottom of the range is silent", levelAt (-60.0) == 0.0);
}

void testBlockSizes ()
{
    const auto noise = withSilence (makeNoise (0.5, 4800), samplesFor (3000.0));

    const auto render = [&] (int blockSize) {
        Plug plug (kSampleRate, 4096);
        setWetOnly (plug, 123.0, 85.0, 70.0, 6.0);
        plug.set (kDryWet, 60.0);
        return plug.process (noise, blockSize);
    };

    const auto reference = render (256);
    for (const int blockSize : { 1, 17, 64, 512, 4096 })
    {
        const auto output = render (blockSize);
        double worst = 0.0;
        for (size_t i = 0; i < reference.left.size (); ++i)
        {
            worst = std::max (worst, static_cast<double> (std::abs (output.left[i] - reference.left[i])));
            worst = std::max (worst, static_cast<double> (std::abs (output.right[i] - reference.right[i])));
        }
        std::printf ("  at %d: worst difference %.2g\n", blockSize, worst);
        check ("the output doesn't depend on the block size", worst == 0.0);
    }
}

void testDeterminism ()
{
    const auto noise = makeNoise (0.5, samplesFor (1000.0));
    const auto render = [&] {
        Plug plug;
        setWetOnly (plug, 200.0, 95.0, 100.0, 3.0);
        return plug.process (noise);
    };

    const auto first = render ();
    const auto second = render ();
    check ("the same input renders the same", first.left == second.left && first.right == second.right);
}

void testSilenceFlags ()
{
    {
        Plug plug;
        setWetOnly (plug, 50.0, 50.0, 50.0);
        const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), samplesFor (30000.0)));
        check ("while it still sounds, it isn't flagged silent", !output.blockSilent[1]);
        check ("once the tail has gone, it is", output.blockSilent.back ());

        const auto again = plug.process (makeNoise (0.5, samplesFor (100.0)));
        check ("and it wakes when something arrives", rms (again.left) > 0.001 && !again.blockSilent.back ());
    }
    {
        Plug plug;
        setWetOnly (plug, 50.0, 100.0, 100.0);
        const auto output = plug.process (withSilence (makeNoise (0.5, kBurst), samplesFor (20000.0)));
        check ("at 100% feedback, it never goes silent", !output.blockSilent.back ());
    }
}

void testParameterState ()
{
    Plug source;
    source.set (kTime, 250.0);
    source.set (kFeedback, 77.0);
    source.set (kDiffusion, 66.0);
    source.set (kDamper, 12.0);
    source.set (kDryWet, 40.0);
    source.set (kVolume, -3.0);
    source.process (Samples (kBlockSize, 0.0f));
    const auto bytes = source.getStateBytes ();

    Plug target;
    target.setStateBytes (bytes);
    check ("the processor's state comes back the same", target.getStateBytes () == bytes);

    const auto plainValue = [&] (ParamId id) {
        return target.controller->normalizedParamToPlain (id, target.controller->getParamNormalized (id));
    };
    check ("Time reaches the controller", std::abs (plainValue (kTime) - 250.0) < 0.01);
    check ("Damper reaches the controller", std::abs (plainValue (kDamper) - 12.0) < 0.01);
}

void testParameterText ()
{
    Plug plug;

    const auto text = [&] (ParamId id, double plain) {
        return paramText (plug.controller, id, plug.controller->plainParamToNormalized (id, plain));
    };

    check ("Time", text (kTime, 375.0) == "375");
    check ("Feedback", text (kFeedback, 100.0) == "100");
    check ("Damper", text (kDamper, 4.5) == "4.5");
    check ("Volume at the bottom", text (kVolume, -60.0) == "-inf");
    check ("Sync", text (kSync, 0.0) == "Off" && text (kSync, 1.0) == "On");
    check ("Division", text (kDivision, 8.0) == "1/8D" && text (kDivision, 18.0) == "4/1");

    double listValue = 0.0;
    check ("a typed division is understood", paramFromText (plug.controller, kDivision, "1/4T", listValue));
    check ("and lands where it should", plug.controller->normalizedParamToPlain (kDivision, listValue) == 7.0);

    double value = 0.0;
    check ("a typed value is understood", paramFromText (plug.controller, kTime, "500", value));
    check ("and lands where it should", std::abs (plug.controller->normalizedParamToPlain (kTime, value) - 500.0) < 0.01);
}

void measureCpu ()
{
    Plug plug;
    setWetOnly (plug, 500.0, 100.0, 100.0, 3.0);
    const auto noise = makeNoise (0.2, samplesFor (10000.0));

    const auto start = std::chrono::steady_clock::now ();
    plug.process (noise);
    const std::chrono::duration<double> taken = std::chrono::steady_clock::now () - start;
    std::printf ("  %.2f s of work for 10 s of audio at 48 kHz: %.2f%% of a core\n", taken.count (), taken.count () / 10.0 * 100.0);
}

} // namespace Tests
} // namespace DiffusionDelay
