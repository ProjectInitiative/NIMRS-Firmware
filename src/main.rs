use std::ffi::CStr;
use log::info;

fn main() {
    esp_idf_sys::link_patches();
    env_logger::builder()
        .filter_level(log::LevelFilter::Info)
        .init();

    info!("NIMRS-Firmware (Rust) starting up...");
    let version = unsafe { CStr::from_ptr(esp_idf_sys::esp_get_idf_version_str()) };
    info!("ESP-IDF version: {}", version.to_string_lossy());

    loop {
        std::thread::sleep(std::time::Duration::from_secs(5));
        info!("tick");
    }
}
