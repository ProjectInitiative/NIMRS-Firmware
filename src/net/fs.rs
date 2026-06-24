use esp_idf_sys::*;
use std::ffi::CStr;

pub struct FileManager;

impl FileManager {
    pub fn init() -> bool {
        unsafe {
            let mut conf: esp_vfs_spiffs_conf_t = core::mem::zeroed();
            conf.base_path = b"/spiffs\0".as_ptr();
            conf.partition_label = core::ptr::null();
            conf.max_files = 10;
            conf.format_if_mount_failed = false;

            let ret = esp_vfs_spiffs_register(&conf);
            if ret != ESP_OK as i32 {
                log::info!("FS: SPIFFS mount failed, formatting...");
                conf.format_if_mount_failed = true;
                let ret2 = esp_vfs_spiffs_register(&conf);
                if ret2 != ESP_OK as i32 {
                    log::error!("FS: SPIFFS mount after format failed: {}", ret2);
                    return false;
                }
            }

            if let Ok(info) = Self::info() {
                log::info!("FS: SPIFFS total={} used={}", info.0, info.1);
            }
        }
        true
    }

    pub fn info() -> Result<(usize, usize), ()> {
        unsafe {
            let mut total: usize = 0;
            let mut used: usize = 0;
            if esp_spiffs_info(core::ptr::null(), &mut total, &mut used) == ESP_OK as i32 {
                Ok((total, used))
            } else {
                Err(())
            }
        }
    }

    pub fn list() -> Vec<String> {
        let mut files = Vec::new();
        unsafe {
            let dir = libc::opendir(b"/spiffs\0".as_ptr());
            if !dir.is_null() {
                loop {
                    let entry = libc::readdir(dir);
                    if entry.is_null() {
                        break;
                    }
                    let name = CStr::from_ptr((*entry).d_name.as_ptr())
                        .to_string_lossy()
                        .into_owned();
                    if name != "." && name != ".." {
                        files.push(name);
                    }
                }
                libc::closedir(dir);
            }
        }
        files
    }

    pub fn exists(path: &str) -> bool {
        let full = format!("/spiffs{}", if path.starts_with('/') { "" } else { "/" });
        let full = format!("{}{}", full, path);
        unsafe {
            let cpath = std::ffi::CString::new(full).unwrap();
            let file = libc::fopen(cpath.as_ptr(), b"r\0".as_ptr());
            if !file.is_null() {
                libc::fclose(file);
                true
            } else {
                false
            }
        }
    }

    pub fn remove(path: &str) -> bool {
        let full = format!("/spiffs{}", if path.starts_with('/') { "" } else { "/" });
        let full = format!("{}{}", full, path);
        unsafe {
            let cpath = std::ffi::CString::new(full).unwrap();
            libc::unlink(cpath.as_ptr()) == 0
        }
    }

    pub fn format() -> bool {
        unsafe { esp_spiffs_format(core::ptr::null()) == ESP_OK as i32 }
    }

    pub fn total_bytes() -> usize {
        Self::info().map(|i| i.0).unwrap_or(0)
    }

    pub fn used_bytes() -> usize {
        Self::info().map(|i| i.1).unwrap_or(0)
    }

    pub fn read(path: &str) -> Option<Vec<u8>> {
        let full = format!("/spiffs{}", if path.starts_with('/') { "" } else { "/" });
        let full = format!("{}{}", full, path);
        unsafe {
            let cpath = std::ffi::CString::new(full).unwrap();
            let file = libc::fopen(cpath.as_ptr(), b"r\0".as_ptr());
            if file.is_null() {
                return None;
            }
            libc::fseek(file, 0, 2);
            let len = libc::ftell(file);
            libc::fseek(file, 0, 0);
            let mut buf = vec![0u8; len as usize];
            libc::fread(
                buf.as_mut_ptr() as *mut core::ffi::c_void,
                1,
                len as usize,
                file,
            );
            libc::fclose(file);
            Some(buf)
        }
    }

    pub fn write(path: &str, data: &[u8]) -> bool {
        let full = format!("/spiffs{}", if path.starts_with('/') { "" } else { "/" });
        let full = format!("{}{}", full, path);
        unsafe {
            let cpath = std::ffi::CString::new(full).unwrap();
            let file = libc::fopen(cpath.as_ptr(), b"w\0".as_ptr());
            if file.is_null() {
                return false;
            }
            let written = libc::fwrite(
                data.as_ptr() as *const core::ffi::c_void,
                1,
                data.len(),
                file,
            );
            libc::fclose(file);
            written == data.len()
        }
    }
}

pub fn list_json() -> String {
    let files = FileManager::list();
    let mut json = String::from('[');
    for (i, name) in files.iter().enumerate() {
        if i > 0 {
            json.push(',');
        }
        let path = format!("/spiffs/{}", name);
        let size = FileManager::read(&format!("/{}", name))
            .map(|d| d.len())
            .unwrap_or(0);
        json.push_str(&format!(r#"{{"name":"/{}","size":{}}}"#, name, size));
    }
    json.push(']');
    json
}

pub fn delete_file(path: &str) -> bool {
    FileManager::remove(path)
}

pub fn format_fs() -> bool {
    FileManager::format()
}
