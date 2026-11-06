#pragma once

// The test host from HadronycheCore, and what this plug-in's tests add to it.

#include "Parameters.h"

#include "hadronyche/testhost/Host.h"

#include <cstddef>
#include <string>
#include <vector>

namespace DiffusionDelay {
namespace Tests {

using namespace Hadronyche::TestHost;

class Plug : public PlugBase
{
public:
    using PlugBase::PlugBase;

    // What the host says the tempo is from now on. A plug-in starts with no tempo from the host at all, and
    // zero goes back to that.
    void setTempo (double bpm);

private:
    Steinberg::Vst::ProcessContext _tempoContext {};
};

double measureFrequency (const Samples& samples, double around, double sampleRate = kSampleRate);

} // namespace Tests
} // namespace DiffusionDelay
