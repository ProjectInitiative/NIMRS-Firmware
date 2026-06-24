use super::dsp::{DcBlocker, EmaFilter};

pub struct RippleDetector {
    dc_blocker: DcBlocker,
    state: bool,
    threshold_high: f32,
    threshold_low: f32,
    samples_since_pulse: u32,
    current_freq: f32,
    freq_filter: EmaFilter,
}

impl Default for RippleDetector {
    fn default() -> Self {
        Self {
            dc_blocker: DcBlocker::new(0.9),
            state: false,
            threshold_high: 0.05,
            threshold_low: -0.05,
            samples_since_pulse: 0,
            current_freq: 0.0,
            freq_filter: EmaFilter::new(0.3),
        }
    }
}

impl RippleDetector {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn process_buffer(&mut self, data: &[f32], sample_rate: f32) {
        if data.is_empty() || sample_rate <= 0.0 {
            return;
        }
        let sample_interval_us = 1_000_000.0 / sample_rate;

        for &sample_in in data {
            self.samples_since_pulse += 1;

            let sample = self.dc_blocker.process(sample_in);

            if !self.state && sample > self.threshold_high {
                self.state = true;
                let dt = self.samples_since_pulse as f32 * sample_interval_us;
                self.samples_since_pulse = 0;

                if dt > 2000.0 && dt < 200_000.0 {
                    let inst_freq = 1_000_000.0 / dt;
                    self.current_freq = self.freq_filter.update(inst_freq);
                }
            } else if self.state && sample < self.threshold_low {
                self.state = false;
            } else {
                let elapsed_us = self.samples_since_pulse as f32 * sample_interval_us;
                if elapsed_us > 200_000.0 {
                    self.current_freq = 0.0;
                    self.state = false;
                }
            }
        }
    }

    pub fn get_frequency(&self) -> f32 {
        self.current_freq
    }

    pub fn reset(&mut self) {
        self.dc_blocker.reset();
        self.state = false;
        self.current_freq = 0.0;
        self.samples_since_pulse = 0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::f32::consts::PI;

    #[test]
    fn test_ripple_100hz() {
        let mut detector = RippleDetector::new();
        let sample_rate = 20_000.0;
        let dt = 1.0 / sample_rate;

        let mut buffer = Vec::with_capacity(4000);
        for i in 0..4000 {
            let t = i as f32 * dt;
            buffer.push((2.0 * PI * 100.0 * t).sin() + 0.5);
        }

        detector.process_buffer(&buffer, sample_rate);
        let freq = detector.get_frequency();
        assert!(freq > 95.0 && freq < 105.0, "freq={} not near 100Hz", freq);
    }

    #[test]
    fn test_ripple_zero_input() {
        let mut detector = RippleDetector::new();
        detector.process_buffer(&[], 20_000.0);
        assert!((detector.get_frequency() - 0.0).abs() < 1e-6);
    }

    #[test]
    fn test_ripple_reset() {
        let mut detector = RippleDetector::new();
        let sample_rate = 20_000.0;
        let dt = 1.0 / sample_rate;
        let mut buffer = Vec::with_capacity(4000);
        for i in 0..4000 {
            let t = i as f32 * dt;
            buffer.push((2.0 * PI * 100.0 * t).sin() + 0.5);
        }
        detector.process_buffer(&buffer, sample_rate);
        assert!(detector.get_frequency() > 0.0);
        detector.reset();
        assert!((detector.get_frequency() - 0.0).abs() < 1e-6);
    }
}
