//! The echo and its feedback path, as described in docs/signal-path.md.
//!
//! Eight parallel channels each pass through four delay stages that add up to exactly the echo time. Between the
//! stages, a rotation moves signal from each channel to the others. A signal that changes channel takes a different
//! total time: some paths arrive before the echo time, which are the early reflections, and some after it.
//!
//! Every part is lossless, whatever the Diffusion setting: delays and rotations can only move energy around. That is
//! what lets the repeats go on for ever at 100% feedback while they blur, instead of ringing or running away, so a
//! change that breaks it (a gain, a filter, an unequal path) would show up in the `endless` tests.
//!
//! The arithmetic is the C++ `DiffusionLoop`'s, in the same order and the same precision at each step, so that the
//! two give the same bits.

use crate::delay_line::DelayLine;

pub const CHANNELS: usize = 8;
pub const STAGES: usize = 4;

const PI: f64 = core::f64::consts::PI;

const DAMPER_HZ: f64 = 4000.0;

/// See `soft_limit` for why it's here. It's so low because a repeat goes through it on every pass, and with a short
/// time, that's many times a second: at 3 Hz, a 40 Hz note at 100 ms fades by 14 dB a minute, and here about 1.6.
const DC_BLOCK_HZ: f64 = 1.0;

/// How far the rotations and the tails turn at 100%. Between them, the straight path is about 8 dB down on each
/// pass, leaving a clear repeat that dissolves over the next few. See docs/signal-path.md.
const MAX_SCATTER_RADIANS: f64 = 35.0 * PI / 180.0;
const MAX_TAIL_RADIANS: f64 = 45.6 * PI / 180.0;

/// At 100% Diffusion.
const OUTPUT_MAKEUP_DB: f64 = 3.0;

/// Parts per thousand of the echo time. Each row adds up to 1000, so that all the straight paths take exactly the
/// echo time. Within each column they're spread out, so that a change of channel changes the arrival time, and they
/// were chosen by a search to make all those times land apart.
const STAGE_SHARES: [[i32; STAGES]; CHANNELS] = [
    [143, 263, 263, 331],
    [173, 341, 355, 131],
    [305, 197, 163, 335],
    [203, 281, 169, 347],
    [201, 139, 307, 353],
    [331, 347, 181, 141],
    [271, 167, 365, 197],
    [363, 211, 243, 183],
];

/// Parts per thousand of the echo time, which are primes so that they don't share echoes.
const TAIL_SHARES: [i32; CHANNELS] = [31, 37, 43, 53, 61, 71, 83, 97];

/// A skew-symmetric conference matrix of order 8, from the Paley construction on the integers modulo 7. Its rows are
/// orthogonal and it's its own negative transpose, so that cos (a) I + sin (a) / sqrt (7) S is exactly a rotation at
/// every angle, and a = 0 leaves the channels alone.
const SCATTER: [[i32; CHANNELS]; CHANNELS] = [
    [0, 1, 1, 1, 1, 1, 1, 1],
    [-1, 0, 1, 1, -1, 1, -1, -1],
    [-1, -1, 0, 1, 1, -1, 1, -1],
    [-1, -1, -1, 0, 1, 1, -1, 1],
    [-1, 1, -1, -1, 0, 1, 1, -1],
    [-1, -1, 1, -1, -1, 0, 1, 1],
    [-1, 1, -1, 1, -1, -1, 0, 1],
    [-1, 1, 1, -1, 1, -1, -1, 0],
];

/// Per channel. A full-scale input puts 0.5 on each of its channels, and adds back to full scale on the way out.
const LIMIT_KNEE: f32 = 0.7;
const LIMIT_CEILING: f32 = 1.0;

#[derive(Clone, Copy, Debug, Default)]
struct DamperState {
    previous_input: f32,
    low_pass: f32,
}

/// Double precision, because the pole is so close to 1.
#[derive(Clone, Copy, Debug, Default)]
struct DcState {
    previous_input: f64,
    output: f64,
}

#[derive(Clone, Debug)]
pub struct DiffusionLoop {
    stages: [[DelayLine; CHANNELS]; STAGES],
    tails: [DelayLine; CHANNELS],
    dampers: [DamperState; CHANNELS],
    dc_blockers: [DcState; CHANNELS],

    glide_coefficient: f64,

    scatter_straight: f32,
    scatter_across: f32,
    tail_straight: f32,
    tail_across: f32,
    tail_keep: f32,
    output_gain: f32,

    damper_gain: f32,
    damper_feedforward: f32,
    damper_feedback: f32,

    dc_pole: f64,
    dc_gain: f64,

    peak: f32,
}

impl Default for DiffusionLoop {
    fn default() -> Self {
        Self {
            stages: Default::default(),
            tails: Default::default(),
            dampers: Default::default(),
            dc_blockers: Default::default(),
            glide_coefficient: 0.0,
            scatter_straight: 1.0,
            scatter_across: 0.0,
            tail_straight: 1.0,
            tail_across: 0.0,
            tail_keep: 0.999,
            output_gain: 0.5,
            damper_gain: 1.0,
            damper_feedforward: 0.0,
            damper_feedback: 0.0,
            dc_pole: 1.0,
            dc_gain: 1.0,
            peak: 0.0,
        }
    }
}

/// `std::max` on floats: the second wins only when it's greater, so a NaN never does.
fn max(a: f32, b: f32) -> f32 {
    if a < b { b } else { a }
}

impl DiffusionLoop {
    /// Not for the audio thread.
    pub fn prepare(&mut self, sample_rate: f64, max_time_seconds: f64) {
        let max_time_samples = (sample_rate * max_time_seconds).ceil();
        for k in 0..CHANNELS {
            for (s, share) in STAGE_SHARES[k].iter().enumerate() {
                // The last stage takes up what the others' rounding left over, so a little extra room.
                self.stages[s][k].prepare((max_time_samples * f64::from(*share) / 1000.0) as usize + STAGES);
            }
            self.tails[k].prepare((max_time_samples * f64::from(TAIL_SHARES[k]) / 1000.0) as usize + 1);
        }

        self.dampers = Default::default();
        self.dc_blockers = Default::default();

        // A one-pole low-pass with its corner at DAMPER_HZ, by the bilinear transform, so it reaches exactly zero at
        // half the sample rate and the shelf reaches its full depth there.
        let warped = (PI * DAMPER_HZ.min(0.45 * sample_rate) / sample_rate).tan();
        self.damper_feedforward = (warped / (1.0 + warped)) as f32;
        self.damper_feedback = ((1.0 - warped) / (1.0 + warped)) as f32;

        self.dc_pole = (-2.0 * PI * DC_BLOCK_HZ / sample_rate).exp();
        self.dc_gain = 0.5 * (1.0 + self.dc_pole);
    }

    /// The time every straight path takes. Rounded so that the stages of each channel add up to exactly this.
    pub fn set_time(&mut self, samples: i32) {
        for k in 0..CHANNELS {
            let mut remaining = samples;
            for (s, share) in STAGE_SHARES[k].iter().enumerate().take(STAGES - 1) {
                let stage = ((f64::from(samples) * f64::from(*share) / 1000.0).round() as i32).max(1);
                self.stages[s][k].set_target(stage);
                remaining -= stage;
            }
            self.stages[STAGES - 1][k].set_target(remaining.max(1));
            let tail = ((f64::from(samples) * f64::from(TAIL_SHARES[k]) / 1000.0).round() as i32).max(1);
            self.tails[k].set_target(tail);
        }
    }

    pub fn snap_to_time(&mut self) {
        self.for_each_line(DelayLine::snap_to_target);
    }

    pub fn set_glide_coefficient(&mut self, coefficient: f64) {
        self.glide_coefficient = coefficient;
    }

    /// The longest one trip through the stages and a tail can take. What goes round a tail again is watched directly.
    pub fn longest_path_samples(&self) -> i32 {
        let stages: i32 = self
            .stages
            .iter()
            .map(|stage| stage.iter().map(DelayLine::target).max().unwrap_or(0))
            .sum();
        let tails = self.tails.iter().map(DelayLine::target).max().unwrap_or(0);
        stages + tails
    }

    /// Amount is 0 to 1. Costs two sines and cosines, so this isn't for every sample.
    pub fn set_diffusion(&mut self, amount: f64) {
        let scatter_angle = amount * MAX_SCATTER_RADIANS;
        self.scatter_straight = scatter_angle.cos() as f32;
        self.scatter_across = (scatter_angle.sin() / 7f64.sqrt()) as f32;

        let tail_angle = amount * MAX_TAIL_RADIANS;
        self.tail_straight = tail_angle.cos() as f32;
        self.tail_across = tail_angle.sin() as f32;

        // With the tails disconnected, whatever is in them could never be heard or leave, and would come back long
        // afterwards when Diffusion was next raised. So it drains away. This can't be heard.
        self.tail_keep = if amount > 0.0 { 1.0 } else { 0.999 };

        // The output adds up four channels. A clean repeat is the same on all four, so they add to full level, but
        // sound that has scattered is on one at a time and comes out 6 dB lower, though its energy is all still in
        // the loop. This makes up for most of that, outside the loop, so that the sound doesn't drop as it dissolves.
        self.output_gain = (0.5 * 10f64.powf(amount * OUTPUT_MAKEUP_DB / 20.0)) as f32;
    }

    /// The shelf's gain above DAMPER_HZ, on each pass. At 1, it changes nothing.
    pub fn set_damper_gain(&mut self, gain: f64) {
        self.damper_gain = gain as f32;
    }

    /// One sample in, and the left and right outputs.
    pub fn process(&mut self, input_left: f32, input_right: f32, feedback: f32) -> (f32, f32) {
        let coefficient = self.glide_coefficient;
        self.for_each_line(|line| line.glide(coefficient));

        let mut signal: [f32; CHANNELS] = core::array::from_fn(|k| self.stages[0][k].read());

        // A tail comes first so that later rotations spread it across the channels, and so across the stereo image.
        let mut peak = self.peak;
        for (tail, sample) in self.tails.iter_mut().zip(signal.iter_mut()) {
            let recirculating = tail.read();
            let direct = *sample;
            *sample = self.tail_straight * direct + self.tail_across * recirculating;
            tail.write((self.tail_straight * recirculating - self.tail_across * direct) * self.tail_keep);
            peak = max(peak, recirculating.abs());
        }

        for stage in self.stages.iter_mut().skip(1) {
            scatter(self.scatter_straight, self.scatter_across, &mut signal);
            for (line, sample) in stage.iter_mut().zip(signal.iter_mut()) {
                let delayed = line.read();
                line.write(*sample);
                *sample = delayed;
            }
        }

        let mut left = 0f32;
        let mut right = 0f32;
        for k in 0..CHANNELS / 2 {
            left += signal[k];
            right += signal[k + CHANNELS / 2];
        }
        let output = (self.output_gain * left, self.output_gain * right);

        // Every line has been read by now, so what comes out can go back in on this same sample. That keeps a trip
        // round the loop at exactly the echo time, rather than a sample more, which would drift out of time.
        for (k, sample) in signal.into_iter().enumerate() {
            peak = max(peak, sample.abs());

            // The damper only touches what goes round again, so the first repeat keeps all its top end.
            let damped = self.damp(k, sample);
            let returning = soft_limit(feedback * self.block_dc(k, damped));

            // Half on each of four channels. Added back up on the way out, that's the same level as the input.
            let input = 0.5 * if k < CHANNELS / 2 { input_left } else { input_right };
            self.stages[0][k].write(input + returning);
        }
        self.peak = peak;
        output
    }

    /// The loudest thing seen in the loop since the last call, for working out when it has gone quiet.
    pub fn take_peak(&mut self) -> f32 {
        core::mem::take(&mut self.peak)
    }

    fn for_each_line(&mut self, mut function: impl FnMut(&mut DelayLine)) {
        for line in self.stages.iter_mut().flatten().chain(self.tails.iter_mut()) {
            function(line);
        }
    }

    /// A one-pole shelf: all the low-pass, plus the rest of the signal at the shelf's gain. It can never have a gain
    /// above 1, and has exactly 1 at DC.
    fn damp(&mut self, channel: usize, input: f32) -> f32 {
        let state = &mut self.dampers[channel];
        state.low_pass =
            self.damper_feedforward * (input + state.previous_input) + self.damper_feedback * state.low_pass;
        state.previous_input = input;
        state.low_pass + self.damper_gain * (input - state.low_pass)
    }

    /// The gain makes this exactly 1 at the top of the range, rather than a little above, which would slowly run away
    /// at 100% feedback.
    fn block_dc(&mut self, channel: usize, input: f32) -> f32 {
        let state = &mut self.dc_blockers[channel];
        let input = f64::from(input);
        state.output = self.dc_gain * (input - state.previous_input) + self.dc_pole * state.output;
        state.previous_input = input;
        state.output as f32
    }
}

/// The rotation between two stages: `straight` of each channel, plus `across` times the matrix's mix of the others,
/// added up in channel order so that the bits match the C++.
fn scatter(straight: f32, across: f32, signal: &mut [f32; CHANNELS]) {
    let mixed: [f32; CHANNELS] = core::array::from_fn(|j| {
        let mix = SCATTER[j]
            .iter()
            .zip(signal.iter())
            .fold(0f32, |sum, (weight, sample)| sum + *weight as f32 * sample);
        straight * signal[j] + across * mix
    });
    *signal = mixed;
}

/// Holding a note into 100% feedback keeps adding to what's there, without limit. This holds that back, and leaves
/// everything quieter than a full-scale input untouched. Without the DC blocker, an offset in the input would also
/// build up to this ceiling and stay there, leaving no room for anything else.
fn soft_limit(sample: f32) -> f32 {
    let size = sample.abs();
    if size <= LIMIT_KNEE {
        return sample;
    }

    let limited =
        LIMIT_KNEE + (LIMIT_CEILING - LIMIT_KNEE) * ((size - LIMIT_KNEE) / (LIMIT_CEILING - LIMIT_KNEE)).tanh();
    if sample < 0.0 { -limited } else { limited }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_row_of_the_stage_shares_adds_up_to_the_whole_time() {
        for row in STAGE_SHARES {
            assert_eq!(row.iter().sum::<i32>(), 1000);
        }
    }

    #[test]
    fn the_stages_of_each_channel_add_up_to_exactly_the_time() {
        let mut network = DiffusionLoop::default();
        network.prepare(48000.0, 10.0);
        for samples in [1, 7, 480, 18000, 480000] {
            network.set_time(samples);
            for k in 0..CHANNELS {
                let total: i32 = (0..STAGES).map(|s| network.stages[s][k].target()).sum();
                assert_eq!(total, samples.max(STAGES as i32), "channel {k} at {samples}");
            }
        }
    }

    #[test]
    fn at_zero_diffusion_it_is_a_clean_delay() {
        let mut network = DiffusionLoop::default();
        network.prepare(48000.0, 1.0);
        network.set_time(100);
        network.snap_to_time();
        network.set_diffusion(0.0);
        let mut first_out = None;
        for i in 0..200 {
            let input = if i == 0 { 1.0 } else { 0.0 };
            let (left, right) = network.process(input, -input, 0.0);
            if left != 0.0 || right != 0.0 {
                first_out.get_or_insert((i, left, right));
            }
        }
        assert_eq!(first_out, Some((100, 1.0, -1.0)));
    }

    #[test]
    fn the_soft_limit_leaves_quiet_samples_alone_and_holds_loud_ones_back() {
        assert_eq!(soft_limit(0.5), 0.5);
        assert_eq!(soft_limit(-0.7), -0.7);
        assert!(soft_limit(3.0) < LIMIT_CEILING && soft_limit(3.0) > LIMIT_KNEE);
        assert_eq!(soft_limit(-3.0), -soft_limit(3.0));
    }
}
