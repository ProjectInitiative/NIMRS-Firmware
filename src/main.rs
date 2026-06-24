mod motor;
mod lighting;
mod logger;
mod boot;
mod ota_overrides;
mod net;
mod dcc;
mod audio;

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
