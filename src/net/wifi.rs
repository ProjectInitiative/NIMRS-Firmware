use esp_idf_sys::*;
use once_cell::sync::Lazy;
use std::sync::Mutex;

const CONNECT_TIMEOUT_MS: u32 = 10000;
const SCAN_MAX_APS: u16 = 20;

static CREDENTIALS: Lazy<Mutex<(String, String)>> = Lazy::new(|| {
    let (ssid, pass) = load_credentials();
    Mutex::new((ssid, pass))
});

#[derive(Clone, Copy, PartialEq)]
pub enum WifiState {
    StaConnecting,
    StaConnected,
    ApMode,
}

pub struct WifiManager {
    pub hostname: String,
    pub state: WifiState,
    connect_start: u32,
}

impl WifiManager {
    pub fn new(hostname: &str) -> Self {
        Self {
            hostname: hostname.to_string(),
            state: WifiState::StaConnecting,
            connect_start: millis(),
        }
    }

    pub fn begin_connect(&mut self) {
        self.connect_start = millis();
        self.state = WifiState::StaConnecting;
        unsafe {
            esp_netif_init();
            esp_event_loop_create_default();
            let mut cfg: wifi_init_config_t = core::mem::zeroed();
            cfg.wifi_task_core_id = 0;
            esp_wifi_init(&cfg);

            let (ref ssid, ref pass) = CREDENTIALS.lock().unwrap().clone();
            if !ssid.is_empty() {
                let mut sta_cfg: wifi_config_t = core::mem::zeroed();
                let ssid_bytes = ssid.as_bytes();
                let pass_bytes = pass.as_bytes();
                let len = ssid_bytes.len().min(32);
                sta_cfg.sta.ssid[..len].copy_from_slice(&ssid_bytes[..len]);
                let plen = pass_bytes.len().min(64);
                sta_cfg.sta.password[..plen].copy_from_slice(&pass_bytes[..plen]);
                esp_wifi_set_mode(1);
                esp_wifi_set_config(0, &mut sta_cfg);
                esp_wifi_start();
                log::info!("WiFi: connecting to {}", ssid);
            } else {
                esp_wifi_set_mode(1);
                esp_wifi_start();
                log::info!("WiFi: no credentials, will fall back to AP");
            }
        }
    }

    pub fn loop_once(&mut self) {
        if self.state == WifiState::StaConnecting {
            if millis() - self.connect_start > CONNECT_TIMEOUT_MS {
                log::info!("WiFi: timeout, starting AP");
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
        log::info!("WiFi: AP \"{}\" started", self.hostname);
    }

    pub fn set_connected(&mut self) {
        self.state = WifiState::StaConnected;
    }

    pub fn is_connected(&self) -> bool {
        self.state == WifiState::StaConnected
    }
}

pub fn is_connected() -> bool {
    // Simple check — will be improved with proper WiFi event handling
    false
}

pub fn scan_json() -> String {
    let mut json = String::from('[');
    unsafe {
        let mut cfg: wifi_scan_config_t = core::mem::zeroed();
        cfg.show_hidden = false;
        cfg.scan_type = 0; // WIFI_SCAN_TYPE_PASSIVE

        if esp_wifi_scan_start(&cfg, true) == ESP_OK as i32 {
            let mut count: u16 = 0;
            esp_wifi_scan_get_ap_num(&mut count);
            if count > SCAN_MAX_APS {
                count = SCAN_MAX_APS;
            }

            let mut records: Vec<wifi_ap_record_t> =
                vec![unsafe { core::mem::zeroed() }; count as usize];

            if esp_wifi_scan_get_ap_records(&mut count, records.as_mut_ptr()) == ESP_OK as i32 {
                for (i, ap) in records.iter().enumerate() {
                    if i > 0 {
                        json.push(',');
                    }
                    let ssid = std::ffi::CStr::from_ptr(ap.ssid.as_ptr()).to_string_lossy();
                    json.push_str(&format!(
                        r#"{{"ssid":"{}","rssi":{},"enc":{}}}"#,
                        ssid,
                        ap.rssi,
                        if ap.authmode != 0 { 1 } else { 0 } // WIFI_AUTH_OPEN = 0
                    ));
                }
            }
        }
    }
    json.push(']');
    json
}

pub fn save_credentials(ssid: &str, pass: &str) {
    if let Ok(mut creds) = CREDENTIALS.lock() {
        *creds = (ssid.to_string(), pass.to_string());
    }
    unsafe {
        let mut handle: nvs_handle_t = 0;
        if nvs_open(b"config\0".as_ptr(), 1, &mut handle) == ESP_OK as i32 {
            nvs_set_str(handle, b"wifi_ssid\0".as_ptr(), ssid.as_ptr());
            nvs_set_str(handle, b"wifi_pass\0".as_ptr(), pass.as_ptr());
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
}

pub fn reset_credentials() {
    if let Ok(mut creds) = CREDENTIALS.lock() {
        *creds = (String::new(), String::new());
    }
    unsafe {
        let mut handle: nvs_handle_t = 0;
        if nvs_open(b"config\0".as_ptr(), 1, &mut handle) == ESP_OK as i32 {
            nvs_erase_key(handle, b"wifi_ssid\0".as_ptr());
            nvs_erase_key(handle, b"wifi_pass\0".as_ptr());
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
    // Force WiFi disconnect
    unsafe {
        esp_wifi_disconnect();
        // Clear stored AP
        let mut cfg: wifi_config_t = core::mem::zeroed();
        esp_wifi_set_config(0, &mut cfg); // WIFI_IF_STA
    }
}

fn load_credentials() -> (String, String) {
    let mut ssid = String::new();
    let mut pass = String::new();
    unsafe {
        let mut handle: nvs_handle_t = 0;
        if nvs_open(b"config\0".as_ptr(), 0, &mut handle) == ESP_OK as i32 {
            let mut len: usize = 0;
            if nvs_get_str(
                handle,
                b"wifi_ssid\0".as_ptr(),
                core::ptr::null_mut(),
                &mut len,
            ) == ESP_OK as i32
                && len > 0
            {
                let mut buf = vec![0u8; len];
                nvs_get_str(handle, b"wifi_ssid\0".as_ptr(), buf.as_mut_ptr(), &mut len);
                ssid = String::from_utf8_lossy(&buf[..len - 1]).into_owned();
            }
            if nvs_get_str(
                handle,
                b"wifi_pass\0".as_ptr(),
                core::ptr::null_mut(),
                &mut len,
            ) == ESP_OK as i32
                && len > 0
            {
                let mut buf = vec![0u8; len];
                nvs_get_str(handle, b"wifi_pass\0".as_ptr(), buf.as_mut_ptr(), &mut len);
                pass = String::from_utf8_lossy(&buf[..len - 1]).into_owned();
            }
            nvs_close(handle);
        }
    }
    (ssid, pass)
}

fn millis() -> u32 {
    unsafe { esp_timer_get_time() as u32 / 1000 }
}
