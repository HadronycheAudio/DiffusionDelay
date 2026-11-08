//! DiffusionDelay's Rust side. This is the first step of moving the signal processing here: the only thing it does
//! yet is measure the input's peak, which the processor uses to tell when it can sleep. Everything exported here is
//! declared in src/Rust.h, and the two have to agree.

use core::slice;

/// The loudest sample across two channels of `count` samples each, ignoring sign. A null channel counts as silent,
/// and so does NaN, as `hadronyche_dsp::peak` describes.
///
/// # Safety
///
/// Each channel is either null or points to `count` floats that stay readable for the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn diffusiondelay_peak(
    left: *const f32,
    right: *const f32,
    count: usize,
) -> f32 {
    let channel = |samples: *const f32| -> &[f32] {
        if samples.is_null() || count == 0 {
            &[]
        } else {
            // SAFETY: the caller promises count readable floats behind a channel that isn't null.
            unsafe { slice::from_raw_parts(samples, count) }
        }
    };
    hadronyche_dsp::peak([channel(left), channel(right)])
}
