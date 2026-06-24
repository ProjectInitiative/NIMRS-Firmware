use std::sync::Mutex;

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum ControlSource {
    Dcc,
    Web,
}

impl Default for ControlSource {
    fn default() -> Self {
        Self::Dcc
    }
}

#[derive(Clone, Copy, Debug)]
pub struct SystemState {
    pub dcc_address: u16,
    pub speed: u8,
    pub direction: bool,
    pub functions: [bool; 29],
    pub speed_source: ControlSource,
    pub last_dcc_speed: u8,
    pub last_dcc_direction: bool,
    pub wifi_connected: bool,
    pub last_dcc_packet_time: u32,
    pub load_factor: f32,
}

impl Default for SystemState {
    fn default() -> Self {
        Self {
            dcc_address: 3,
            speed: 0,
            direction: true,
            functions: [false; 29],
            speed_source: ControlSource::Dcc,
            last_dcc_speed: 0,
            last_dcc_direction: true,
            wifi_connected: false,
            last_dcc_packet_time: 0,
            load_factor: 0.0,
        }
    }
}

use once_cell::sync::Lazy;

pub static SYSTEM_STATE: Lazy<Mutex<SystemState>> =
    Lazy::new(|| Mutex::new(SystemState::default()));

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_state() {
        let state = SystemState::default();
        assert_eq!(state.dcc_address, 3);
        assert_eq!(state.speed, 0);
        assert!(state.direction);
        assert_eq!(state.functions.len(), 29);
        for &f in state.functions.iter() {
            assert!(!f);
        }
        assert_eq!(state.speed_source, ControlSource::Dcc);
        assert!(!state.wifi_connected);
    }

    #[test]
    fn test_system_state_mutex() {
        {
            let mut state = SYSTEM_STATE.lock().unwrap();
            state.speed = 42;
            state.dcc_address = 100;
        }
        {
            let state = SYSTEM_STATE.lock().unwrap();
            assert_eq!(state.speed, 42);
            assert_eq!(state.dcc_address, 100);
        }
    }
}
