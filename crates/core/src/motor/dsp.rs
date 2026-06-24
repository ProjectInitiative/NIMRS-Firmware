pub struct EmaFilter {
    alpha: f32,
    value: f32,
}

impl EmaFilter {
    pub fn new(alpha: f32) -> Self {
        Self {
            alpha: alpha.clamp(0.0, 1.0),
            value: 0.0,
        }
    }

    pub fn set_alpha(&mut self, alpha: f32) {
        self.alpha = alpha.clamp(0.0, 1.0);
    }

    pub fn update(&mut self, input: f32) -> f32 {
        self.value = self.alpha * input + (1.0 - self.alpha) * self.value;
        self.value
    }

    pub fn value(&self) -> f32 {
        self.value
    }

    pub fn reset(&mut self, initial: f32) {
        self.value = initial;
    }
}

pub struct DcBlocker {
    alpha: f32,
    prev_input: f32,
    prev_output: f32,
}

impl DcBlocker {
    pub fn new(alpha: f32) -> Self {
        Self {
            alpha,
            prev_input: 0.0,
            prev_output: 0.0,
        }
    }

    pub fn process(&mut self, input: f32) -> f32 {
        let output = self.alpha * self.prev_output + self.alpha * (input - self.prev_input);
        self.prev_input = input;
        self.prev_output = output;
        output
    }

    pub fn reset(&mut self) {
        self.prev_input = 0.0;
        self.prev_output = 0.0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ema_filter() {
        let mut ema = EmaFilter::new(0.5);
        assert!((ema.update(10.0) - 5.0).abs() < 1e-6);
        assert!((ema.update(10.0) - 7.5).abs() < 1e-6);
    }

    #[test]
    fn test_ema_clamp() {
        let ema = EmaFilter::new(1.5);
        assert!((ema.alpha - 1.0).abs() < 1e-6);
        let ema = EmaFilter::new(-0.5);
        assert!((ema.alpha - 0.0).abs() < 1e-6);
    }

    #[test]
    fn test_ema_reset() {
        let mut ema = EmaFilter::new(0.5);
        ema.update(100.0);
        ema.reset(0.0);
        assert!((ema.value() - 0.0).abs() < 1e-6);
    }

    #[test]
    fn test_dc_blocker() {
        let mut dc = DcBlocker::new(0.9);
        let y1 = dc.process(10.0);
        assert!((y1 - 9.0).abs() < 0.001);
    }

    #[test]
    fn test_dc_blocker_reset() {
        let mut dc = DcBlocker::new(0.9);
        dc.process(10.0);
        dc.reset();
        assert!((dc.process(0.0) - 0.0).abs() < 1e-6);
    }
}
