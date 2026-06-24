mod audio;
mod boot;
mod dcc;
mod lighting;
mod logger;
mod motor;
mod net;
mod ota_overrides;

fn main() {
    esp_idf_sys::link_patches();
    logger::init();
    boot::check();
    log::info!("NIMRS-Firmware (Rust) starting up...");

    let mut wifi = net::wifi::WifiManager::new("NIMRS-Decoder");
    wifi.begin_connect();

    let _server = net::http_server::HttpServer::new();

    loop {
        wifi.loop_once();
        std::thread::sleep(std::time::Duration::from_millis(50));
    }
}
