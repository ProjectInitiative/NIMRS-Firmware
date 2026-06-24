use std::sync::{Mutex, OnceLock};

use esp_idf_sys::*;

use nimrs_core::motor::bemf::BemfEstimator;
use nimrs_core::motor::dsp::EmaFilter;
use nimrs_core::motor::ripple::RippleDetector;

use super::hal;

static MOTOR_TASK: OnceLock<Mutex<MotorTaskInner>> = OnceLock::new();

#[derive(Clone, Copy, PartialEq)]
pub enum AdaptiveState {
    Stopped,
    Startup,
    Baselining,
    Running,
}

#[derive(Clone, Copy, PartialEq)]
pub enum ResistanceState {
    Idle,
    Measuring,
    Done,
    Error,
}

pub struct Status {
    pub applied_voltage: f32,
    pub current: f32,
    pub estimated_rpm: f32,
    pub ripple_freq: f32,
    pub stalled: bool,
    pub hardware_fault: bool,
    pub is_moving: bool,
    pub duty: f32,
    pub raw_adc: u32,
}

struct MotorTaskInner {
    estimator: BemfEstimator,
    ripple_detector: RippleDetector,
    current_filter: EmaFilter,
    peak_filter: EmaFilter,
    target_speed_step: u8,
    target_direction: bool,
    current_duty: f32,
    pi_error_sum: f32,
    prev_current: f32,
    filtered_di_dt: f32,
    adaptive_state: AdaptiveState,
    state_start_time: u32,
    baseline_current: f32,
    baseline_sample_count: u16,
    baseline_sum: f32,
    kp: f32,
    ki: f32,
    track_voltage: f32,
    max_rpm: f32,
    v_start: f32,
    cv_pwm_dither: u8,
    cv_stiction_kick: u8,
    v_kick_active: bool,
    v_kick_start_time: u32,
    status: Status,
    resistance_state: ResistanceState,
    resistance_start_time: u32,
    measured_resistance: f32,
    test_mode: bool,

    last_v_control: f32,
    adc_offset: f32,
    last_log_time: u32,
    adc_buffer: [f32; 1024],
}

impl MotorTaskInner {
    fn new() -> Self {
        Self {
            estimator: BemfEstimator::new(),
            ripple_detector: RippleDetector::new(),
            current_filter: EmaFilter::new(0.1),
            peak_filter: EmaFilter::new(0.2),
            target_speed_step: 0,
            target_direction: true,
            current_duty: 0.0,
            pi_error_sum: 0.0,
            prev_current: 0.0,
            filtered_di_dt: 0.0,
            adaptive_state: AdaptiveState::Stopped,
            state_start_time: 0,
            baseline_current: 0.0,
            baseline_sample_count: 0,
            baseline_sum: 0.0,
            kp: 0.002,
            ki: 0.0005,
            track_voltage: 14.0,
            max_rpm: 3000.0,
            v_start: 0.0,
            cv_pwm_dither: 0,
            cv_stiction_kick: 0,
            v_kick_active: false,
            v_kick_start_time: 0,
            status: Status {
                applied_voltage: 0.0,
                current: 0.0,
                estimated_rpm: 0.0,
                ripple_freq: 0.0,
                stalled: false,
                hardware_fault: false,
                is_moving: false,
                duty: 0.0,
                raw_adc: 0,
            },
            resistance_state: ResistanceState::Idle,
            resistance_start_time: 0,
            measured_resistance: 0.0,
            test_mode: false,
            last_v_control: 0.0,
            adc_offset: 0.0,
            last_log_time: 0,
            adc_buffer: [0.0; 1024],
        }
    }

    fn process_tick(&mut self) {
        let scalar = hal::get_current_scalar();
        let samples = hal::get_adc_samples(&mut self.adc_buffer);

        let (avg_current, raw_max_adc, ripple_freq) = if samples > 0 {
            let mut sum_current = 0.0f32;
            let mut max_sample = 0.0f32;
            for &s in self.adc_buffer[..samples].iter() {
                sum_current += s;
                if s > max_sample {
                    max_sample = s;
                }
            }
            let instant_avg = sum_current / samples as f32;

            if self.current_duty.abs() < 0.01 {
                self.adc_offset = self.adc_offset * 0.9 + instant_avg * 0.1;
            }

            let calibrated_avg = (instant_avg - self.adc_offset).max(0.0);
            let avg = self.current_filter.update(calibrated_avg * scalar);
            self.peak_filter
                .update((max_sample - self.adc_offset).max(0.0) * scalar);

            for s in self.adc_buffer[..samples].iter_mut() {
                *s *= scalar;
            }
            self.ripple_detector
                .process_buffer(&self.adc_buffer[..samples], hal::get_adc_sample_rate());
            let rfreq = self.ripple_detector.get_frequency();

            (avg, max_sample as u32, rfreq)
        } else {
            (self.current_filter.value(), 0, 0.0)
        };

        let raw_di_dt = avg_current - self.prev_current;
        self.filtered_di_dt = 0.05 * raw_di_dt + 0.95 * self.filtered_di_dt;
        self.prev_current = avg_current;

        let now_ms = millis();
        let step = self.target_speed_step;
        let dir = self.target_direction;

        let mut low_speed_stall = false;
        if step == 0 {
            self.adaptive_state = AdaptiveState::Stopped;
        } else {
            match self.adaptive_state {
                AdaptiveState::Stopped => {
                    self.adaptive_state = AdaptiveState::Startup;
                    self.state_start_time = now_ms;
                }
                AdaptiveState::Startup => {
                    if (now_ms - self.state_start_time) > 500 {
                        self.adaptive_state = AdaptiveState::Baselining;
                        self.baseline_sample_count = 0;
                        self.baseline_sum = 0.0;
                    }
                }
                AdaptiveState::Baselining => {
                    self.baseline_sum += avg_current;
                    self.baseline_sample_count += 1;
                    if self.baseline_sample_count >= 50 {
                        self.baseline_current = self.baseline_sum / 50.0;
                        self.adaptive_state = AdaptiveState::Running;
                    }
                }
                AdaptiveState::Running => {
                    let current_step = (step as f32).min(128.0);
                    let slope = (1.5 - 4.0) / 128.0;
                    let dynamic_multiplier = 4.0 + slope * current_step;
                    if avg_current > self.baseline_current * dynamic_multiplier
                        && self.filtered_di_dt > 0.5
                    {
                        low_speed_stall = true;
                    }
                }
            }
        }

        // Resistance measurement override
        if self.resistance_state == ResistanceState::Measuring {
            let elapsed = now_ms - self.resistance_start_time;
            if elapsed < 1000 {
                self.current_duty = 3.0 / self.track_voltage;
            } else {
                self.current_duty = 0.0;
                if avg_current > 0.01 {
                    self.measured_resistance = 3.0 / avg_current;
                    self.estimator.set_motor_params(self.measured_resistance, -1);
                    self.resistance_state = ResistanceState::Done;
                } else {
                    self.resistance_state = ResistanceState::Error;
                }
            }
            hal::set_duty(self.current_duty);
            self.status.current = avg_current;
            self.status.duty = self.current_duty;
            self.status.applied_voltage = 3.0;
            return;
        }
        if self.resistance_state == ResistanceState::Done
            || self.resistance_state == ResistanceState::Error
        {
            self.current_duty = 0.0;
            hal::set_duty(0.0);
            if (now_ms - self.resistance_start_time) > 5000 {
                self.resistance_state = ResistanceState::Idle;
            }
            return;
        }

        // Three-zone motor control
        let v_applied_now = self.track_voltage * self.current_duty.abs();
        self.estimator.update_low_speed_data(v_applied_now, avg_current);
        self.estimator.update_ripple_freq(ripple_freq);
        self.estimator.calculate_estimate();
        let actual_rpm = self.estimator.get_estimated_rpm();
        let ripple_confirm = ripple_freq > 10.0;

        if step == 0 {
            self.current_duty = 0.0;
            self.pi_error_sum = 0.0;
            self.last_v_control = 0.0;
            self.v_kick_active = false;
        } else {
            let mut kick_bonus = 0.0;
            if !self.v_kick_active && v_applied_now < 0.1 {
                self.v_kick_active = true;
                self.v_kick_start_time = now_ms;
            }
            if self.v_kick_active {
                let elapsed = now_ms - self.v_kick_start_time;
                if elapsed < 100 {
                    kick_bonus = (self.cv_stiction_kick as f32 / 255.0) * 4.0;
                } else {
                    self.v_kick_active = false;
                }
            }

            let target_rpm = (step as f32 / 255.0) * self.max_rpm;

            let v_target = if !ripple_confirm && step < 20 {
                let target_current = (step as f32 / 255.0) * 0.5;
                let v = target_current * self.estimator.get_measured_resistance()
                    + self.v_start
                    + kick_bonus;
                if self.ki > 0.0 {
                    self.pi_error_sum = v / self.ki;
                }
                v
            } else {
                let error = target_rpm - actual_rpm;
                if error.abs() > 5.0 {
                    self.pi_error_sum = (self.pi_error_sum + error).clamp(-1000.0, 1000.0);
                }
                let v_pi = self.kp * error + self.ki * self.pi_error_sum;
                v_pi + self.v_start + kick_bonus
            };

            let max_increase = 0.4;
            let max_decrease = 0.02;
            let mut v_control = v_target;
            if v_target > self.last_v_control + max_increase {
                v_control = self.last_v_control + max_increase;
            }
            if v_target < self.last_v_control - max_decrease {
                v_control = self.last_v_control - max_decrease;
            }
            v_control = v_control.clamp(0.0, self.track_voltage);
            self.last_v_control = v_control;

            let mut duty = v_control / self.track_voltage;

            if step > 0 && step < 15 && self.cv_pwm_dither > 0 {
                let phase = (now_ms as u64) % 40;
                let base_amplitude = (self.cv_pwm_dither as f32 / 255.0) * 0.39;
                let fade_factor = 1.0 - (step as f32 / 15.0);
                let dither = base_amplitude * fade_factor;
                if phase < 20 {
                    duty += dither;
                } else {
                    duty -= dither;
                }
            }
            if !dir {
                duty = -duty;
            }
            self.current_duty = duty;
        }

        hal::set_duty(self.current_duty);

        self.status.applied_voltage = self.track_voltage * self.current_duty.abs();
        self.status.current = avg_current;
        self.status.estimated_rpm = actual_rpm;
        self.status.ripple_freq = ripple_freq;
        self.status.stalled = low_speed_stall || self.estimator.is_stalled();
        self.status.hardware_fault = hal::read_fault();
        self.status.is_moving = ripple_confirm;
        self.status.duty = self.current_duty;
        self.status.raw_adc = raw_max_adc;

        if now_ms - self.last_log_time >= 100 {
            self.last_log_time = now_ms;
            let zone = if step == 0 {
                0
            } else if !ripple_confirm && step < 20 {
                2
            } else {
                3
            };
            log::info!(
                target: "data",
                "[NIMRS_DATA] {{\"tgt\":{},\"cur\":{:.3},\"rpm\":{:.1},\"rip_ok\":{},\"zone\":{},\"v\":{:.2},\"ke\":{:.4},\"stall\":{}}}",
                step,
                avg_current,
                actual_rpm,
                if ripple_confirm { 1 } else { 0 },
                zone,
                self.status.applied_voltage,
                self.estimator.get_bemf_constant(),
                if self.status.stalled { 1 } else { 0 },
            );
        }
    }
}

pub fn start() {
    MOTOR_TASK.get_or_init(|| Mutex::new(MotorTaskInner::new()));

    unsafe {
        let mut task_handle: TaskHandle_t = core::ptr::null_mut();
        extern "C" fn task_entry(param: *mut core::ffi::c_void) {
            let mtx: &'static Mutex<MotorTaskInner> = unsafe { &*(param as *const Mutex<MotorTaskInner>) };
            let mut last_wake = unsafe { xTaskGetTickCount() };
            loop {
                unsafe { xTaskDelayUntil(&mut last_wake, 20) };
                if let Ok(mut inner) = mtx.lock() {
                    inner.process_tick();
                }
            }
        }
        static TASK_NAME: [u8; 10] = *b"MotorTask\0";
        let task_ptr = MOTOR_TASK.get().unwrap() as *const Mutex<MotorTaskInner> as *mut core::ffi::c_void;
        xTaskCreatePinnedToCore(
            Some(task_entry),
            TASK_NAME.as_ptr(),
            4096,
            task_ptr,
            10,
            &mut task_handle,
            1,
        );
    }
}

pub fn set_target_speed(speed_step: u8, forward: bool) {
    if let Some(task) = MOTOR_TASK.get() {
        if let Ok(mut inner) = task.lock() {
            if !inner.test_mode && inner.resistance_state == ResistanceState::Idle {
                inner.target_speed_step = speed_step;
                inner.target_direction = forward;
            }
        }
    }
}

pub fn reload_cvs() {
    if let Some(task) = MOTOR_TASK.get() {
        if let Ok(mut inner) = task.lock() {
            inner.estimator.set_motor_params(35.0, 5);
            inner.estimator.set_bemf_constant(0.015);
            inner.track_voltage = 14.0;
            inner.v_start = 0.0;
            inner.cv_stiction_kick = 0;
            inner.kp = 0.002;
            inner.ki = 0.0005;
            inner.cv_pwm_dither = 0;
            inner.max_rpm = 3000.0;
        }
    }
}

fn millis() -> u32 {
    unsafe { esp_idf_sys::esp_timer_get_time() as u32 / 1000 }
}
