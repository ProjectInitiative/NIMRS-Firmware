use std::sync::OnceLock;
use esp_idf_sys::*;

use nimrs_core::pinout;

static MOTOR_HAL: OnceLock<MotorHal> = OnceLock::new();

const V_PER_STEP: f32 = 3.3 / 4095.0;

pub struct MotorHal {
    timer: mcpwm_timer_handle_t,
    oper: mcpwm_oper_handle_t,
    gen_a: mcpwm_gen_handle_t,
    gen_b: mcpwm_gen_handle_t,
    cmpr_a: mcpwm_cmpr_handle_t,
    cmpr_b: mcpwm_cmpr_handle_t,
    last_gain: u8,
    current_duty: f32,
    adc_stream_buffer: StreamBufferHandle_t,
    last_current_adc: core::sync::atomic::AtomicF32,
}

impl MotorHal {
    pub fn init() {
        MOTOR_HAL.get_or_init(|| {
            let hal = unsafe { Self::new() };
            hal
        });
    }

    unsafe fn new() -> Self {
        let adc_stream_buffer = xStreamBufferCreate(4096, core::mem::size_of::<f32>() as u32);

        adc1_config_width(adc1_bits_width_t_ADC_WIDTH_BIT_12);
        adc1_config_channel_atten(
            adc1_channel_t_ADC1_CHANNEL_5,
            adc_atten_t_ADC_ATTEN_DB_12,
        );

        let mut timer_config: mcpwm_timer_config_t = core::mem::zeroed();
        timer_config.group_id = 0;
        timer_config.clk_src = mcpwm_timer_clock_source_t_MCPWM_TIMER_CLK_SRC_DEFAULT;
        timer_config.resolution_hz = 1_000_000;
        timer_config.count_mode = mcpwm_timer_count_mode_t_MCPWM_TIMER_COUNT_MODE_UP_DOWN;
        timer_config.period_ticks = 25;

        let mut timer: mcpwm_timer_handle_t = core::ptr::null_mut();
        esp_err_check(mcpwm_new_timer(&timer_config as *const _, &mut timer as *mut _));

        let mut oper_config: mcpwm_operator_config_t = core::mem::zeroed();
        oper_config.group_id = 0;
        let mut oper: mcpwm_oper_handle_t = core::ptr::null_mut();
        esp_err_check(mcpwm_new_operator(&oper_config as *const _, &mut oper as *mut _));
        esp_err_check(mcpwm_operator_connect_timer(oper, timer));

        let mut cmpr_config: mcpwm_comparator_config_t = core::mem::zeroed();
        cmpr_config.flags.update_cmp_on_tez = 1;
        let mut cmpr_a: mcpwm_cmpr_handle_t = core::ptr::null_mut();
        let mut cmpr_b: mcpwm_cmpr_handle_t = core::ptr::null_mut();
        esp_err_check(mcpwm_new_comparator(oper, &cmpr_config as *const _, &mut cmpr_a as *mut _));
        esp_err_check(mcpwm_new_comparator(oper, &cmpr_config as *const _, &mut cmpr_b as *mut _));

        let mut gen_config: mcpwm_generator_config_t = core::mem::zeroed();
        gen_config.gen_gpio_num = pinout::MOTOR_IN1 as i32;
        let mut gen_a: mcpwm_gen_handle_t = core::ptr::null_mut();
        esp_err_check(mcpwm_new_generator(oper, &gen_config as *const _, &mut gen_a as *mut _));
        gen_config.gen_gpio_num = pinout::MOTOR_IN2 as i32;
        let mut gen_b: mcpwm_gen_handle_t = core::ptr::null_mut();
        esp_err_check(mcpwm_new_generator(oper, &gen_config as *const _, &mut gen_b as *mut _));

        let mut cbs: mcpwm_timer_event_callbacks_t = core::mem::zeroed();
        cbs.on_empty = Some(motor_hal_mcpwm_cb);
        esp_err_check(mcpwm_timer_register_event_callbacks(
            timer,
            &cbs as *const _,
            core::ptr::null_mut(),
        ));
        esp_err_check(mcpwm_timer_enable(timer));
        esp_err_check(mcpwm_timer_start_stop(
            timer,
            mcpwm_timer_start_stop_cmd_t_MCPWM_TIMER_START_NO_STOP,
        ));

        let hal = Self {
            timer,
            oper,
            gen_a,
            gen_b,
            cmpr_a,
            cmpr_b,
            last_gain: 255,
            current_duty: 0.0,
            adc_stream_buffer,
            last_current_adc: core::sync::atomic::AtomicF32::new(0.0),
        };

        gpio_set_direction(pinout::MOTOR_FAULT as i32, gpio_mode_t_GPIO_MODE_INPUT);
        gpio_set_pull_mode(pinout::MOTOR_FAULT as i32, gpio_pull_mode_t_GPIO_PULLUP_ONLY);

        hal.set_hardware_gain(1);
        hal.set_duty(0.0);

        hal
    }

    pub fn get_instance() -> &'static Self {
        MOTOR_HAL.get().expect("MotorHal not initialized")
    }

    pub fn set_duty(&self, duty: f32) {
        let duty_percent = duty.clamp(-1.0, 1.0);
        let compare_val = (duty_percent.abs() * 25.0) as u32;

        unsafe {
            if duty_percent.abs() < 0.01 {
                let action_high = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                let action_high_down = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_timer_event(self.gen_a, &action_high as *const _);
                mcpwm_generator_set_action_on_timer_event(
                    self.gen_a,
                    &action_high_down as *const _,
                );
                mcpwm_generator_set_action_on_timer_event(self.gen_b, &action_high as *const _);
                mcpwm_generator_set_action_on_timer_event(
                    self.gen_b,
                    &action_high_down as *const _,
                );
            } else if duty_percent > 0.0 {
                mcpwm_comparator_set_compare_value(self.cmpr_a, compare_val);

                let action_low = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let action_low_down = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                mcpwm_generator_set_action_on_timer_event(self.gen_b, &action_low as *const _);
                mcpwm_generator_set_action_on_timer_event(
                    self.gen_b,
                    &action_low_down as *const _,
                );

                let action_high = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_timer_event(self.gen_a, &action_high as *const _);

                let cmp_up = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    comparator: self.cmpr_a,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let cmp_down = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    comparator: self.cmpr_a,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_compare_event(
                    self.gen_a,
                    &cmp_up as *const _,
                );
                mcpwm_generator_set_action_on_compare_event(
                    self.gen_a,
                    &cmp_down as *const _,
                );
            } else {
                mcpwm_comparator_set_compare_value(self.cmpr_b, compare_val);

                let action_low = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let action_low_down = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                mcpwm_generator_set_action_on_timer_event(self.gen_a, &action_low as *const _);
                mcpwm_generator_set_action_on_timer_event(
                    self.gen_a,
                    &action_low_down as *const _,
                );

                let action_high = mcpwm_gen_timer_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    event: mcpwm_timer_event_t_MCPWM_TIMER_EVENT_EMPTY,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_timer_event(self.gen_b, &action_high as *const _);

                let cmp_up = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_UP,
                    comparator: self.cmpr_b,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_LOW,
                };
                let cmp_down = mcpwm_gen_compare_event_action_t {
                    direction: mcpwm_timer_direction_t_MCPWM_TIMER_DIRECTION_DOWN,
                    comparator: self.cmpr_b,
                    action: mcpwm_generator_action_t_MCPWM_GEN_ACTION_HIGH,
                };
                mcpwm_generator_set_action_on_compare_event(
                    self.gen_b,
                    &cmp_up as *const _,
                );
                mcpwm_generator_set_action_on_compare_event(
                    self.gen_b,
                    &cmp_down as *const _,
                );
            }
        }
    }

    pub fn set_hardware_gain(&self, mode: u8) {
        if self.last_gain == mode {
            return;
        }
        unsafe {
            match mode {
                0 => {
                    gpio_set_direction(
                        pinout::MOTOR_GAIN_SEL as i32,
                        gpio_mode_t_GPIO_MODE_OUTPUT,
                    );
                    gpio_set_level(pinout::MOTOR_GAIN_SEL as i32, 0);
                }
                1 => {
                    gpio_set_direction(
                        pinout::MOTOR_GAIN_SEL as i32,
                        gpio_mode_t_GPIO_MODE_INPUT,
                    );
                    gpio_set_pull_mode(
                        pinout::MOTOR_GAIN_SEL as i32,
                        gpio_pull_mode_t_GPIO_FLOATING,
                    );
                }
                2 => {
                    gpio_set_direction(
                        pinout::MOTOR_GAIN_SEL as i32,
                        gpio_mode_t_GPIO_MODE_OUTPUT,
                    );
                    gpio_set_level(pinout::MOTOR_GAIN_SEL as i32, 1);
                }
                _ => {}
            }
        }
    }

    pub fn read_fault(&self) -> bool {
        unsafe { gpio_get_level(pinout::MOTOR_FAULT as i32) == 0 }
    }

    pub fn get_current_scalar(&self) -> f32 {
        match self.last_gain {
            0 => V_PER_STEP / 0.492,
            1 => V_PER_STEP / 2.520,
            2 => V_PER_STEP / 11.760,
            _ => V_PER_STEP / 2.520,
        }
    }

    pub fn get_latest_current_adc(&self) -> f32 {
        self.last_current_adc.load(core::sync::atomic::Ordering::Relaxed)
    }

    pub fn get_adc_samples(&self, buffer: &mut [f32]) -> usize {
        if self.adc_stream_buffer.is_null() {
            return 0;
        }
        unsafe {
            let bytes = xStreamBufferReceive(
                self.adc_stream_buffer,
                buffer.as_mut_ptr() as *mut core::ffi::c_void,
                (buffer.len() * core::mem::size_of::<f32>()) as u32,
                0,
            );
            (bytes / core::mem::size_of::<f32>() as u32) as usize
        }
    }

    pub fn get_adc_sample_rate(&self) -> f32 {
        20_000.0
    }
}

unsafe extern "C" fn motor_hal_mcpwm_cb(
    _timer: mcpwm_timer_handle_t,
    edata: *const mcpwm_timer_event_data_t,
    _user_ctx: *mut core::ffi::c_void,
) -> bool {
    if let Some(edata) = unsafe { edata.as_ref() } {
        if edata.count_value == 0 {
            let raw = unsafe { adc1_get_raw(adc1_channel_t_ADC1_CHANNEL_5) };
            let val = raw as f32;
            if let Some(hal) = MOTOR_HAL.get() {
                hal.last_current_adc
                    .store(val, core::sync::atomic::Ordering::Relaxed);
                if !hal.adc_stream_buffer.is_null() {
                    unsafe {
                        let mut wakeup: isize = 0;
                        xStreamBufferSendFromISR(
                            hal.adc_stream_buffer,
                            &val as *const f32 as *const core::ffi::c_void,
                            core::mem::size_of::<f32>() as u32,
                            &mut wakeup as *mut isize,
                        );
                    }
                }
            }
        }
    }
    false
}

fn esp_err_check(ret: i32) {
    if ret != esp_idf_sys::ESP_OK as i32 {
        panic!("ESP-IDF error: {}", ret);
    }
}
