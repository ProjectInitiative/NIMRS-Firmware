mod lighting;
mod motor;

fn main() {
    esp_idf_sys::link_patches();
    println!("NIMRS-Firmware (Rust) starting up...");
    loop {
        std::thread::sleep(std::time::Duration::from_secs(5));
        println!("tick");
    }
}
