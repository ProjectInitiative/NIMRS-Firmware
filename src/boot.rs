use esp_idf_sys::*;
use std::ffi::CStr;

const STABILITY_MS: u32 = 30000;

pub fn check() {
    unsafe {
        let running = esp_ota_get_running_partition();
        if running.is_null() {
            return;
        }
        let mut state: esp_ota_img_states_t = 0;
        if esp_ota_get_state_partition(running, &mut state) == ESP_OK as i32 {
            if state == 1 || state == 0 {
                // ESP_OTA_IMG_PENDING_VERIFY = 1, ESP_OTA_IMG_NEW = 0
                log::info!("Boot: unverified firmware, starting stability timer");
                start_stability_timer();
            }
        }
    }
}

fn start_stability_timer() {
    std::thread::Builder::new()
        .name("StabilityTimer".into())
        .stack_size(2048)
        .spawn(|| {
            std::thread::sleep(std::time::Duration::from_millis(STABILITY_MS as u64));
            log::info!("Boot: stability check passed");
            mark_successful();
        })
        .ok();
}

pub fn mark_successful() {
    let err = unsafe { esp_ota_mark_app_valid_cancel_rollback() };
    if err == ESP_OK as i32 {
        log::info!("Boot: firmware marked VALID");
    }
}

pub fn did_rollback() -> bool {
    let mut val: u8 = 0;
    unsafe {
        let mut handle: nvs_handle_t = 0;
        if nvs_open(
            "bootloop\0".as_ptr() as *const i8,
            0,
            &mut handle,
        ) == ESP_OK as i32
        {
            nvs_get_u8(handle, "rolledback\0".as_ptr() as *const i8, &mut val);
            nvs_close(handle);
        }
    }
    val != 0
}

pub fn clear_rollback() {
    unsafe {
        let mut handle: nvs_handle_t = 0;
        if nvs_open(
            "bootloop\0".as_ptr() as *const i8,
            1,
            &mut handle,
        ) == ESP_OK as i32
        {
            nvs_set_u8(handle, "rolledback\0".as_ptr() as *const i8, 0);
            nvs_set_u8(handle, "acknowledged\0".as_ptr() as *const i8, 1);
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
}

pub fn get_rollback_info() -> (bool, String, String) {
    let mut running_ver = String::from("Unknown");
    let mut crashed_ver = String::from("Unknown");

    unsafe {
        let running = esp_ota_get_running_partition();
        if !running.is_null() {
            let mut desc: esp_app_desc_t = core::mem::zeroed();
            if esp_ota_get_partition_description(running, &mut desc) == ESP_OK as i32 {
                if let Ok(c) = CStr::from_ptr(desc.version.as_ptr()).to_str() {
                    running_ver = c.to_string();
                }
            }
        }

        let invalid = esp_ota_get_last_invalid_partition();
        if !invalid.is_null() {
            let mut desc: esp_app_desc_t = core::mem::zeroed();
            if esp_ota_get_partition_description(invalid, &mut desc) == ESP_OK as i32 {
                if let Ok(c) = CStr::from_ptr(desc.version.as_ptr()).to_str() {
                    crashed_ver = c.to_string();
                }
            }
        }
    }

    (did_rollback(), running_ver, crashed_ver)
}
