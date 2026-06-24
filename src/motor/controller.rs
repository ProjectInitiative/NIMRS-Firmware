use std::sync::OnceLock;

use super::task;

static MOTOR_CONTROLLER: OnceLock<MotorController> = OnceLock::new();

pub struct MotorController {
    cv_accel: u8,
    cv_decel: u8,
    current_speed: f32,
    last_momentum_update: u32,
    last_cv_update: u32,
}

impl MotorController {
    pub fn init() -> &'static Self {
        MOTOR_CONTROLLER.get_or_init(|| Self {
            cv_accel: 4,
            cv_decel: 4,
            current_speed: 0.0,
            last_momentum_update: 0,
            last_cv_update: 0,
        })
    }

    pub fn setup() {
        Self::init();
        task::start();
        log::info!("NIMRS: Hybrid Motor Control (MotorTask)");
    }

    pub fn loop_once(&mut self) {
        let target_speed = 0;
        let direction = true;

        self.update_cv_cache();

        let now = millis();
        let dt = now - self.last_momentum_update;
        if dt >= 10 {
            self.last_momentum_update = now;

            let accel_delay = (self.cv_accel as i32).max(1) as f32 * 5.0;
            let step = dt as f32 / accel_delay;

            let target = target_speed as f32;
            if target > self.current_speed {
                self.current_speed += step;
                if self.current_speed > target {
                    self.current_speed = target;
                }
            } else if target < self.current_speed {
                self.current_speed -= step;
                if self.current_speed < target {
                    self.current_speed = target;
                }
            }
        }

        task::set_target_speed(self.current_speed as u8, direction);
        self.stream_telemetry();
    }

    fn stream_telemetry(&self) {
        let _now = millis();
        let pwm = (self.current_speed.abs() * 1023.0) as i32;
        log::info!(
            target: "data",
            "[NIMRS_DATA],{},{:.1},{},{:.3},{:.3},{},{}",
            0,
            self.current_speed,
            pwm,
            0.0f32,
            0.0f32,
            0u32,
            0.0f32,
        );
    }

    pub fn stop_immediate() {
        if let Some(mc) = MOTOR_CONTROLLER.get() {
            unsafe {
                let ptr = mc as *const MotorController as *mut MotorController;
                (*ptr).current_speed = 0.0;
            }
        }
        task::set_target_speed(0, true);
        log::info!("MotorController: Emergency STOP (Bypass Momentum)");
    }

    fn update_cv_cache(&mut self) {
        let now = millis();
        if now - self.last_cv_update > 500 {
            self.last_cv_update = now;
            self.cv_accel = 4;
            task::reload_cvs();
        }
    }
}

fn millis() -> u32 {
    unsafe { esp_idf_sys::esp_timer_get_time() as u32 / 1000 }
}
