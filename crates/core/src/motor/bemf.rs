use super::dsp::EmaFilter;

pub struct BemfEstimator {
    r_armature: f32,
    poles: i32,

    v_applied: f32,
    i_avg: f32,
    ripple_freq: f32,

    v_bemf: f32,
    estimated_rpm: f32,

    bemf_constant: f32,
    bemf_k_filter: EmaFilter,
    #[allow(dead_code)]
    r_filter: EmaFilter,
    rpm_filter: EmaFilter,

    use_ripple: bool,
}

impl Default for BemfEstimator {
    fn default() -> Self {
        let mut f = EmaFilter::new(0.001);
        f.reset(0.015);
        Self {
            r_armature: 35.0,
            poles: 5,
            v_applied: 0.0,
            i_avg: 0.0,
            ripple_freq: 0.0,
            v_bemf: 0.0,
            estimated_rpm: 0.0,
            bemf_constant: 0.015,
            bemf_k_filter: f,
            r_filter: EmaFilter::new(1.0),
            rpm_filter: EmaFilter::new(0.05),
            use_ripple: false,
        }
    }
}

impl BemfEstimator {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn set_motor_params(&mut self, r_armature: f32, poles: i32) {
        if r_armature > 0.0 {
            self.r_armature = r_armature;
        }
        if poles > 0 {
            self.poles = poles;
        }
    }

    pub fn set_bemf_constant(&mut self, ke: f32) {
        if ke > 0.0 {
            self.bemf_constant = ke;
            self.bemf_k_filter.reset(ke);
        }
    }

    pub fn update_low_speed_data(&mut self, v_applied: f32, i_avg: f32) {
        self.v_applied = v_applied;
        self.i_avg = i_avg;
    }

    pub fn update_ripple_freq(&mut self, freq_hz: f32) {
        self.ripple_freq = freq_hz;
    }

    pub fn calculate_estimate(&mut self) {
        let i_stall_theoretical = if self.r_armature > 0.1 {
            self.v_applied / self.r_armature
        } else {
            0.0
        };

        let physically_stalled = self.v_applied > 0.5
            && self.i_avg > 0.01
            && self.i_avg > i_stall_theoretical * 0.98;

        let v_drop = self.i_avg * self.r_armature;
        self.v_bemf = self.v_applied - v_drop;
        if self.v_bemf < 0.0 {
            self.v_bemf = 0.0;
        }

        let estimated_rpm_from_bemf = if self.bemf_constant > 0.0 {
            self.v_bemf / self.bemf_constant
        } else {
            0.0
        };

        let ripple_rpm = if self.ripple_freq > 0.0 {
            (self.ripple_freq * 60.0) / (2.0 * self.poles as f32)
        } else {
            0.0
        };

        let ripple_valid =
            ripple_rpm > 0.0 && self.i_avg > 0.20 && self.v_applied > 3.0;

        let raw_estimate;
        if ripple_valid {
            raw_estimate = ripple_rpm;
            self.use_ripple = true;

            if ripple_rpm > 800.0 && self.v_applied > 4.0 && self.v_bemf > 1.0 {
                let instant_k = self.v_bemf / ripple_rpm;
                if instant_k > 0.005 && instant_k < 0.03 {
                    self.bemf_constant = self.bemf_k_filter.update(instant_k);
                }
            }
        } else if self.v_applied > 2.0 {
            raw_estimate = estimated_rpm_from_bemf;
            self.use_ripple = false;
        } else {
            raw_estimate = 0.0;
            self.use_ripple = false;
        }

        let raw_estimate = if physically_stalled || self.v_applied < 0.4 {
            0.0
        } else {
            raw_estimate
        };

        self.estimated_rpm = self.rpm_filter.update(raw_estimate);

        if self.v_applied < 0.05 {
            self.estimated_rpm = 0.0;
            self.rpm_filter.reset(0.0);
        }
    }

    pub fn reset(&mut self) {
        self.r_armature = 35.0;
        self.bemf_constant = 0.015;
        self.rpm_filter.reset(0.0);
        self.use_ripple = false;
        self.bemf_k_filter.reset(0.015);
    }

    pub fn get_estimated_rpm(&self) -> f32 {
        self.estimated_rpm
    }

    pub fn get_bemf_voltage(&self) -> f32 {
        self.v_bemf
    }

    pub fn get_measured_resistance(&self) -> f32 {
        self.r_armature
    }

    pub fn get_bemf_constant(&self) -> f32 {
        self.bemf_constant
    }

    pub fn is_stalled(&self) -> bool {
        self.v_applied > 2.0 && self.estimated_rpm < 10.0
    }

    pub fn use_ripple(&self) -> bool {
        self.use_ripple
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_bemf_low_speed() {
        let mut est = BemfEstimator::new();
        est.set_motor_params(2.0, 5);
        est.update_low_speed_data(5.0, 1.0);
        est.calculate_estimate();
        assert!((est.get_bemf_voltage() - 3.0).abs() < 0.1);
    }

    #[test]
    fn test_bemf_ripple() {
        let mut est = BemfEstimator::new();
        est.set_motor_params(2.0, 5);
        est.update_low_speed_data(5.0, 1.0);
        est.update_ripple_freq(110.0);
        est.calculate_estimate();
        let rpm = est.get_estimated_rpm();
        assert!(rpm > 30.0 && rpm < 50.0, "rpm={} not in expected range", rpm);
    }

    #[test]
    fn test_bemf_stalled() {
        let mut est = BemfEstimator::new();
        est.set_motor_params(35.0, 5);
        est.update_low_speed_data(12.0, 0.35);
        est.calculate_estimate();
        assert!(est.is_stalled());
    }

    #[test]
    fn test_bemf_no_voltage() {
        let mut est = BemfEstimator::new();
        est.update_low_speed_data(0.0, 0.0);
        est.calculate_estimate();
        assert!((est.get_estimated_rpm() - 0.0).abs() < 1e-6);
    }
}
