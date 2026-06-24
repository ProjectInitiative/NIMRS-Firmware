use esp_idf_sys::*;
use nimrs_core::context::{ControlSource, SYSTEM_STATE};
use nimrs_core::cv;
use nimrs_core::pinout;

use crate::boot;
use crate::motor::hal;

extern "C" {
    fn dcc_init(pin: u8, mfr: u8, ver: u8, flags: u8);
    fn dcc_process() -> u8;
    fn dcc_get_cv(cv: u16) -> u8;
    fn dcc_set_cv(cv: u16, value: u8) -> u8;
    fn dcc_get_addr() -> u16;
}

pub fn setup() {
    unsafe {
        gpio_reset_pin(pinout::SUPERCAP_CTRL as i32);
        gpio_set_direction(pinout::SUPERCAP_CTRL as i32, gpio_mode_t_GPIO_MODE_OUTPUT);
        gpio_set_drive_capability(
            pinout::SUPERCAP_CTRL as i32,
            gpio_drive_cap_t_GPIO_DRIVE_CAP_3,
        );

        let sc_enable = dcc_get_cv(cv::SUPERCAP_ENABLE);
        gpio_set_level(
            pinout::SUPERCAP_CTRL as i32,
            if sc_enable > 0 { 0 } else { 1 },
        );

        dcc_init(pinout::TRACK_LEFT_3V3, 13, 10, 0x02);
        log::info!("DCC: Listening on pin {}", pinout::TRACK_LEFT_3V3);
    }
}

pub fn loop_once() {
    unsafe {
        dcc_process();
    }
}

#[no_mangle]
pub extern "C" fn notifyDccSpeed(addr: u16, _addr_type: u8, speed: u8, dir: u8, _steps: u8) {
    let direction = dir != 0;
    let target_speed = if speed > 1 { speed } else { 0 };
    let mut state = SYSTEM_STATE.lock().unwrap();

    let dcc_delta = (target_speed as i16 - state.last_dcc_speed as i16).abs();
    let is_dcc_internal_change = dcc_delta > 2 || state.last_dcc_direction != direction;

    if state.speed_source == ControlSource::Dcc || is_dcc_internal_change {
        state.speed = target_speed;
        state.direction = direction;
        state.speed_source = ControlSource::Dcc;
        state.dcc_address = addr;
    }

    state.last_dcc_speed = target_speed;
    state.last_dcc_direction = direction;
    state.last_dcc_packet_time = millis();
}

#[no_mangle]
pub extern "C" fn notifyDccFunc(_addr: u16, _addr_type: u8, func_grp: u8, func_state: u8) {
    let base_index: u8 = match func_grp {
        0 => 0,  // FN_0_4
        1 => 5,  // FN_5_8
        2 => 9,  // FN_9_12
        3 => 13, // FN_13_20
        4 => 21, // FN_21_28
        _ => return,
    };

    let mut state = SYSTEM_STATE.lock().unwrap();
    if func_grp == 0 {
        state.functions[0] = (func_state & 0x10) != 0; // FN_BIT_00
        state.functions[1] = (func_state & 0x01) != 0; // FN_BIT_01
        state.functions[2] = (func_state & 0x02) != 0; // FN_BIT_02
        state.functions[3] = (func_state & 0x04) != 0; // FN_BIT_03
        state.functions[4] = (func_state & 0x08) != 0; // FN_BIT_04
    } else {
        for i in 0..8 {
            state.functions[base_index as usize + i] = ((func_state >> i) & 0x01) != 0;
        }
    }
}

#[no_mangle]
pub extern "C" fn notifyCVWrite(cv: u16, value: u8) -> u8 {
    if cv == 8 {
        if millis() < 3000 {
            return value;
        }
        log::info!("DCC: CV8 factory reset");
        crate::boot::perform_factory_reset();
        return value;
    }

    if cv == cv::SUPERCAP_ENABLE {
        unsafe {
            gpio_set_level(pinout::SUPERCAP_CTRL as i32, if value > 0 { 0 } else { 1 });
        }
    }

    log::info!("DCC: Write CV{} = {}", cv, value);
    value
}

#[no_mangle]
pub extern "C" fn notifyCVAck() {
    unsafe {
        gpio_set_level(pinout::SUPERCAP_CTRL as i32, 1);
    }
    hal::set_duty(0.2);
    std::thread::sleep(std::time::Duration::from_millis(6));
    hal::set_duty(0.0);

    let sc_enable = unsafe { dcc_get_cv(cv::SUPERCAP_ENABLE) };
    unsafe {
        gpio_set_level(
            pinout::SUPERCAP_CTRL as i32,
            if sc_enable > 0 { 0 } else { 1 },
        );
    }
}

#[no_mangle]
pub extern "C" fn notifyCVResetFactoryDefault() {
    log::info!("DCC: Factory Reset");
    for def in nimrs_core::cv::CV_DEFS {
        if def.id != 8 {
            unsafe {
                dcc_set_cv(def.id, def.default_value);
            }
        }
    }
}

fn millis() -> u32 {
    unsafe { esp_timer_get_time() as u32 / 1000 }
}
