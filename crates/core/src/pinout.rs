pub const TRACK_RIGHT_3V3: u8 = 0;
pub const TRACK_LEFT_3V3: u8 = 1;

pub const MOTOR_IN1: u8 = 41;
pub const MOTOR_IN2: u8 = 40;
pub const MOTOR_GAIN_SEL: u8 = 34;
pub const VMOTOR_PG: u8 = 39;
pub const MOTOR_FAULT: u8 = 39;

pub const LIGHT_FRONT: u8 = 13;
pub const LIGHT_REAR: u8 = 11;

pub const AUX1: u8 = 9;
pub const AUX2: u8 = 10;
pub const AUX3: u8 = 35;
pub const AUX4: u8 = 14;
pub const AUX5: u8 = 17;
pub const AUX6: u8 = 12;

pub const INPUT1_AUX7: u8 = 7;
pub const INPUT2_AUX8: u8 = 8;

pub const MOTOR_CURRENT: u8 = 5;
pub const V_SENSE_3V3: u8 = 6;

pub const AMP_BCLK: u8 = 38;
pub const AMP_DIN: u8 = 37;
pub const AMP_LRCLK: u8 = 36;
pub const AMP_SD_MODE: u8 = 33;

pub const SUPERCAP_CTRL: u8 = 19;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_pin_constants() {
        assert_eq!(TRACK_RIGHT_3V3, 0);
        assert_eq!(TRACK_LEFT_3V3, 1);
        assert_eq!(MOTOR_IN1, 41);
        assert_eq!(MOTOR_IN2, 40);
        assert_eq!(MOTOR_GAIN_SEL, 34);
        assert_eq!(LIGHT_FRONT, 13);
        assert_eq!(LIGHT_REAR, 11);
        assert_eq!(MOTOR_CURRENT, 5);
        assert_eq!(AMP_BCLK, 38);
        assert_eq!(AMP_DIN, 37);
        assert_eq!(AMP_LRCLK, 36);
        assert_eq!(AMP_SD_MODE, 33);
        assert_eq!(SUPERCAP_CTRL, 19);
        assert_eq!(AUX1, 9);
        assert_eq!(INPUT2_AUX8, 8);
    }
}
