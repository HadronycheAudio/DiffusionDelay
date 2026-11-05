#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace DiffusionDelay {

// A delay line whose length glides to a new value. Once it arrives, it reads whole samples, which pass through
// unchanged however many times they go round the feedback loop. Only while it moves does it interpolate, and
// linearly, because that can't add energy to a loop that has to stay stable at 100% feedback.
class DelayLine
{
public:
    // Not for the audio thread.
    void prepare (size_t maxDelaySamples)
    {
        size_t size = 16;
        while (size < maxDelaySamples + 2)
            size *= 2;

        _buffer.assign (size, 0.0f);
        _mask = size - 1;
        _write = 0;
    }

    void setTarget (int delaySamples) { _target = delaySamples; }

    void snapToTarget ()
    {
        _current = _target;
        _whole = static_cast<size_t> (_target);
        _fraction = 0.0f;
    }

    int getTarget () const { return _target; }

    // Moves the length on by one sample. Close enough to the target, it lands on it exactly.
    void glide (double coefficient)
    {
        if (_current == _target)
            return;

        _current = _target + (_current - _target) * coefficient;
        if (std::abs (_current - _target) < kArrivedSamples)
        {
            snapToTarget ();
            return;
        }

        const double whole = std::floor (_current);
        _whole = static_cast<size_t> (whole);
        _fraction = static_cast<float> (_current - whole);
    }

    float read () const
    {
        const float newer = _buffer[(_write - _whole) & _mask];
        if (_fraction == 0.0f)
            return newer;

        const float older = _buffer[(_write - _whole - 1) & _mask];
        return newer + (older - newer) * _fraction;
    }

    void write (float sample)
    {
        _buffer[_write] = sample;
        _write = (_write + 1) & _mask;
    }

private:
    static constexpr double kArrivedSamples = 0.001;

    std::vector<float> _buffer;
    size_t _mask { 0 };
    size_t _write { 0 };

    int _target { 1 };
    double _current { 1.0 };
    size_t _whole { 1 };
    float _fraction { 0.0f };
};

} // namespace DiffusionDelay
