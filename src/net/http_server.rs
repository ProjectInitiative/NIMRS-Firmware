use esp_idf_sys::*;

pub struct HttpServer {
    handle: httpd_handle_t,
}

impl HttpServer {
    pub fn new() -> Option<Self> {
        let mut config: httpd_config_t = unsafe { core::mem::zeroed() };
        config.server_port = 80;
        config.lru_purge_enable = true;
        config.max_uri_handlers = 32;

        let mut handle: httpd_handle_t = core::ptr::null_mut();
        let ret = unsafe { httpd_start(&mut handle, &config) };
        if ret != ESP_OK as i32 {
            log::error!("HTTP: start failed: {}", ret);
            return None;
        }

        let mut reg: httpd_uri_t = unsafe { core::mem::zeroed() };
        // Need to set up a basic handler to test
        let uri = b"/api/status\0";
        reg.uri = uri.as_ptr();
        reg.method = 1;
        reg.handler = Some(status_handler);
        reg.user_ctx = core::ptr::null_mut();
        unsafe {
            httpd_register_uri_handler(handle, &reg);
        }

        log::info!("HTTP: server on port 80");
        Some(Self { handle })
    }
}

impl Drop for HttpServer {
    fn drop(&mut self) {
        unsafe {
            httpd_stop(self.handle);
        }
    }
}

unsafe extern "C" fn status_handler(req: *mut httpd_req_t) -> esp_err_t {
    let json = b"{\"status\":\"ok\"}\0";
    let ctype = b"application/json\0";
    httpd_resp_set_type(req, ctype.as_ptr());
    httpd_resp_send(req, json.as_ptr(), json.len() as isize - 1);
    ESP_OK
}
