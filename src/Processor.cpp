#include "Processor.h"

#include "Controller.h"

#include "hadronyche/dsp/Gain.h"

#include "pluginterfaces/vst/ivstprocesscontext.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;

namespace DiffusionDelay {

namespace {

constexpr double kSmoothingMs = 20.0;

// How long a change of Time takes to settle, as a time constant. The lines glide there like a tape, which
// bends the pitch of what's in them.
constexpr double kTimeGlideMs = 100.0;

// How often what's costly to calculate is updated, in samples.
constexpr int kControlInterval = 16;

// Below this, there's nothing to hear. It's about -140 dB.
constexpr float kSilentPeak = 1e-7f;

} // namespace

Processor::Processor ()
: StereoProcessor (Controller::getClassFUID (), true, getParamSpec, readParamState, writeParamState)
{
    processContextRequirements.needTempo ();
}

uint32 PLUGIN_API Processor::getLatencySamples ()
{
    return 0;
}

// At 100% feedback, it never ends. The silence flags say when it has.
uint32 PLUGIN_API Processor::getTailSamples ()
{
    return Vst::kInfiniteTail;
}

double Processor::plain (ParamId id) const
{
    return _params.plain (id);
}

double Processor::getTimeMs () const
{
    if (plain (kSync) < 0.5)
        return plain (kTime);

    // A slow tempo can put the longest divisions past the end of the delay lines.
    const double beats = getDivisionBeats (static_cast<int> (plain (kDivision)));
    return std::clamp (beats * 60000.0 / _tempo, kMinTimeMs, kMaxTimeMs);
}

void Processor::applyParameters ()
{
    const int timeSamples = static_cast<int> (std::lround (getTimeMs () / 1000.0 * processSetup.sampleRate));
    if (timeSamples != _timeSamples)
    {
        _timeSamples = timeSamples;
        _loop.setTime (timeSamples);
    }

    _diffusion.setTarget (plain (kDiffusion) / 100.0);
    _damperGain.setTarget (Hadronyche::dbToGain (-plain (kDamper)));
    _feedback.setTarget (plain (kFeedback) / 100.0);
    _dryGain.setTarget (Hadronyche::dryGain (plain (kDryWet)));
    _wetGain.setTarget (Hadronyche::wetGain (plain (kDryWet)));
    _volume.setTarget (Hadronyche::volumeToGain (plain (kVolume)));
}

// Moves everything straight to its current target, for when nobody can hear it moving.
void Processor::snapSmoothers ()
{
    for (auto* smoother : { &_diffusion, &_damperGain, &_feedback, &_dryGain, &_wetGain, &_volume })
        smoother->snapToTarget ();
    _loop.snapToTime ();
}

void Processor::prepareDsp ()
{
    const double sampleRate = processSetup.sampleRate;

    _loop.prepare (sampleRate, kMaxTimeMs / 1000.0);
    _loop.setGlideCoefficient (std::exp (-1000.0 / (sampleRate * kTimeGlideMs)));
    _timeSamples = 0;

    const double ticksPerSecond = sampleRate / kControlInterval;
    for (auto* smoother : { &_diffusion, &_damperGain })
        smoother->prepare (ticksPerSecond, kSmoothingMs);
    for (auto* smoother : { &_feedback, &_dryGain, &_wetGain, &_volume })
        smoother->prepare (sampleRate, kSmoothingMs);

    _controlCountdown = 0;
    _silentSamples = 0;
    _isFirstBlock = true;

    applyParameters ();
    snapSmoothers ();
    updateControls ();
}

void Processor::updateControls ()
{
    _loop.setDiffusion (_diffusion.next ());
    _loop.setDamperGain (_damperGain.next ());
}

void Processor::render (const float* inputLeft, const float* inputRight, float* outputLeft, float* outputRight, int32 count)
{
    for (int32 i = 0; i < count; ++i)
    {
        // A broken sample upstream would otherwise go round the loop for ever.
        const float left = inputLeft && std::isfinite (inputLeft[i]) ? inputLeft[i] : 0.0f;
        const float right = inputRight && std::isfinite (inputRight[i]) ? inputRight[i] : 0.0f;

        if (--_controlCountdown < 0)
        {
            _controlCountdown = kControlInterval - 1;
            updateControls ();
        }

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        _loop.process (left, right, static_cast<float> (_feedback.next ()), wetLeft, wetRight);

        const auto dryGain = static_cast<float> (_dryGain.next ());
        const auto wetGain = static_cast<float> (_wetGain.next ());
        const auto volume = static_cast<float> (_volume.next ());
        outputLeft[i] = (left * dryGain + wetLeft * wetGain) * volume;
        outputRight[i] = (right * dryGain + wetRight * wetGain) * volume;
    }
}

void Processor::renderBlock (Vst::ProcessData& data, Vst::AudioBusBuffers& output)
{
    if (data.processContext && (data.processContext->state & Vst::ProcessContext::kTempoValid) != 0
        && std::isfinite (data.processContext->tempo) && data.processContext->tempo > 0.0)
        _tempo = data.processContext->tempo;

    applyParameters ();

    // Changes arriving with the first block are where the plug-in starts, not something to ramp to.
    if (_isFirstBlock)
    {
        snapSmoothers ();
        _isFirstBlock = false;
    }

    const float* inputLeft = nullptr;
    const float* inputRight = nullptr;
    if (data.numInputs > 0 && data.inputs[0].numChannels == 2)
    {
        inputLeft = data.inputs[0].channelBuffers32[0];
        inputRight = data.inputs[0].channelBuffers32[1];
    }

    // Looks at the samples rather than the host's silence flags, which are only ever a hint.
    float inputPeak = 0.0f;
    if (inputLeft)
    {
        for (int32 i = 0; i < data.numSamples; ++i)
            inputPeak = std::max ({ inputPeak, std::abs (inputLeft[i]), std::abs (inputRight[i]) });
    }

    // Quiet for as long as anything can take to get through, and there's nothing left to come out.
    const bool isAsleep = inputPeak < kSilentPeak && _silentSamples >= _loop.getLongestPathSamples () + data.numSamples;
    if (isAsleep)
    {
        _controlCountdown = (_controlCountdown + kControlInterval - data.numSamples % kControlInterval) % kControlInterval;

        // Nobody can hear parameters move while asleep, and sound shouldn't start with them still ramping.
        snapSmoothers ();
        updateControls ();

        std::memset (output.channelBuffers32[0], 0, sizeof (float) * static_cast<size_t> (data.numSamples));
        std::memset (output.channelBuffers32[1], 0, sizeof (float) * static_cast<size_t> (data.numSamples));
        output.silenceFlags = 3;
        return;
    }

    render (inputLeft, inputRight, output.channelBuffers32[0], output.channelBuffers32[1], data.numSamples);
    output.silenceFlags = 0;

    const bool isQuiet = inputPeak < kSilentPeak && _loop.takePeak () < kSilentPeak;
    _silentSamples = isQuiet ? _silentSamples + data.numSamples : 0;
}

} // namespace DiffusionDelay
