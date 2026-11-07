#pragma once

// The test host from HadronycheCore, and what this plug-in's tests add to it.

#include "Parameters.h"

#include "hadronyche/testhost/Host.h"

#include <cstddef>
#include <vector>

namespace DiffusionDelay {
namespace Tests {

using namespace Hadronyche::TestHost;

using Plug = PlugBase;

double measureFrequency (const Samples& samples, double around, double sampleRate = kSampleRate);

} // namespace Tests
} // namespace DiffusionDelay
