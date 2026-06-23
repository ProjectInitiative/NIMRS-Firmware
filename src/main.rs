use std::ffi::CStr;

fn main() {
    esp_idf_sys::link_patches();

    let version = unsafe { CStr::from_ptr(esp_idf_sys::esp_get_idf_version()) };
    println!("NIMRS-Firmware (Rust) starting up...");
    println!("ESP-IDF version: {}", version.to_string_lossy());

    loop {
        std::thread::sleep(std::time::Duration::from_secs(5));
        println!("tick");
    }
}
