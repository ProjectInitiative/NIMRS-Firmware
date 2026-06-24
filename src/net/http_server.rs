use esp_idf_sys::*;
use nimrs_core::context::{ControlSource, SYSTEM_STATE};
use nimrs_core::cv::CV_DEFS;

use super::{fs, ota, wifi};
use crate::{boot, dcc, logger};

pub struct HttpServer {
    handle: httpd_handle_t,
}

impl HttpServer {
    pub fn new() -> Option<Self> {
        let mut config: httpd_config_t = unsafe { core::mem::zeroed() };
        config.server_port = 80;
        config.max_uri_handlers = 32;

        let mut handle: httpd_handle_t = core::ptr::null_mut();
        let ret = unsafe { httpd_start(&mut handle, &config) };
        if ret != ESP_OK as i32 {
            log::error!("HTTP: start failed: {}", ret);
            return None;
        }

        let srv = Self { handle };
        srv.register_static_routes();
        srv.register_api_routes();
        srv.register_not_found();

        log::info!("HTTP: server on port 80");
        Some(srv)
    }

    fn reg(
        &self,
        uri: &str,
        method: httpd_method_t,
        handler: unsafe extern "C" fn(*mut httpd_req_t) -> esp_err_t,
    ) {
        let uri_c = std::ffi::CString::new(uri).unwrap();
        let mut reg: httpd_uri_t = unsafe { core::mem::zeroed() };
        reg.uri = uri_c.as_ptr();
        reg.method = method;
        let h: Option<unsafe extern "C" fn(*mut httpd_req_t) -> esp_err_t> = Some(handler);
        reg.handler = h;
        reg.user_ctx = core::ptr::null_mut();
        unsafe { httpd_register_uri_handler(self.handle, &reg) };
    }

    fn register_static_routes(&self) {
        // Web UI
        self.reg("/", 1, static_index);
        self.reg("/index.html", 1, static_index);
        self.reg("/style.css", 1, static_style);
        self.reg("/app.js", 1, static_app_js);
    }

    fn register_api_routes(&self) {
        self.reg("/api/status", 1, api_status);
        self.reg("/api/control", 2, api_control);
        self.reg("/api/cv", 2, api_cv);
        self.reg("/api/cv/all", 1, api_cv_all_get);
        self.reg("/api/cv/all", 2, api_cv_all_post);
        self.reg("/api/cv/defs", 1, api_cv_defs);
        self.reg("/api/logs", 1, api_logs_get);
        self.reg("/api/logs", 4, api_logs_delete);
        self.reg("/api/wifi/save", 2, api_wifi_save);
        self.reg("/api/wifi/reset", 2, api_wifi_reset);
        self.reg("/api/wifi/scan", 1, api_wifi_scan);
        self.reg("/api/config/hostname", 2, api_config_hostname);
        self.reg("/api/config/webauth", 2, api_config_webauth);
        self.reg("/api/telemetry", 1, api_telemetry);
        self.reg("/api/motor/test", 1, api_motor_test_get);
        self.reg("/api/motor/test", 2, api_motor_test_post);
        self.reg("/api/motor/reset_model", 2, api_motor_reset);
        self.reg("/api/motor/calibrate", 1, api_motor_calibrate_get);
        self.reg("/api/motor/calibrate", 2, api_motor_calibrate_post);
        self.reg("/api/files/list", 1, api_files_list);
        self.reg("/api/files/delete", 2, api_files_delete);
        self.reg("/api/files/format", 2, api_files_format);
        self.reg("/api/audio/play", 2, api_audio_play);
    }

    fn register_ota_route(&self) {
        self.reg("/update", 2, api_ota_update);
    }

    fn register_not_found(&self) {
        let uri_c = std::ffi::CString::new("*").unwrap();
        let mut reg: httpd_uri_t = unsafe { core::mem::zeroed() };
        reg.uri = uri_c.as_ptr();
        reg.method = 1;
        reg.handler = Some(not_found_handler);
        reg.user_ctx = core::ptr::null_mut();
        unsafe { httpd_register_uri_handler(self.handle, &reg) };
    }
}

impl Drop for HttpServer {
    fn drop(&mut self) {
        unsafe { httpd_stop(self.handle) };
    }
}

fn send_json(req: *mut httpd_req_t, json: &str) {
    unsafe {
        httpd_resp_set_type(req, b"application/json\0".as_ptr());
        httpd_resp_send(req, json.as_ptr(), json.len() as isize);
    }
}

fn send_text(req: *mut httpd_req_t, text: &str, _status: i32) {
    unsafe {
        httpd_resp_send(req, text.as_ptr(), text.len() as isize);
    }
}

fn millis() -> u32 {
    unsafe { esp_timer_get_time() as u32 / 1000 }
}

// --- Static handlers ---
unsafe extern "C" fn static_index(req: *mut httpd_req_t) -> esp_err_t {
    let html = nimrs_core::net::webassets::INDEX_HTML;
    httpd_resp_set_type(req, b"text/html\0".as_ptr());
    httpd_resp_send(req, html.as_ptr(), html.len() as isize);
    ESP_OK
}

unsafe extern "C" fn static_style(req: *mut httpd_req_t) -> esp_err_t {
    let css = nimrs_core::net::webassets::STYLE_CSS;
    httpd_resp_set_type(req, b"text/css\0".as_ptr());
    httpd_resp_send(req, css.as_ptr(), css.len() as isize);
    ESP_OK
}

unsafe extern "C" fn static_app_js(req: *mut httpd_req_t) -> esp_err_t {
    let js = nimrs_core::net::webassets::APP_JS;
    httpd_resp_set_type(req, b"application/javascript\0".as_ptr());
    httpd_resp_send(req, js.as_ptr(), js.len() as isize);
    ESP_OK
}

// --- /api/status ---
unsafe extern "C" fn api_status(req: *mut httpd_req_t) -> esp_err_t {
    let state = SYSTEM_STATE.lock().unwrap();
    let rb = boot::get_rollback_info();
    let json = format!(
        r#"{{"address":{},"speed":{},"direction":"{}","wifi":{},"uptime":{},"version":"rust","hash":"unknown","hostname":"NIMRS-Decoder","rolled_back":{},"running_version":"{}","crashed_version":"{}","functions":{}}}"#,
        state.dcc_address,
        state.speed,
        if state.direction {
            "forward"
        } else {
            "reverse"
        },
        if wifi::is_connected() { 1 } else { 0 } as u8,
        millis() / 1000,
        if rb.0 { "true" } else { "false" },
        rb.1,
        rb.2,
        format_functions(&state.functions),
    );
    send_json(req, &json);
    ESP_OK
}

fn format_functions(fns: &[bool; 29]) -> String {
    let mut s = String::from('[');
    for (i, &f) in fns.iter().enumerate() {
        if i > 0 {
            s.push(',');
        }
        s.push_str(if f { "true" } else { "false" });
    }
    s.push(']');
    s
}

// --- /api/control ---
unsafe extern "C" fn api_control(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 256];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Body missing", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let parsed: serde_json::Value = match serde_json::from_str(body) {
        Ok(v) => v,
        Err(_) => {
            send_text(req, "Invalid JSON", 400);
            return ESP_OK;
        }
    };

    let action = parsed["action"].as_str().unwrap_or("");
    let mut state = SYSTEM_STATE.lock().unwrap();

    match action {
        "stop" => {
            state.speed = 0;
            state.speed_source = ControlSource::Web;
            crate::motor::task::set_target_speed(0, true);
        }
        "toggle_lights" => {
            state.functions[0] = !state.functions[0];
        }
        "set_function" => {
            let idx = parsed["index"].as_u64().unwrap_or(99) as usize;
            let val = parsed["value"].as_bool().unwrap_or(false);
            if idx < 29 {
                state.functions[idx] = val;
            }
        }
        "set_speed" => {
            let dcc_step = parsed["value"].as_u64().unwrap_or(0) as u8;
            state.speed = (dcc_step as u16 * 255 / 126) as u8;
            state.speed_source = ControlSource::Web;
            crate::motor::task::set_target_speed(state.speed, state.direction);
        }
        "set_direction" => {
            state.direction = parsed["value"].as_bool().unwrap_or(true);
            state.speed_source = ControlSource::Web;
        }
        "clear_rollback" => {
            boot::clear_rollback();
        }
        _ => {
            send_text(req, "Unknown action", 400);
            return ESP_OK;
        }
    }

    send_json(req, r#"{"status":"ok"}"#);
    ESP_OK
}

// --- /api/cv ---
unsafe extern "C" fn api_cv(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 128];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Body missing", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let parsed: serde_json::Value = match serde_json::from_str(body) {
        Ok(v) => v,
        Err(_) => {
            send_text(req, "Invalid JSON", 400);
            return ESP_OK;
        }
    };

    let cmd = parsed["cmd"].as_str().unwrap_or("");
    let cv = parsed["cv"].as_u64().unwrap_or(0) as u16;
    match cmd {
        "read" => {
            let val = unsafe { crate::dcc::dcc_get_cv(cv) };
            send_json(req, &format!(r#"{{"cv":{},"value":{}}}"#, cv, val));
        }
        "write" => {
            let val = parsed["value"].as_u64().unwrap_or(0) as u8;
            unsafe {
                crate::dcc::dcc_set_cv(cv, val);
            }
            send_json(req, r#"{"status":"ok"}"#);
        }
        _ => {
            send_text(req, "Unknown cmd", 400);
        }
    }
    ESP_OK
}

// --- /api/cv/all GET ---
unsafe extern "C" fn api_cv_all_get(req: *mut httpd_req_t) -> esp_err_t {
    let mut json = String::from('{');
    for def in CV_DEFS {
        let val = crate::dcc::dcc_get_cv(def.id);
        if json.len() > 1 {
            json.push(',');
        }
        json.push_str(&format!("\"{}\":{}", def.id, val));
    }
    json.push('}');
    send_json(req, &json);
    ESP_OK
}

// --- /api/cv/all POST ---
unsafe extern "C" fn api_cv_all_post(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 1024];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Body missing", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let parsed: serde_json::Value = match serde_json::from_str(body) {
        Ok(v) => v,
        Err(_) => {
            send_text(req, "Invalid JSON", 400);
            return ESP_OK;
        }
    };
    if let Some(obj) = parsed.as_object() {
        for (key, val) in obj {
            if let Ok(cv) = key.parse::<u16>() {
                if let Some(v) = val.as_u64() {
                    crate::dcc::dcc_set_cv(cv, v as u8);
                }
            }
        }
    }
    send_json(req, r#"{"status":"ok"}"#);
    ESP_OK
}

// --- /api/cv/defs ---
unsafe extern "C" fn api_cv_defs(req: *mut httpd_req_t) -> esp_err_t {
    let mut json = String::from('[');
    for def in CV_DEFS {
        if json.len() > 1 {
            json.push(',');
        }
        json.push_str(&format!(
            r#"{{"cv":{},"name":"{}","desc":"{}"}}"#,
            def.id, def.name, def.desc
        ));
    }
    json.push(']');
    send_json(req, &json);
    ESP_OK
}

// --- /api/logs GET ---
unsafe extern "C" fn api_logs_get(req: *mut httpd_req_t) -> esp_err_t {
    let mut query = [0i8; 64];
    let has_query =
        httpd_req_get_url_query_str(req, query.as_mut_ptr(), query.len() as u32) == ESP_OK;
    let filter = if has_query {
        let mut val = [0i8; 64];
        if httpd_query_key_value(
            query.as_ptr(),
            b"type\0".as_ptr(),
            val.as_mut_ptr(),
            val.len() as u32,
        ) == ESP_OK
        {
            let s = std::ffi::CStr::from_ptr(val.as_ptr())
                .to_string_lossy()
                .into_owned();
            match s.as_str() {
                "data" => "[NIMRS_DATA]",
                "debug" => "DCC:",
                _ => "",
            }
        } else {
            ""
        }
    } else {
        ""
    };
    send_json(req, &logger::get_logs_json(filter));
    ESP_OK
}

// --- /api/logs DELETE ---
unsafe extern "C" fn api_logs_delete(req: *mut httpd_req_t) -> esp_err_t {
    logger::clear();
    send_json(req, r#"{"status":"cleared"}"#);
    ESP_OK
}

// --- /api/wifi/save ---
unsafe extern "C" fn api_wifi_save(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 256];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Missing body", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let mut ssid = "";
    let mut pass = "";
    for part in body.split('&') {
        if let Some(val) = part.strip_prefix("ssid=") {
            ssid = val;
        }
        if let Some(val) = part.strip_prefix("pass=") {
            pass = val;
        }
    }
    wifi::save_credentials(ssid, pass);
    send_text(req, "WiFi credentials saved. Restarting...", 200);
    std::thread::spawn(|| {
        std::thread::sleep(std::time::Duration::from_secs(1));
        unsafe {
            esp_restart();
        }
    });
    ESP_OK
}

// --- /api/wifi/reset ---
unsafe extern "C" fn api_wifi_reset(req: *mut httpd_req_t) -> esp_err_t {
    wifi::reset_credentials();
    send_text(req, "WiFi settings reset. Restarting...", 200);
    std::thread::spawn(|| {
        std::thread::sleep(std::time::Duration::from_secs(1));
        unsafe {
            esp_restart();
        }
    });
    ESP_OK
}

// --- /api/wifi/scan ---
unsafe extern "C" fn api_wifi_scan(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, &wifi::scan_json());
    ESP_OK
}

// --- /api/config/hostname ---
unsafe extern "C" fn api_config_hostname(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 128];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Missing name", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let name = body.strip_prefix("name=").unwrap_or("");
    if name.is_empty() || name.len() > 31 {
        send_text(req, "Invalid name length", 400);
    } else {
        unsafe {
            wifi_save_hostname(name);
        }
        send_text(req, "Hostname saved. Restart required.", 200);
    }
    ESP_OK
}

fn wifi_save_hostname(name: &str) {
    unsafe {
        let mut handle: nvs_handle_t = 0;
        if nvs_open(b"config\0".as_ptr(), 1, &mut handle) == ESP_OK as i32 {
            nvs_set_str(handle, b"hostname\0".as_ptr(), name.as_ptr());
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
}

// --- /api/config/webauth ---
unsafe extern "C" fn api_config_webauth(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 256];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Missing user or pass", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let mut user = "";
    let mut pass = "";
    for part in body.split('&') {
        if let Some(val) = part.strip_prefix("user=") {
            user = val;
        }
        if let Some(val) = part.strip_prefix("pass=") {
            pass = val;
        }
    }
    if user.len() < 32 && pass.len() < 32 {
        unsafe {
            let mut h: nvs_handle_t = 0;
            if nvs_open(b"config\0".as_ptr(), 1, &mut h) == ESP_OK as i32 {
                nvs_set_str(h, b"web_user\0".as_ptr(), user.as_ptr());
                nvs_set_str(h, b"web_pass\0".as_ptr(), pass.as_ptr());
                nvs_commit(h);
                nvs_close(h);
            }
        }
        send_text(req, "Web credentials saved. Restart required.", 200);
    } else {
        send_text(req, "Invalid length", 400);
    }
    ESP_OK
}

// --- /api/telemetry ---
unsafe extern "C" fn api_telemetry(req: *mut httpd_req_t) -> esp_err_t {
    let json = r#"{"target_speed":0,"duty":0,"current":0,"voltage":0,"rpm":0,"ripple_freq":0,"stalled":false,"moving":false}"#;
    send_json(req, json);
    ESP_OK
}

// --- Motor test endpoints ---
unsafe extern "C" fn api_motor_test_get(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, r#"[]"#);
    ESP_OK
}

unsafe extern "C" fn api_motor_test_post(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, r#"{"status":"started"}"#);
    ESP_OK
}

unsafe extern "C" fn api_motor_reset(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, r#"{"status":"reset"}"#);
    ESP_OK
}

unsafe extern "C" fn api_motor_calibrate_get(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, r#"{"state":"IDLE","resistance":0}"#);
    ESP_OK
}

unsafe extern "C" fn api_motor_calibrate_post(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, r#"{"status":"started"}"#);
    ESP_OK
}

// --- File endpoints ---
unsafe extern "C" fn api_files_list(req: *mut httpd_req_t) -> esp_err_t {
    send_json(req, &fs::list_json());
    ESP_OK
}

unsafe extern "C" fn api_files_delete(req: *mut httpd_req_t) -> esp_err_t {
    let mut buf = [0u8; 256];
    let len = httpd_req_recv(req, buf.as_mut_ptr() as *mut i8, buf.len() as isize);
    if len < 0 {
        send_text(req, "Missing path", 400);
        return ESP_OK;
    }
    let body = std::str::from_utf8(&buf[..len as usize]).unwrap_or("");
    let path = body.strip_prefix("path=").unwrap_or("");
    if fs::delete_file(path) {
        send_text(req, "Deleted", 200);
    } else {
        send_text(req, "File not found", 404);
    }
    ESP_OK
}

unsafe extern "C" fn api_files_format(req: *mut httpd_req_t) -> esp_err_t {
    send_text(req, "Formatting started...", 200);
    fs::format_fs();
    ESP_OK
}

// --- /api/audio/play ---
unsafe extern "C" fn api_audio_play(req: *mut httpd_req_t) -> esp_err_t {
    send_text(req, "Playing", 200);
    ESP_OK
}

// --- /update (OTA) ---
unsafe extern "C" fn api_ota_update(req: *mut httpd_req_t) -> esp_err_t {
    send_text(req, "OK", 200);
    ESP_OK
}

// --- 404 ---
unsafe extern "C" fn not_found_handler(req: *mut httpd_req_t) -> esp_err_t {
    httpd_resp_send_err(req, 0x0019, b"Not Found\0".as_ptr());
    ESP_OK
}
