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

    // WiFi + HTTP server
    let mut wifi = net::wifi::WifiManager::new("NIMRS-Decoder");
    wifi.begin_connect();
    let _server = net::http_server::HttpServer::new();

    // Motor control (starts PI loop on core 1)
    motor::hal::init();
    motor::task::start();

    // Lighting
    lighting::LightingController::setup();

    // DCC
    dcc::setup();

    let mut tick: u32 = 0;
    loop {
        wifi.loop_once();
        dcc::loop_once();

        if tick % 5 == 0 {
            lighting::LightingController::run_loop();
        }

        std::thread::sleep(std::time::Duration::from_millis(10));
        tick = tick.wrapping_add(1);
    }
}
