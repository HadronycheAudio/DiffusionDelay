//! DiffusionDelay's signal processing: the parameters, and the processor that runs the signal path described in
//! docs/signal-path.md. The C++ side, in src/, is the VST3 classes around it, and reads the parameter table from
//! here through `hadronyche_plugin`.

mod delay_line;
mod diffusion_loop;

use core::ffi::CStr;

use hadronyche_dsp::{Smoother, VOLUME_SILENCE_DB, db_to_gain, dry_gain, volume_to_gain, wet_gain};
use hadronyche_plugin::{Block, Curve, Output, ParamSpec, Params, Processor, Tail, export_plugin, params};

use diffusion_loop::DiffusionLoop;

/// The range of Time. A synced time is held to it as well, and the longest sets the size of the delay lines.
pub const MIN_TIME_MS: f64 = 10.0;
pub const MAX_TIME_MS: f64 = 10000.0;

// Saved in projects by position, so only ever add to the end of these. Note values rather than bars, so that they
// mean the same whatever the time signature.
const SYNC_MODES: &[&CStr] = &[c"Off", c"On"];

const DIVISIONS: &[&CStr] = &[
    c"1/32", c"1/16T", c"1/32D", c"1/16", c"1/8T", c"1/16D", c"1/8", c"1/4T", c"1/8D", c"1/4", c"1/2T", c"1/4D",
    c"1/2", c"1/1T", c"1/2D", c"1/1", c"1/1D", c"2/1", c"4/1",
];

/// The length of each entry in DIVISIONS, in quarter notes.
const DIVISION_BEATS: [f64; 19] = [
    1.0 / 8.0,
    1.0 / 6.0,
    3.0 / 16.0,
    1.0 / 4.0,
    1.0 / 3.0,
    3.0 / 8.0,
    1.0 / 2.0,
    2.0 / 3.0,
    3.0 / 4.0,
    1.0,
    4.0 / 3.0,
    3.0 / 2.0,
    2.0,
    8.0 / 3.0,
    3.0,
    4.0,
    6.0,
    8.0,
    16.0,
];

params! {
    /// Parameter IDs are saved in projects and automation: never reorder or remove one, only add at the end.
    pub enum Param => PARAMS {
        Time => ParamSpec::new(c"Time", c"ms", MIN_TIME_MS, MAX_TIME_MS, 375.0, Curve::Log, 0),
        Feedback => ParamSpec::new(c"Feedback", c"%", 0.0, 100.0, 50.0, Curve::Linear, 0),
        Diffusion => ParamSpec::new(c"Diffusion", c"%", 0.0, 100.0, 50.0, Curve::Linear, 0),
        Damper => ParamSpec::new(c"Damper", c"dB", 0.0, 18.0, 3.0, Curve::Linear, 1),
        DryWet => ParamSpec::new(c"Dry/wet", c"%", 0.0, 100.0, 50.0, Curve::Linear, 0),
        Volume => ParamSpec::new(c"Volume", c"dB", VOLUME_SILENCE_DB, 6.0, 0.0, Curve::Linear, 1).silent_at_min(),
        Sync => ParamSpec::list(c"Sync", SYNC_MODES, 0),
        Division => ParamSpec::list(c"Division", DIVISIONS, 8),
    }
}

/// The length of an entry in Division's list, in quarter notes.
pub fn division_beats(division: i32) -> f64 {
    DIVISION_BEATS[division.clamp(0, DIVISION_BEATS.len() as i32 - 1) as usize]
}

const SMOOTHING_MS: f64 = 20.0;

/// How long a change of Time takes to settle, as a time constant. The lines glide there like a tape, which bends the
/// pitch of what's in them.
const TIME_GLIDE_MS: f64 = 100.0;

/// How often what's costly to calculate is updated, in samples.
const CONTROL_INTERVAL: i32 = 16;

/// Below this, there's nothing to hear. It's about -140 dB.
const SILENT_PEAK: f32 = 1e-7;

pub struct DiffusionDelay {
    sample_rate: f64,
    network: DiffusionLoop,
    time_samples: i32,
    control_countdown: i32,

    /// The last tempo the host gave, for hosts that don't always give one.
    tempo: f64,

    // Per control tick.
    diffusion: Smoother,
    damper_gain: Smoother,

    // Per sample.
    feedback: Smoother,
    dry_gain: Smoother,
    wet_gain: Smoother,
    volume: Smoother,

    /// With nothing left to hear, and nothing left in the loop, the plug-in stops working.
    silent_samples: i64,
    is_first_block: bool,
}

impl DiffusionDelay {
    fn time_ms(&self, params: &Params<'_>) -> f64 {
        if params.plain(Param::Sync) < 0.5 {
            return params.plain(Param::Time);
        }

        // A slow tempo can put the longest divisions past the end of the delay lines.
        let beats = division_beats(params.plain(Param::Division) as i32);
        (beats * 60000.0 / self.tempo).clamp(MIN_TIME_MS, MAX_TIME_MS)
    }

    fn apply_parameters(&mut self, params: &Params<'_>) {
        let time_samples = (self.time_ms(params) / 1000.0 * self.sample_rate).round() as i32;
        if time_samples != self.time_samples {
            self.time_samples = time_samples;
            self.network.set_time(time_samples);
        }

        self.diffusion.set_target(params.plain(Param::Diffusion) / 100.0);
        self.damper_gain.set_target(db_to_gain(-params.plain(Param::Damper)));
        self.feedback.set_target(params.plain(Param::Feedback) / 100.0);
        self.dry_gain.set_target(dry_gain(params.plain(Param::DryWet)));
        self.wet_gain.set_target(wet_gain(params.plain(Param::DryWet)));
        self.volume.set_target(volume_to_gain(params.plain(Param::Volume)));
    }

    /// Moves everything straight to its current target, for when nobody can hear it moving.
    fn snap_smoothers(&mut self) {
        for smoother in [
            &mut self.diffusion,
            &mut self.damper_gain,
            &mut self.feedback,
            &mut self.dry_gain,
            &mut self.wet_gain,
            &mut self.volume,
        ] {
            smoother.snap_to_target();
        }
        self.network.snap_to_time();
    }

    fn update_controls(&mut self) {
        self.network.set_diffusion(self.diffusion.step());
        self.network.set_damper_gain(self.damper_gain.step());
    }

    fn render(&mut self, input: Option<[&[f32]; 2]>, output: [&mut [f32]; 2]) {
        let [output_left, output_right] = output;
        for i in 0..output_left.len() {
            // A broken sample upstream would otherwise go round the loop for ever.
            let sample = |channel: usize| input.map_or(0.0, |input| input[channel][i]);
            let left = finite_or_silence(sample(0));
            let right = finite_or_silence(sample(1));

            self.control_countdown -= 1;
            if self.control_countdown < 0 {
                self.control_countdown = CONTROL_INTERVAL - 1;
                self.update_controls();
            }

            let feedback = self.feedback.step_f32();
            let (wet_left, wet_right) = self.network.process(left, right, feedback);

            let dry_gain = self.dry_gain.step_f32();
            let wet_gain = self.wet_gain.step_f32();
            let volume = self.volume.step_f32();
            output_left[i] = (left * dry_gain + wet_left * wet_gain) * volume;
            output_right[i] = (right * dry_gain + wet_right * wet_gain) * volume;
        }
    }
}

fn finite_or_silence(sample: f32) -> f32 {
    if sample.is_finite() { sample } else { 0.0 }
}

impl Processor for DiffusionDelay {
    const PARAMS: &'static [ParamSpec] = &PARAMS;
    const STATE_VERSION: i32 = 1;
    const HAS_AUDIO_INPUT: bool = true;

    fn new() -> Self {
        Self {
            sample_rate: 0.0,
            network: DiffusionLoop::default(),
            time_samples: 0,
            control_countdown: 0,
            tempo: 120.0,
            diffusion: Smoother::default(),
            damper_gain: Smoother::default(),
            feedback: Smoother::default(),
            dry_gain: Smoother::default(),
            wet_gain: Smoother::default(),
            volume: Smoother::default(),
            silent_samples: 0,
            is_first_block: true,
        }
    }

    fn prepare(&mut self, sample_rate: f64, _max_block_size: usize, params: &Params<'_>) {
        self.sample_rate = sample_rate;
        self.network.prepare(sample_rate, MAX_TIME_MS / 1000.0);
        self.network
            .set_glide_coefficient((-1000.0 / (sample_rate * TIME_GLIDE_MS)).exp());
        self.time_samples = 0;

        let ticks_per_second = sample_rate / f64::from(CONTROL_INTERVAL);
        for smoother in [&mut self.diffusion, &mut self.damper_gain] {
            smoother.prepare(ticks_per_second, SMOOTHING_MS);
        }
        for smoother in [
            &mut self.feedback,
            &mut self.dry_gain,
            &mut self.wet_gain,
            &mut self.volume,
        ] {
            smoother.prepare(sample_rate, SMOOTHING_MS);
        }

        self.control_countdown = 0;
        self.silent_samples = 0;
        self.is_first_block = true;

        self.apply_parameters(params);
        self.snap_smoothers();
        self.update_controls();
    }

    fn process(&mut self, block: Block<'_>) -> Output {
        if let Some(tempo) = block.transport.tempo() {
            self.tempo = tempo;
        }

        self.apply_parameters(&block.params);

        // Changes arriving with the first block are where the plug-in starts, not something to ramp to.
        if self.is_first_block {
            self.snap_smoothers();
            self.is_first_block = false;
        }

        let count = block.len() as i32;

        // Looks at the samples rather than the host's silence flags, which are only ever a hint.
        let input_peak = block.input.map_or(0.0, hadronyche_dsp::peak);

        // Quiet for as long as anything can take to get through, and there's nothing left to come out.
        let longest = i64::from(self.network.longest_path_samples()) + i64::from(count);
        if input_peak < SILENT_PEAK && self.silent_samples >= longest {
            self.control_countdown =
                (self.control_countdown + CONTROL_INTERVAL - count % CONTROL_INTERVAL) % CONTROL_INTERVAL;

            // Nobody can hear parameters move while asleep, and sound shouldn't start with them still ramping.
            self.snap_smoothers();
            self.update_controls();
            return Output::Silence;
        }

        self.render(block.input, block.output);

        let is_quiet = input_peak < SILENT_PEAK && self.network.take_peak() < SILENT_PEAK;
        self.silent_samples = if is_quiet {
            self.silent_samples + i64::from(count)
        } else {
            0
        };
        Output::Sound
    }

    /// At 100% feedback, it never ends. The silence of each block says when it has.
    fn tail(&self, _sample_rate: f64) -> Tail {
        Tail::Infinite
    }
}

export_plugin!(DiffusionDelay);

#[cfg(test)]
mod tests {
    use super::*;
    use hadronyche_plugin::header;
    use std::path::Path;

    #[test]
    fn param_ids_header_is_current() {
        let path = Path::new(env!("CARGO_MANIFEST_DIR")).join("../src/ParamIds.h");
        if let Err(problem) = header::check(&path, &header::param_ids("DiffusionDelay", &Param::NAMES)) {
            panic!("{problem}");
        }
    }

    #[test]
    fn every_division_has_its_length() {
        assert_eq!(DIVISIONS.len(), DIVISION_BEATS.len());
        assert_eq!(division_beats(9), 1.0, "a quarter note is one beat");
        assert_eq!(division_beats(-5), DIVISION_BEATS[0]);
        assert_eq!(division_beats(99), 16.0);
        assert_eq!(
            PARAMS[Param::Division as usize].default,
            8.0,
            "a dotted eighth to start with"
        );
    }

    #[test]
    fn the_defaults_are_what_projects_have_always_started_from() {
        let plain: Vec<f64> = PARAMS.iter().map(|spec| spec.default).collect();
        assert_eq!(plain, [375.0, 50.0, 50.0, 3.0, 50.0, 0.0, 0.0, 8.0]);
    }
}
