use esp_idf_sys::*;
use nimrs_core::pinout;

const INVERT_OUTPUTS: bool = false;

pub struct LightingController;

impl LightingController {
    pub fn setup() {
        unsafe {
            let pins = [
                pinout::LIGHT_FRONT,
                pinout::LIGHT_REAR,
                pinout::AUX1,
                pinout::AUX2,
                pinout::AUX3,
                pinout::AUX4,
                pinout::AUX5,
                pinout::AUX6,
                pinout::INPUT1_AUX7,
                pinout::INPUT2_AUX8,
            ];
            let off_val = if INVERT_OUTPUTS { 1 } else { 0 };
            for &pin in &pins {
                gpio_set_direction(pin as i32, gpio_mode_t_GPIO_MODE_OUTPUT);
                gpio_set_level(pin as i32, off_val);
            }
        }
        log::info!("OutputController: Ready.");
    }

    pub fn run_loop() {
        let functions = [false; 29];
        let direction = true;

        let drive_output = |_name: &str, pin: u8, f_map: u8, is_front: bool, is_rear: bool| {
            let active = if f_map < 29 {
                if f_map == 0 {
                    if functions[0] {
                        if is_front {
                            direction
                        } else if is_rear {
                            !direction
                        } else {
                            true
                        }
                    } else {
                        false
                    }
                } else {
                    functions[f_map as usize]
                }
            } else {
                false
            };

            let phys_val = if active {
                if INVERT_OUTPUTS {
                    0
                } else {
                    1
                }
            } else {
                if INVERT_OUTPUTS {
                    1
                } else {
                    0
                }
            };
            unsafe {
                gpio_set_level(pin as i32, phys_val);
            }
        };

        drive_output("FRONT", pinout::LIGHT_FRONT, 0, true, false);
        drive_output("REAR", pinout::LIGHT_REAR, 0, false, true);
        drive_output("AUX1", pinout::AUX1, 1, false, false);
        drive_output("AUX2", pinout::AUX2, 2, false, false);
        drive_output("AUX3", pinout::AUX3, 3, false, false);
        drive_output("AUX4", pinout::AUX4, 4, false, false);
        drive_output("AUX5", pinout::AUX5, 5, false, false);
        drive_output("AUX6", pinout::AUX6, 6, false, false);
        drive_output("AUX7", pinout::INPUT1_AUX7, 7, false, false);
        drive_output("AUX8", pinout::INPUT2_AUX8, 8, false, false);
    }
}
