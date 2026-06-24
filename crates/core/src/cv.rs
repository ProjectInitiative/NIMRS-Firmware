pub const ADDR_SHORT: u16 = 1;
pub const V_START: u16 = 2;
pub const ACCEL: u16 = 3;
pub const DECEL: u16 = 4;
pub const V_HIGH: u16 = 5;
pub const V_MID: u16 = 6;
pub const DECODER_VERSION: u16 = 7;
pub const DECODER_MAN_ID: u16 = 8;
pub const PWM_FREQ: u16 = 9;
pub const PWM_FREQ_H: u16 = 10;
pub const ADDR_LONG_MSB: u16 = 17;
pub const ADDR_LONG_LSB: u16 = 18;
pub const CONFIG: u16 = 29;

pub const MASTER_VOL: u16 = 50;
pub const AUDIO_MAP_BASE: u16 = 100;
pub const CHUFF_RATE: u16 = 133;
pub const CHUFF_DRAG: u16 = 134;

pub const LOAD_GAIN: u16 = 60;
pub const BASELINE_ALPHA: u16 = 61;
pub const STICTION_KICK: u16 = 62;
pub const DELTA_CAP: u16 = 63;
pub const PWM_DITHER: u16 = 64;
pub const BASELINE_RESET: u16 = 65;
pub const CURVE_INTENSITY: u16 = 66;

pub const MOTOR_KP: u16 = 112;
pub const MOTOR_KI: u16 = 114;
pub const MOTOR_KP_SLOW: u16 = 118;
pub const MOTOR_LOAD_FILTER: u16 = 189;

pub const DRIVE_MODE: u16 = 144;
pub const PEDESTAL_FLOOR: u16 = 57;
pub const LOAD_GAIN_SCALAR: u16 = 146;
pub const LEARN_THRESHOLD: u16 = 147;
pub const HARDWARE_GAIN: u16 = 148;

pub const MOTOR_POLES: u16 = 143;
pub const TRACK_VOLTAGE: u16 = 145;
pub const MOTOR_R_ARM: u16 = 149;
pub const MOTOR_KE: u16 = 150;
pub const SUPERCAP_ENABLE: u16 = 151;

pub const FRONT: u16 = 33;
pub const REAR: u16 = 34;
pub const AUX1: u16 = 35;
pub const AUX2: u16 = 36;
pub const AUX3: u16 = 37;
pub const AUX4: u16 = 38;
pub const AUX5: u16 = 39;
pub const AUX6: u16 = 40;
pub const AUX7: u16 = 41;
pub const AUX8: u16 = 42;

pub struct CvDef {
    pub id: u16,
    pub default_value: u8,
    pub name: &'static str,
    pub desc: &'static str,
}

pub static CV_DEFS: &[CvDef] = &[
    CvDef {
        id: 1,
        default_value: 3,
        name: "Primary Address",
        desc: "Short Address (1-127)",
    },
    CvDef {
        id: 2,
        default_value: 20,
        name: "Vstart",
        desc: "Starting Voltage (0-255).",
    },
    CvDef {
        id: 3,
        default_value: 4,
        name: "Acceleration",
        desc: "Momentum Delay (Rate)",
    },
    CvDef {
        id: 4,
        default_value: 2,
        name: "Deceleration",
        desc: "Momentum Delay (Rate)",
    },
    CvDef {
        id: 5,
        default_value: 255,
        name: "Vhigh",
        desc: "Max Voltage/Speed",
    },
    CvDef {
        id: 6,
        default_value: 128,
        name: "Vmid",
        desc: "Mid-range Speed Curve",
    },
    CvDef {
        id: 7,
        default_value: 14,
        name: "Version ID",
        desc: "Read-only Version",
    },
    CvDef {
        id: 8,
        default_value: 13,
        name: "Manufacturer",
        desc: "Read-only Man ID (DIY=13)",
    },
    CvDef {
        id: 9,
        default_value: 128,
        name: "PWM Freq Low",
        desc: "Freq Low Byte (Default 16kHz)",
    },
    CvDef {
        id: 10,
        default_value: 62,
        name: "PWM Freq High",
        desc: "Freq High Byte (Default 16kHz)",
    },
    CvDef {
        id: 17,
        default_value: 192,
        name: "Long Addr MSB",
        desc: "Upper byte of Long Address",
    },
    CvDef {
        id: 18,
        default_value: 3,
        name: "Long Addr LSB",
        desc: "Lower byte of Long Address",
    },
    CvDef {
        id: 29,
        default_value: 38,
        name: "Configuration",
        desc: "Bit 5=LongAddr, Bit 2=Analog",
    },
    CvDef {
        id: 50,
        default_value: 128,
        name: "Master Volume",
        desc: "Audio Volume (0-255)",
    },
    CvDef {
        id: 133,
        default_value: 10,
        name: "Chuff Rate",
        desc: "Sync Multiplier (PWM -> RPM)",
    },
    CvDef {
        id: 134,
        default_value: 5,
        name: "Chuff Load Drag",
        desc: "Current -> RPM Drag Factor",
    },
    CvDef {
        id: 60,
        default_value: 15,
        name: "Load Gain",
        desc: "Grade Comp Strength (0-255).",
    },
    CvDef {
        id: 61,
        default_value: 5,
        name: "Baseline Alpha",
        desc: "Learning Speed (0-255).",
    },
    CvDef {
        id: 62,
        default_value: 40,
        name: "Stiction Kick",
        desc: "Start Pulse Strength (0-255).",
    },
    CvDef {
        id: 63,
        default_value: 180,
        name: "Delta Cap",
        desc: "Max Boost Limit (0-255).",
    },
    CvDef {
        id: 64,
        default_value: 0,
        name: "PWM Dither",
        desc: "Vibration for Brushes (0-255).",
    },
    CvDef {
        id: 65,
        default_value: 0,
        name: "Baseline Cmd",
        desc: "1=Wipe, 2=Save Snapshot to Flash.",
    },
    CvDef {
        id: 66,
        default_value: 0,
        name: "Curve Intensity",
        desc: "Auto-generate S-Curve (0=Off, 1-255=Strength).",
    },
    CvDef {
        id: 112,
        default_value: 20,
        name: "Motor Kp",
        desc: "Proportional Gain",
    },
    CvDef {
        id: 114,
        default_value: 10,
        name: "Motor Ki",
        desc: "Integral Gain",
    },
    CvDef {
        id: 118,
        default_value: 128,
        name: "Slow Speed Gain",
        desc: "Torque Punch multiplier (CV118)",
    },
    CvDef {
        id: 189,
        default_value: 150,
        name: "Load Filter",
        desc: "Current sense smoothing (CV189)",
    },
    CvDef {
        id: 144,
        default_value: 1,
        name: "Drive Mode",
        desc: "0=Fast, 1=Slow Decay (Default 1).",
    },
    CvDef {
        id: 57,
        default_value: 80,
        name: "Pedestal Floor",
        desc: "Absolute min PWM floor (0-255).",
    },
    CvDef {
        id: 146,
        default_value: 20,
        name: "Load Scalar",
        desc: "Multiplier for CV60 (*10).",
    },
    CvDef {
        id: 147,
        default_value: 20,
        name: "Learn Thresh",
        desc: "Min speed to learn baseline (0-255).",
    },
    CvDef {
        id: 148,
        default_value: 1,
        name: "Hardware Gain",
        desc: "0=Low, 1=High-Z (Med), 2=High.",
    },
    CvDef {
        id: 149,
        default_value: 150,
        name: "Armature R",
        desc: "Armature Resistance in 200mOhm units (150=30.0 Ohm).",
    },
    CvDef {
        id: 150,
        default_value: 50,
        name: "Motor Ke",
        desc: "Back-EMF Constant (mV/RPM).",
    },
    CvDef {
        id: 151,
        default_value: 1,
        name: "SuperCap Enable",
        desc: "Enable Capacitor Pack (0=Off, 1=On).",
    },
    CvDef {
        id: 145,
        default_value: 140,
        name: "Track Voltage",
        desc: "Track Voltage in 100mV units (140=14.0V).",
    },
    CvDef {
        id: 143,
        default_value: 5,
        name: "Motor Poles",
        desc: "Number of motor poles (Default 5).",
    },
    CvDef {
        id: 101,
        default_value: 0,
        name: "Map: Sound ID 1",
        desc: "Function to trigger Sound 1 (0-28)",
    },
    CvDef {
        id: 102,
        default_value: 0,
        name: "Map: Sound ID 2",
        desc: "Function to trigger Sound 2 (0-28)",
    },
    CvDef {
        id: 103,
        default_value: 0,
        name: "Map: Sound ID 3",
        desc: "Function to trigger Sound 3 (0-28)",
    },
    CvDef {
        id: 104,
        default_value: 0,
        name: "Map: Sound ID 4",
        desc: "Function to trigger Sound 4 (0-28)",
    },
    CvDef {
        id: 110,
        default_value: 0,
        name: "Map: Sound ID 10",
        desc: "Function to trigger Sound 10 (0-28)",
    },
    CvDef {
        id: 111,
        default_value: 0,
        name: "Map: Sound ID 11",
        desc: "Function to trigger Sound 11 (0-28)",
    },
    CvDef {
        id: 33,
        default_value: 0,
        name: "Map: Front Light",
        desc: "Function to map to Front Light (0-28)",
    },
    CvDef {
        id: 34,
        default_value: 0,
        name: "Map: Rear Light",
        desc: "Function to map to Rear Light (0-28)",
    },
    CvDef {
        id: 35,
        default_value: 1,
        name: "Map: AUX 1",
        desc: "Function to map to AUX 1 (0-28)",
    },
    CvDef {
        id: 36,
        default_value: 2,
        name: "Map: AUX 2",
        desc: "Function to map to AUX 2 (0-28)",
    },
    CvDef {
        id: 37,
        default_value: 3,
        name: "Map: AUX 3",
        desc: "Function to map to AUX 3 (0-28)",
    },
    CvDef {
        id: 38,
        default_value: 4,
        name: "Map: AUX 4",
        desc: "Function to map to AUX 4 (0-28)",
    },
    CvDef {
        id: 39,
        default_value: 5,
        name: "Map: AUX 5",
        desc: "Function to map to AUX 5 (0-28)",
    },
    CvDef {
        id: 40,
        default_value: 6,
        name: "Map: AUX 6",
        desc: "Function to map to AUX 6 (0-28)",
    },
    CvDef {
        id: 41,
        default_value: 7,
        name: "Map: AUX 7",
        desc: "Function to map to AUX 7 (0-28)",
    },
    CvDef {
        id: 42,
        default_value: 8,
        name: "Map: AUX 8",
        desc: "Function to map to AUX 8 (0-28)",
    },
];

pub const CV_DEFS_COUNT: usize = CV_DEFS.len();

pub const fn cv_default(id: u16) -> u8 {
    let mut i = 0;
    while i < CV_DEFS.len() {
        if CV_DEFS[i].id == id {
            return CV_DEFS[i].default_value;
        }
        i += 1;
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cv_constants_match_cpp() {
        assert_eq!(ADDR_SHORT, 1);
        assert_eq!(V_START, 2);
        assert_eq!(ACCEL, 3);
        assert_eq!(DECEL, 4);
        assert_eq!(V_HIGH, 5);
        assert_eq!(V_MID, 6);
        assert_eq!(DECODER_VERSION, 7);
        assert_eq!(DECODER_MAN_ID, 8);
        assert_eq!(PWM_FREQ, 9);
        assert_eq!(PWM_FREQ_H, 10);
        assert_eq!(ADDR_LONG_MSB, 17);
        assert_eq!(ADDR_LONG_LSB, 18);
        assert_eq!(CONFIG, 29);
        assert_eq!(MASTER_VOL, 50);
        assert_eq!(AUDIO_MAP_BASE, 100);
        assert_eq!(CHUFF_RATE, 133);
        assert_eq!(CHUFF_DRAG, 134);
        assert_eq!(LOAD_GAIN, 60);
        assert_eq!(BASELINE_ALPHA, 61);
        assert_eq!(STICTION_KICK, 62);
        assert_eq!(DELTA_CAP, 63);
        assert_eq!(PWM_DITHER, 64);
        assert_eq!(BASELINE_RESET, 65);
        assert_eq!(CURVE_INTENSITY, 66);
        assert_eq!(MOTOR_KP, 112);
        assert_eq!(MOTOR_KI, 114);
        assert_eq!(MOTOR_KP_SLOW, 118);
        assert_eq!(MOTOR_LOAD_FILTER, 189);
        assert_eq!(DRIVE_MODE, 144);
        assert_eq!(PEDESTAL_FLOOR, 57);
        assert_eq!(LOAD_GAIN_SCALAR, 146);
        assert_eq!(LEARN_THRESHOLD, 147);
        assert_eq!(HARDWARE_GAIN, 148);
        assert_eq!(MOTOR_POLES, 143);
        assert_eq!(TRACK_VOLTAGE, 145);
        assert_eq!(MOTOR_R_ARM, 149);
        assert_eq!(MOTOR_KE, 150);
        assert_eq!(SUPERCAP_ENABLE, 151);
        assert_eq!(FRONT, 33);
        assert_eq!(REAR, 34);
        assert_eq!(AUX1, 35);
        assert_eq!(AUX2, 36);
        assert_eq!(AUX3, 37);
        assert_eq!(AUX4, 38);
        assert_eq!(AUX5, 39);
        assert_eq!(AUX6, 40);
        assert_eq!(AUX7, 41);
        assert_eq!(AUX8, 42);
    }

    #[test]
    fn test_cv_defs_count() {
        assert_eq!(CV_DEFS_COUNT, 53);
    }

    #[test]
    fn test_cv_defs_values() {
        assert_eq!(CV_DEFS[0].id, 1);
        assert_eq!(CV_DEFS[0].default_value, 3);
        assert_eq!(CV_DEFS[1].default_value, 20);
        assert_eq!(CV_DEFS[3].id, 4);
        assert_eq!(CV_DEFS[7].id, 8);
        assert_eq!(CV_DEFS[7].default_value, 13);
    }

    #[test]
    fn test_cv_default() {
        assert_eq!(cv_default(1), 3);
        assert_eq!(cv_default(8), 13);
        assert_eq!(cv_default(999), 0);
    }
}
