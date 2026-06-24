use esp_idf_sys::*;

pub struct FileManager;

impl FileManager {
    pub fn init() -> bool {
        unsafe {
            let mut conf: esp_vfs_spiffs_conf_t = core::mem::zeroed();
            conf.base_path = b"/spiffs\0".as_ptr();
            conf.partition_label = core::ptr::null();
            conf.max_files = 10;
            conf.format_if_mount_failed = false;

            if esp_vfs_spiffs_register(&conf) != ESP_OK as i32 {
                conf.format_if_mount_failed = true;
                if esp_vfs_spiffs_register(&conf) != ESP_OK as i32 {
                    return false;
                }
            }
        }
        true
    }

    pub fn total_bytes() -> u64 {
        let mut total: usize = 0;
        let mut _used: usize = 0;
        unsafe { esp_spiffs_info(core::ptr::null(), &mut total, &mut _used) };
        total as u64
    }

    pub fn used_bytes() -> u64 {
        let mut _total: usize = 0;
        let mut used: usize = 0;
        unsafe { esp_spiffs_info(core::ptr::null(), &mut _total, &mut used) };
        used as u64
    }
}

pub fn list_json() -> String {
    String::from("[]")
}

pub fn delete_file(_path: &str) -> bool {
    false
}

pub fn format_fs() -> bool {
    false
}
