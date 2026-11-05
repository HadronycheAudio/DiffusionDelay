#pragma once

#include "DelayLine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace DiffusionDelay {

// The echo and its feedback path, as described in docs/signal-path.md. Eight parallel channels each pass through
// four delay stages that add up to exactly the echo time. Between the stages, a rotation moves signal from each
// channel to the others. A signal that changes channel takes a different total time: some paths arrive before
// the echo time, which are the early reflections, and some after it.
//
// Every part is lossless, whatever the Diffusion setting: delays and rotations can only move energy around. That
// is what lets the repeats go on for ever at 100% feedback while they blur, instead of ringing or running away,
// so a change that breaks it (a gain, a filter, an unequal path) would show up in the `endless` tests.
class DiffusionLoop
{
public:
    static constexpr size_t kChannels = 8;
    static constexpr size_t kStages = 4;

    // Not for the audio thread.
    void prepare (double sampleRate, double maxTimeSeconds)
    {
        const double maxTimeSamples = std::ceil (sampleRate * maxTimeSeconds);
        for (size_t k = 0; k < kChannels; ++k)
        {
            for (size_t s = 0; s < kStages; ++s)
            {
                // The last stage takes up what the others' rounding left over, so a little extra room.
                _stages[s][k].prepare (static_cast<size_t> (maxTimeSamples * kStageShares[k][s] / 1000.0) + kStages);
            }
            _tails[k].prepare (static_cast<size_t> (maxTimeSamples * kTailShares[k] / 1000.0) + 1);
        }

        for (auto& filter : _dampers)
            filter = {};
        for (auto& filter : _dcBlockers)
            filter = {};

        // A one-pole low-pass with its corner at kDamperHz, by the bilinear transform, so it reaches exactly zero
        // at half the sample rate and the shelf reaches its full depth there.
        const double warped = std::tan (kPi * std::min (kDamperHz, 0.45 * sampleRate) / sampleRate);
        _damperFeedforward = static_cast<float> (warped / (1.0 + warped));
        _damperFeedback = static_cast<float> ((1.0 - warped) / (1.0 + warped));

        _dcPole = std::exp (-2.0 * kPi * kDcBlockHz / sampleRate);
        _dcGain = 0.5 * (1.0 + _dcPole);
    }

    // The time every straight path takes. Rounded so that the stages of each channel add up to exactly this.
    void setTime (int samples)
    {
        for (size_t k = 0; k < kChannels; ++k)
        {
            int remaining = samples;
            for (size_t s = 0; s + 1 < kStages; ++s)
            {
                const int stage = std::max (1, static_cast<int> (std::lround (samples * kStageShares[k][s] / 1000.0)));
                _stages[s][k].setTarget (stage);
                remaining -= stage;
            }
            _stages[kStages - 1][k].setTarget (std::max (1, remaining));
            _tails[k].setTarget (std::max (1, static_cast<int> (std::lround (samples * kTailShares[k] / 1000.0))));
        }
    }

    void snapToTime ()
    {
        forEachLine ([] (DelayLine& line) { line.snapToTarget (); });
    }

    void setGlideCoefficient (double coefficient) { _glideCoefficient = coefficient; }

    // The longest one trip through the stages and a tail can take. What goes round a tail again is watched directly.
    int getLongestPathSamples () const
    {
        int longest = 0;
        for (const auto& stage : _stages)
        {
            int stageLongest = 0;
            for (const auto& line : stage)
                stageLongest = std::max (stageLongest, line.getTarget ());
            longest += stageLongest;
        }

        int tailLongest = 0;
        for (const auto& line : _tails)
            tailLongest = std::max (tailLongest, line.getTarget ());
        return longest + tailLongest;
    }

    // Amount is 0 to 1. Costs two sines and cosines, so this isn't for every sample.
    void setDiffusion (double amount)
    {
        const double scatterAngle = amount * kMaxScatterRadians;
        _scatterStraight = static_cast<float> (std::cos (scatterAngle));
        _scatterAcross = static_cast<float> (std::sin (scatterAngle) / std::sqrt (7.0));

        const double tailAngle = amount * kMaxTailRadians;
        _tailStraight = static_cast<float> (std::cos (tailAngle));
        _tailAcross = static_cast<float> (std::sin (tailAngle));

        // With the tails disconnected, whatever is in them could never be heard or leave, and would come back
        // long afterwards when Diffusion was next raised. So it drains away. This can't be heard.
        _tailKeep = amount > 0.0 ? 1.0f : 0.999f;

        // The output adds up four channels. A clean repeat is the same on all four, so they add to full level, but
        // sound that has scattered is on one at a time and comes out 6 dB lower, though its energy is all still in
        // the loop. This makes up for most of that, outside the loop, so that the sound doesn't drop as it dissolves.
        _outputGain = static_cast<float> (0.5 * std::pow (10.0, amount * kOutputMakeupDb / 20.0));
    }

    // The shelf's gain above kDamperHz, on each pass. At 1, it changes nothing.
    void setDamperGain (double gain) { _damperGain = static_cast<float> (gain); }

    void process (float inputLeft, float inputRight, float feedback, float& outputLeft, float& outputRight)
    {
        forEachLine ([this] (DelayLine& line) { line.glide (_glideCoefficient); });

        std::array<float, kChannels> signal;
        for (size_t k = 0; k < kChannels; ++k)
            signal[k] = _stages[0][k].read ();

        // A tail comes first so that later rotations spread it across the channels, and so across the stereo image.
        float peak = _peak;
        for (size_t k = 0; k < kChannels; ++k)
        {
            const float recirculating = _tails[k].read ();
            const float direct = signal[k];
            signal[k] = _tailStraight * direct + _tailAcross * recirculating;
            _tails[k].write ((_tailStraight * recirculating - _tailAcross * direct) * _tailKeep);
            peak = std::max (peak, std::abs (recirculating));
        }

        for (size_t s = 1; s < kStages; ++s)
        {
            scatter (signal);
            for (size_t k = 0; k < kChannels; ++k)
            {
                const float delayed = _stages[s][k].read ();
                _stages[s][k].write (signal[k]);
                signal[k] = delayed;
            }
        }

        float left = 0.0f;
        float right = 0.0f;
        for (size_t k = 0; k < kChannels / 2; ++k)
        {
            left += signal[k];
            right += signal[k + kChannels / 2];
        }
        outputLeft = _outputGain * left;
        outputRight = _outputGain * right;

        // Every line has been read by now, so what comes out can go back in on this same sample. That keeps a trip
        // round the loop at exactly the echo time, rather than a sample more, which would drift out of time.
        for (size_t k = 0; k < kChannels; ++k)
        {
            peak = std::max (peak, std::abs (signal[k]));

            // The damper only touches what goes round again, so the first repeat keeps all its top end.
            const float returning = softLimit (feedback * blockDc (_dcBlockers[k], damp (_dampers[k], signal[k])));

            // Half on each of four channels. Added back up on the way out, that's the same level as the input.
            const float input = 0.5f * (k < kChannels / 2 ? inputLeft : inputRight);
            _stages[0][k].write (input + returning);
        }
        _peak = peak;
    }

    // The loudest thing seen in the loop since the last call, for working out when it has gone quiet.
    float takePeak ()
    {
        const float peak = _peak;
        _peak = 0.0f;
        return peak;
    }

private:
    struct DamperState
    {
        float previousInput { 0.0f };
        float lowPass { 0.0f };
    };

    struct DcState
    {
        // Double precision, because the pole is so close to 1.
        double previousInput { 0.0 };
        double output { 0.0 };
    };

    static constexpr double kPi = 3.14159265358979323846;

    static constexpr double kDamperHz = 4000.0;

    // See softLimit for why it's here. It's so low because a repeat goes through it on every pass, and with a short
    // time, that's many times a second: at 3 Hz, a 40 Hz note at 100 ms fades by 14 dB a minute, and here about 1.6.
    static constexpr double kDcBlockHz = 1.0;

    // How far the rotations and the tails turn at 100%. Between them, the straight path is about 8 dB down on
    // each pass, leaving a clear repeat that dissolves over the next few. See docs/signal-path.md.
    static constexpr double kMaxScatterRadians = 35.0 * kPi / 180.0;
    static constexpr double kMaxTailRadians = 45.6 * kPi / 180.0;

    // At 100% Diffusion.
    static constexpr double kOutputMakeupDb = 3.0;

    // Parts per thousand of the echo time. Each row adds up to 1000, so that all the straight paths take exactly
    // the echo time. Within each column they're spread out, so that a change of channel changes the arrival time,
    // and they were chosen by a search to make all those times land apart.
    static constexpr int kStageShares[kChannels][kStages] = {
        { 143, 263, 263, 331 },
        { 173, 341, 355, 131 },
        { 305, 197, 163, 335 },
        { 203, 281, 169, 347 },
        { 201, 139, 307, 353 },
        { 331, 347, 181, 141 },
        { 271, 167, 365, 197 },
        { 363, 211, 243, 183 },
    };

    // Parts per thousand of the echo time, which are primes so that they don't share echoes.
    static constexpr int kTailShares[kChannels] = { 31, 37, 43, 53, 61, 71, 83, 97 };

    // A skew-symmetric conference matrix of order 8, from the Paley construction on the integers modulo 7. Its
    // rows are orthogonal and it's its own negative transpose, so that cos (a) I + sin (a) / sqrt (7) S is exactly
    // a rotation at every angle, and a = 0 leaves the channels alone.
    static constexpr int kScatter[kChannels][kChannels] = {
        { 0, 1, 1, 1, 1, 1, 1, 1 },
        { -1, 0, 1, 1, -1, 1, -1, -1 },
        { -1, -1, 0, 1, 1, -1, 1, -1 },
        { -1, -1, -1, 0, 1, 1, -1, 1 },
        { -1, 1, -1, -1, 0, 1, 1, -1 },
        { -1, -1, 1, -1, -1, 0, 1, 1 },
        { -1, 1, -1, 1, -1, -1, 0, 1 },
        { -1, 1, 1, -1, 1, -1, -1, 0 },
    };

    template <typename Function>
    void forEachLine (Function function)
    {
        for (auto& stage : _stages)
            for (auto& line : stage)
                function (line);
        for (auto& line : _tails)
            function (line);
    }

    void scatter (std::array<float, kChannels>& signal) const
    {
        std::array<float, kChannels> mixed;
        for (size_t j = 0; j < kChannels; ++j)
        {
            float across = 0.0f;
            for (size_t i = 0; i < kChannels; ++i)
                across += static_cast<float> (kScatter[j][i]) * signal[i];
            mixed[j] = _scatterStraight * signal[j] + _scatterAcross * across;
        }
        signal = mixed;
    }

    // A one-pole shelf: all the low-pass, plus the rest of the signal at the shelf's gain. It can never have a
    // gain above 1, and has exactly 1 at DC.
    float damp (DamperState& state, float input) const
    {
        state.lowPass = _damperFeedforward * (input + state.previousInput) + _damperFeedback * state.lowPass;
        state.previousInput = input;
        return state.lowPass + _damperGain * (input - state.lowPass);
    }

    // The gain makes this exactly 1 at the top of the range, rather than a little above, which would slowly run
    // away at 100% feedback.
    float blockDc (DcState& state, float input) const
    {
        state.output = _dcGain * (input - state.previousInput) + _dcPole * state.output;
        state.previousInput = input;
        return static_cast<float> (state.output);
    }

    // Holding a note into 100% feedback keeps adding to what's there, without limit. This holds that back, and
    // leaves everything quieter than a full-scale input untouched. Without the DC blocker, an offset in the input
    // would also build up to this ceiling and stay there, leaving no room for anything else.
    static float softLimit (float sample)
    {
        const float size = std::abs (sample);
        if (size <= kLimitKnee)
            return sample;

        const float limited = kLimitKnee + (kLimitCeiling - kLimitKnee) * std::tanh ((size - kLimitKnee) / (kLimitCeiling - kLimitKnee));
        return sample < 0.0f ? -limited : limited;
    }

    // Per channel. A full-scale input puts 0.5 on each of its channels, and adds back to full scale on the way out.
    static constexpr float kLimitKnee = 0.7f;
    static constexpr float kLimitCeiling = 1.0f;

    std::array<std::array<DelayLine, kChannels>, kStages> _stages;
    std::array<DelayLine, kChannels> _tails;
    std::array<DamperState, kChannels> _dampers {};
    std::array<DcState, kChannels> _dcBlockers {};

    double _glideCoefficient { 0.0 };

    float _scatterStraight { 1.0f };
    float _scatterAcross { 0.0f };
    float _tailStraight { 1.0f };
    float _tailAcross { 0.0f };
    float _tailKeep { 0.999f };
    float _outputGain { 0.5f };

    float _damperGain { 1.0f };
    float _damperFeedforward { 0.0f };
    float _damperFeedback { 0.0f };

    double _dcPole { 1.0 };
    double _dcGain { 1.0 };

    float _peak { 0.0f };
};

} // namespace DiffusionDelay
