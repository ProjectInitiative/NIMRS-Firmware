use esp_idf_sys::*;

const CONNECT_TIMEOUT_MS: u32 = 10000;

#[derive(Clone, Copy, PartialEq)]
pub enum WifiState {
    StaConnecting,
    StaConnected,
    ApMode,
}

pub struct WifiManager {
    pub hostname: String,
    pub state: WifiState,
    pub ip: String,
    connect_start: u32,
}

impl WifiManager {
    pub fn new(hostname: &str) -> Self {
        Self {
            hostname: hostname.to_string(),
            state: WifiState::StaConnecting,
            ip: String::new(),
            connect_start: millis(),
        }
    }

    pub fn begin_connect(&mut self) {
        self.connect_start = millis();
        self.state = WifiState::StaConnecting;
        unsafe {
            esp_netif_init();
            esp_event_loop_create_default();
            esp_wifi_start();
        }
    }

    pub fn loop_once(&mut self) {
        if self.state == WifiState::StaConnecting {
            if millis() - self.connect_start > CONNECT_TIMEOUT_MS {
                log::info!("WiFi: connection timeout, starting AP");
                self.start_ap();
            }
        }
    }

    fn start_ap(&mut self) {
        unsafe {
            esp_wifi_stop();
            esp_wifi_set_mode(2); // WIFI_MODE_AP
            esp_wifi_start();
        }
        self.state = WifiState::ApMode;
        self.ip = String::from("192.168.4.1");
        log::info!("WiFi: AP \"{}\" started", self.hostname);
    }
}

fn millis() -> u32 {
    unsafe { esp_timer_get_time() as u32 / 1000 }
}
