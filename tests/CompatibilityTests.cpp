// These protect projects people have saved. The expected values are written out here on purpose, rather
// than taken from the plug-in's own definitions, so that changing those definitions fails a test.

#include "Harness.h"
#include "Tests.h"

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ustring.h"
#include "public.sdk/source/common/memorystream.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace Steinberg;

namespace DiffusionDelay {
namespace Tests {

namespace {

// Hosts save these numbers, so they can never change. New parameters get new numbers.
static_assert (kTime == 0 && kFeedback == 1 && kDiffusion == 2 && kDamper == 3, "Parameter IDs are saved in projects");
static_assert (kDryWet == 4 && kVolume == 5 && kSync == 6 && kDivision == 7, "Parameter IDs are saved in projects");

template <typename T>
T readAt (const std::vector<char>& bytes, size_t offset)
{
    T value {};
    std::memcpy (&value, bytes.data () + offset, sizeof (value));
    return value;
}

bool nearly (double a, double b)
{
    return std::abs (a - b) < 1e-6 * std::max (1.0, std::abs (b));
}

} // namespace

void testClassIds ()
{
    bool foundProcessor = false;
    bool foundController = false;
    const FUID processor (0x75D5B919, 0x9B1B4F84, 0xAA4EA7BA, 0xF77997FD);
    const FUID controller (0x29D38EDE, 0x8A5A47D6, 0xAC09247F, 0x94AEB704);

    auto* factory = getFactory ();
    for (int32 i = 0; i < factory->countClasses (); ++i)
    {
        PClassInfo info;
        factory->getClassInfo (i, &info);
        foundProcessor = foundProcessor || FUID::fromTUID (info.cid) == processor;
        foundController = foundController || FUID::fromTUID (info.cid) == controller;
    }
    check ("the processor's class ID hasn't changed", foundProcessor);
    check ("the controller's class ID hasn't changed", foundController);
}

void testParameterIds ()
{
    Plug plug;
    check ("there are 8 parameters", plug.controller->getParameterCount () == 8);

    bool allMatch = true;
    for (int32 i = 0; i < plug.controller->getParameterCount (); ++i)
    {
        Vst::ParameterInfo info {};
        plug.controller->getParameterInfo (i, info);
        allMatch = allMatch && info.id == static_cast<Vst::ParamID> (i)
                   && (info.flags & Vst::ParameterInfo::kCanAutomate) != 0;
    }
    check ("IDs follow the enum, and all are automatable", allMatch);
}

void testParameterRanges ()
{
    struct Range
    {
        ParamId id;
        double min;
        double max;
    };
    const Range kRanges[] = {
        { kTime, 10, 10000 }, { kFeedback, 0, 100 }, { kDiffusion, 0, 100 }, { kDamper, 0, 18 },
        { kDryWet, 0, 100 }, { kVolume, -60, 6 }, { kSync, 0, 1 }, { kDivision, 0, 18 },
    };

    Plug plug;
    bool allMatch = true;
    for (const auto& range : kRanges)
    {
        allMatch = allMatch && nearly (plug.controller->normalizedParamToPlain (range.id, 0.0), range.min)
                   && nearly (plug.controller->normalizedParamToPlain (range.id, 1.0), range.max);
    }
    check ("every range is as it was", allMatch);

    check ("time is logarithmic", std::abs (plug.controller->normalizedParamToPlain (kTime, 0.5) - 316.23) < 0.01);
    check ("sync starts off", plug.controller->normalizedParamToPlain (kSync, plug.controller->getParamNormalized (kSync)) == 0.0);
    check ("the division starts at a dotted eighth", plug.controller->normalizedParamToPlain (kDivision, plug.controller->getParamNormalized (kDivision)) == 8.0);
    check ("feedback is linear", nearly (plug.controller->normalizedParamToPlain (kFeedback, 0.5), 50.0));
    check ("diffusion is linear", nearly (plug.controller->normalizedParamToPlain (kDiffusion, 0.5), 50.0));
    check ("the damper is linear in dB", nearly (plug.controller->normalizedParamToPlain (kDamper, 0.5), 9.0));
}

void testSavedFormat ()
{
    Plug plug;
    const auto bytes = plug.getStateBytes ();
    check ("version, count, then 8 ID and value pairs", bytes.size () == 8 + 8 * 12);
    check ("version 1", readAt<int32_t> (bytes, 0) == 1);
    check ("the count is 8", readAt<int32_t> (bytes, 4) == 8);
    check ("the second pair is Feedback, at its default", readAt<int32_t> (bytes, 20) == 1 && std::abs (readAt<double> (bytes, 24) - 0.5) < 1e-9);

    MemoryStream empty;
    check ("an empty state is rejected without crashing", plug.component->setState (&empty) != kResultOk);

    // A state from a future version with parameters unknown to this one still loads what it knows.
    MemoryStream stream;
    const int32_t header[2] = { 1, 2 };
    int32 written = 0;
    stream.write (const_cast<int32_t*> (header), sizeof (header), &written);
    const int32_t unknownId = 999;
    const double value = 0.25;
    const int32_t timeId = 0;
    stream.write (const_cast<int32_t*> (&unknownId), 4, &written);
    stream.write (const_cast<double*> (&value), 8, &written);
    stream.write (const_cast<int32_t*> (&timeId), 4, &written);
    stream.write (const_cast<double*> (&value), 8, &written);
    int64 position = 0;
    stream.seek (0, IBStream::kIBSeekSet, &position);
    check ("an unknown parameter is skipped", plug.component->setState (&stream) == kResultOk);
    check ("and the known one still arrives", nearly (readAt<double> (plug.getStateBytes (), 12), 0.25));
}

void testBuses ()
{
    Plug plug;
    check ("one audio input", plug.component->getBusCount (Vst::kAudio, Vst::kInput) == 1);
    check ("one audio output", plug.component->getBusCount (Vst::kAudio, Vst::kOutput) == 1);
    check ("no event inputs", plug.component->getBusCount (Vst::kEvent, Vst::kInput) == 0);

    Vst::SpeakerArrangement arrangement = 0;
    plug.processor->getBusArrangement (Vst::kInput, 0, arrangement);
    check ("the input is stereo", arrangement == Vst::SpeakerArr::kStereo);
    plug.processor->getBusArrangement (Vst::kOutput, 0, arrangement);
    check ("the output is stereo", arrangement == Vst::SpeakerArr::kStereo);

    FUnknownPtr<IPluginFactory2> factory (getFactory ());
    bool isEffect = false;
    for (int32 i = 0; factory && i < factory->countClasses (); ++i)
    {
        PClassInfo2 info;
        factory->getClassInfo2 (i, &info);
        if (std::strcmp (info.category, kVstAudioEffectClass) == 0)
            isEffect = std::strcmp (info.subCategories, "Fx") == 0;
    }
    check ("it's registered as an effect", isEffect);
}

void testLatencyReport ()
{
    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Plug plug (sampleRate);
        check ("there's no latency", plug.processor->getLatencySamples () == 0);
    }

    Plug plug;
    check ("the tail is reported as infinite, and the silence flags say when it ends", plug.processor->getTailSamples () == Vst::kInfiniteTail);
}

} // namespace Tests
} // namespace DiffusionDelay
