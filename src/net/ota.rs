use esp_idf_sys::*;

static mut OTA_HANDLE: esp_ota_handle_t = 0;
static mut UPDATE_PARTITION: *const esp_partition_t = core::ptr::null();

pub fn start_update() -> bool {
    unsafe {
        UPDATE_PARTITION = esp_ota_get_next_update_partition(core::ptr::null());
        if UPDATE_PARTITION.is_null() {
            log::error!("OTA: no update partition");
            return false;
        }
        let ret = esp_ota_begin(
            UPDATE_PARTITION,
            IMAGE_MAX_SIZE as usize,
            &mut OTA_HANDLE as *mut _,
        );
        if ret != ESP_OK as i32 {
            log::error!("OTA: begin failed: {}", ret);
            return false;
        }
        log::info!("OTA: update started");
    }
    true
}

pub fn write_chunk(data: &[u8]) -> bool {
    unsafe {
        let ret = esp_ota_write(
            OTA_HANDLE,
            data.as_ptr() as *const core::ffi::c_void,
            data.len(),
        );
        if ret != ESP_OK as i32 {
            log::error!("OTA: write failed: {}", ret);
            false
        } else {
            true
        }
    }
}

pub fn end_update(success: bool) -> bool {
    unsafe {
        if success {
            let ret = esp_ota_end(OTA_HANDLE);
            if ret != ESP_OK as i32 {
                log::error!("OTA: end failed: {}", ret);
                return false;
            }
            let ret = esp_ota_set_boot_partition(UPDATE_PARTITION);
            if ret == ESP_OK as i32 {
                log::info!("OTA: success, rebooting...");
                true
            } else {
                log::error!("OTA: set_boot_partition failed: {}", ret);
                false
            }
        } else {
            esp_ota_abort(OTA_HANDLE);
            log::info!("OTA: aborted");
            false
        }
    }
}

const IMAGE_MAX_SIZE: u64 = 0x200000; // 2MB
