//! A delay line whose length glides to a new value.

/// Once it arrives, it reads whole samples, which pass through unchanged however many times they go round the
/// feedback loop. Only while it moves does it interpolate, and linearly, because that can't add energy to a loop that
/// has to stay stable at 100% feedback.
#[derive(Clone, Debug)]
pub struct DelayLine {
    buffer: Vec<f32>,
    mask: usize,
    write: usize,
    target: i32,
    current: f64,
    whole: usize,
    fraction: f32,
}

impl Default for DelayLine {
    fn default() -> Self {
        Self {
            buffer: Vec::new(),
            mask: 0,
            write: 0,
            target: 1,
            current: 1.0,
            whole: 1,
            fraction: 0.0,
        }
    }
}

/// Close enough to the target to land on it exactly.
const ARRIVED_SAMPLES: f64 = 0.001;

impl DelayLine {
    /// Not for the audio thread.
    pub fn prepare(&mut self, max_delay_samples: usize) {
        let mut size = 16;
        while size < max_delay_samples + 2 {
            size *= 2;
        }
        self.buffer.clear();
        self.buffer.resize(size, 0.0);
        self.mask = size - 1;
        self.write = 0;
    }

    pub fn set_target(&mut self, delay_samples: i32) {
        self.target = delay_samples;
    }

    pub fn snap_to_target(&mut self) {
        self.current = f64::from(self.target);
        self.whole = self.target as usize;
        self.fraction = 0.0;
    }

    pub fn target(&self) -> i32 {
        self.target
    }

    /// Moves the length on by one sample. Close enough to the target, it lands on it exactly.
    pub fn glide(&mut self, coefficient: f64) {
        let target = f64::from(self.target);
        if self.current == target {
            return;
        }

        self.current = target + (self.current - target) * coefficient;
        if (self.current - target).abs() < ARRIVED_SAMPLES {
            self.snap_to_target();
            return;
        }

        let whole = self.current.floor();
        self.whole = whole as usize;
        self.fraction = (self.current - whole) as f32;
    }

    pub fn read(&self) -> f32 {
        let newer = self.buffer[self.write.wrapping_sub(self.whole) & self.mask];
        if self.fraction == 0.0 {
            return newer;
        }

        let older = self.buffer[self.write.wrapping_sub(self.whole).wrapping_sub(1) & self.mask];
        newer + (older - newer) * self.fraction
    }

    pub fn write(&mut self, sample: f32) {
        self.buffer[self.write] = sample;
        self.write = (self.write + 1) & self.mask;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_settled_line_returns_whole_samples_after_its_length() {
        let mut line = DelayLine::default();
        line.prepare(8);
        line.set_target(3);
        line.snap_to_target();
        let mut out = Vec::new();
        for i in 1..=6 {
            out.push(line.read());
            line.write(i as f32);
        }
        assert_eq!(out, [0.0, 0.0, 0.0, 1.0, 2.0, 3.0]);
    }

    #[test]
    fn a_moving_line_interpolates_and_then_lands_exactly() {
        let mut line = DelayLine::default();
        line.prepare(8);
        line.set_target(1);
        line.snap_to_target();
        for i in 0..8 {
            line.write(i as f32);
        }
        line.set_target(3);
        line.glide(0.25);
        assert_eq!(line.read(), 5.5, "halfway between 2 and 3 samples ago");
        for _ in 0..20 {
            line.glide(0.25);
        }
        assert_eq!(line.read(), 5.0, "arrived at exactly 3 samples");
    }
}
