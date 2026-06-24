use core::sync::atomic::{AtomicU32, Ordering};
use std::sync::OnceLock;

use esp_idf_sys::*;

use nimrs_core::pinout;

static MOTOR_HAL: OnceLock<MotorHalInner> = OnceLock::new();

const V_PER_STEP: f32 = 3.3 / 4095.0;

struct SafeMCPWM {
    timer: mcpwm_timer_handle_t,
    oper: mcpwm_oper_handle_t,
    gen_a: mcpwm_gen_handle_t,
    gen_b: mcpwm_gen_handle_t,
    cmpr_a: mcpwm_cmpr_handle_t,
    cmpr_b: mcpwm_cmpr_handle_t,
}

unsafe impl Send for SafeMCPWM {}
unsafe impl Sync for SafeMCPWM {}

struct MotorHalInner {
    mcpwm: SafeMCPWM,
    last_gain: AtomicU32,
    stream_buf: StreamBufferHandle_t,
    last_current_adc: AtomicU32,
}

unsafe impl Send for MotorHalInner {}
unsafe impl Sync for MotorHalInner {}

impl MotorHalInner {
    unsafe fn new() -> Self {
        let stream_buf = {
            extern "C" {
                fn xStreamBufferGenericCreate(
                    xBufferSizeBytes: usize,
                    xTriggerLevelBytes: usize,
                    xIsr: u8,
                ) -> StreamBufferHandle_t;
            }
            xStreamBufferGenericCreate(4096, core::mem::size_of::<f32>(), 0)
        };

        adc1_config_width(3); // ADC_WIDTH_BIT_12
        adc1_config_channel_atten(5, 3); // ADC1_CHANNEL_5, ADC_ATTEN_DB_12

        let mut tc: mcpwm_timer_config_t = core::mem::zeroed();
        tc.group_id = 0;
        tc.clk_src = 0; // MCPWM_TIMER_CLK_SRC_DEFAULT
        tc.resolution_hz = 1_000_000;
        tc.count_mode = mcpwm_timer_count_mode_t_MCPWM_TIMER_COUNT_MODE_UP_DOWN;
        tc.period_ticks = 25;

        let mut timer = core::ptr::null_mut();
        check(mcpwm_new_timer(&tc as *const _, &mut timer));

        let mut oc: mcpwm_operator_config_t = core::mem::zeroed();
        oc.group_id = 0;
        let mut oper = core::ptr::null_mut();
        check(mcpwm_new_operator(&oc as *const _, &mut oper));
        check(mcpwm_operator_connect_timer(oper, timer));

        let mut cc: mcpwm_comparator_config_t = core::mem::zeroed();
        cc.flags.set_update_cmp_on_tez(1);
        let mut ca = core::ptr::null_mut();
        let mut cb = core::ptr::null_mut();
        check(mcpwm_new_comparator(oper, &cc as *const _, &mut ca));
        check(mcpwm_new_comparator(oper, &cc as *const _, &mut cb));

        let mut gc: mcpwm_generator_config_t = core::mem::zeroed();
        gc.gen_gpio_num = pinout::MOTOR_IN1 as i32;
        let mut ga = core::ptr::null_mut();
        check(mcpwm_new_generator(oper, &gc as *const _, &mut ga));
        gc.gen_gpio_num = pinout::MOTOR_IN2 as i32;
        let mut gb = core::ptr::null_mut();
        check(mcpwm_new_generator(oper, &gc as *const _, &mut gb));

        let mut cbs: mcpwm_timer_event_callbacks_t = core::mem::zeroed();
        cbs.on_empty = Some(motor_hal_mcpwm_cb);
        check(mcpwm_timer_register_event_callbacks(
            timer,
            &cbs as *const _,
            core::ptr::null_mut(),
        ));
        check(mcpwm_timer_enable(timer));
        check(mcpwm_timer_start_stop(
            timer,
            mcpwm_timer_start_stop_cmd_t_MCPWM_TIMER_START_NO_STOP,
        ));

        let hal = Self {
            mcpwm: SafeMCPWM {
                timer,
                oper,
                gen_a: ga,
                gen_b: gb,
                cmpr_a: ca,
                cmpr_b: cb,
            },
            last_gain: AtomicU32::new(255),
            stream_buf,
            last_current_adc: AtomicU32::new(0),
        };

        gpio_set_direction(pinout::MOTOR_FAULT as i32, gpio_mode_t_GPIO_MODE_INPUT);
        gpio_set_pull_mode(pinout::MOTOR_FAULT as i32, gpio_pull_mode_t_GPIO_PULLUP_ONLY);

        hal.set_hardware_gain(1);
        hal.set_duty(0.0);

        hal
    }

    fn set_duty(&self, duty: f32) {
        let p = &self.mcpwm;
        let dp = duty.clamp(-1.0, 1.0);
        let cv = (dp.abs() * 25.0) as u32;

        unsafe {
            if dp.abs() < 0.01 {
                let ah = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                let ahd = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                for g in [p.gen_a, p.gen_b] {
                    mcpwm_generator_set_action_on_timer_event(g, ah);
                    mcpwm_generator_set_action_on_timer_event(g, ahd);
                }
            } else if dp > 0.0 {
                mcpwm_comparator_set_compare_value(p.cmpr_a, cv);
                let al = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let ald = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                mcpwm_generator_set_action_on_timer_event(p.gen_b, al);
                mcpwm_generator_set_action_on_timer_event(p.gen_b, ald);

                let ah = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_timer_event(p.gen_a, ah);

                let cup = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    comparator: p.cmpr_a,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let cdown = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    comparator: p.cmpr_a,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_compare_event(p.gen_a, cup);
                mcpwm_generator_set_action_on_compare_event(p.gen_a, cdown);
            } else {
                mcpwm_comparator_set_compare_value(p.cmpr_b, cv);
                let al = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let ald = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                mcpwm_generator_set_action_on_timer_event(p.gen_a, al);
                mcpwm_generator_set_action_on_timer_event(p.gen_a, ald);

                let ah = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_timer_event(p.gen_b, ah);

                let cup = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    comparator: p.cmpr_b,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let cdown = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    comparator: p.cmpr_b,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_compare_event(p.gen_b, cup);
                mcpwm_generator_set_action_on_compare_event(p.gen_b, cdown);
            }
        }
    }

    fn set_hardware_gain(&self, mode: u8) {
        unsafe {
            if self.last_gain.swap(mode as u32, Ordering::Relaxed) == mode as u32 {
                return;
            }
            match mode {
                0 => {
                    gpio_set_direction(pinout::MOTOR_GAIN_SEL as i32, gpio_mode_t_GPIO_MODE_OUTPUT);
                    gpio_set_level(pinout::MOTOR_GAIN_SEL as i32, 0);
                }
                1 => {
                    gpio_set_direction(pinout::MOTOR_GAIN_SEL as i32, gpio_mode_t_GPIO_MODE_INPUT);
                    gpio_set_pull_mode(pinout::MOTOR_GAIN_SEL as i32, gpio_pull_mode_t_GPIO_FLOATING);
                }
                2 => {
                    gpio_set_direction(pinout::MOTOR_GAIN_SEL as i32, gpio_mode_t_GPIO_MODE_OUTPUT);
                    gpio_set_level(pinout::MOTOR_GAIN_SEL as i32, 1);
                }
                _ => {}
            }
        }
    }

    fn read_fault(&self) -> bool {
        unsafe { gpio_get_level(pinout::MOTOR_FAULT as i32) == 0 }
    }

    fn get_current_scalar(&self) -> f32 {
        match self.last_gain.load(Ordering::Relaxed) {
            0 => V_PER_STEP / 0.492,
            1 => V_PER_STEP / 2.520,
            2 => V_PER_STEP / 11.760,
            _ => V_PER_STEP / 2.520,
        }
    }

    fn get_adc_samples(&self, buffer: &mut [f32]) -> usize {
        if self.stream_buf.is_null() {
            return 0;
        }
        let nbytes = buffer.len() * core::mem::size_of::<f32>();
        unsafe {
            let received = xStreamBufferReceive(
                self.stream_buf,
                buffer.as_mut_ptr() as *mut core::ffi::c_void,
                nbytes,
                0,
            );
            received / core::mem::size_of::<f32>()
    }
}

pub fn init() {
    MOTOR_HAL.get_or_init(|| unsafe { MotorHalInner::new() });
}

pub fn set_duty(duty: f32) {
    if let Some(h) = MOTOR_HAL.get() {
        h.set_duty(duty);
    }
}

pub fn read_fault() -> bool {
    MOTOR_HAL.get().map_or(false, |h| h.read_fault())
}

pub fn get_current_scalar() -> f32 {
    MOTOR_HAL.get().map_or(0.0, |h| h.get_current_scalar())
}

pub fn get_adc_samples(buffer: &mut [f32]) -> usize {
    MOTOR_HAL.get().map_or(0, |h| h.get_adc_samples(buffer))
}

pub fn get_adc_sample_rate() -> f32 {
    20_000.0
}

unsafe extern "C" fn motor_hal_mcpwm_cb(
    _timer: mcpwm_timer_handle_t,
    edata: *const mcpwm_timer_event_data_t,
    _user_ctx: *mut core::ffi::c_void,
) -> bool {
    if let Some(ed) = unsafe { edata.as_ref() } {
        if ed.count_value == 0 {
            let raw = unsafe { adc1_get_raw(5) }; // ADC1_CHANNEL_5
            let val = raw as f32;
            if let Some(h) = MOTOR_HAL.get() {
                h.last_current_adc
                    .store(val.to_bits(), Ordering::Relaxed);
                if !h.stream_buf.is_null() {
                    unsafe {
                        let mut wakeup: i32 = 0;
                        xStreamBufferSendFromISR(
                            h.stream_buf,
                            &val as *const f32 as *const core::ffi::c_void,
                            core::mem::size_of::<f32>(),
                            &mut wakeup as *mut i32,
                        );
                    }
                }
            }
        }
    }
    false
}

fn check(ret: i32) {
    if ret != ESP_OK as i32 {
        panic!("ESP-IDF error: {}", ret);
    }
}
