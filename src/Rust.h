#pragma once

#include <cstddef>

// What the plug-in's Rust side in rust/ exports. rust/src/lib.rs defines each of these, and the two have to agree.
extern "C" {

// The loudest sample across both channels, ignoring sign. A null channel counts as silent, and so does NaN.
float diffusiondelay_peak (const float* left, const float* right, size_t count);

} // extern "C"
