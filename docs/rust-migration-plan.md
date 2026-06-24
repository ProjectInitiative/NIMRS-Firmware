# NIMRS-Firmware: C++ to Rust Migration Plan & Agent Guide

> **Status:** Phase 0 complete (toolchain + build proven). Phases 1-5 remain.
> **Goal:** Port ~4,000 lines of C++ firmware to Rust, 1:1 where possible.

## How to Use This Document

Each phase is self-contained. An agent can pick up any phase, read the listed C++ files,
and produce the Rust modules. Every module entry has:
- **Source:** the exact C++ file(s) to read
- **Target:** the Rust file to create
- **Pattern:** the translation rules
- **Verify:** how to confirm it works

Run `nix build .#rust-firmware` after each module to catch breakage early. Run `cargo test`
after each phase to verify logic.

---

## C++ to Rust Translation Patterns

These patterns appear throughout the codebase. Learn them once, apply everywhere.

### Singleton (Meyer) -> struct + Lazy / OnceLock

```cpp
// C++ — Meyer singleton
static MotorController& getInstance() {
    static MotorController instance;
    return instance;
}
```
```rust
// Rust — use once_cell or std::sync::OnceLock
use std::sync::OnceLock;
static MOTOR_CONTROLLER: OnceLock<MotorController> = OnceLock::new();
// In setup:
MOTOR_CONTROLLER.get_or_init(|| MotorController::new());
// Access:
MOTOR_CONTROLLER.get().unwrap()
```

### ScopedLock -> std::sync::Mutex

```cpp
// C++
SystemContext& ctx = SystemContext::getInstance();
ScopedLock lock(ctx);
SystemState& state = ctx.getState();
state.speed = 100;
```
```rust
// Rust
let mut state = SYSTEM_STATE.lock().unwrap();
state.speed = 100;
// MutexGuard dropped automatically at end of scope
```

### Global `#define Log Logger::getInstance()` -> log crate

```cpp
// C++
Log.printf("DCC: Write CV%d = %d\n", cv, value);
Log.debug("Motor starting");
```
```rust
// Rust — use the `log` crate
log::info!("DCC: Write CV{} = {}", cv, value);
log::debug!("Motor starting");
```
Telemetry data lines (`[NIMRS_DATA]`) use a custom log target:
```rust
log::info!(target: "data", "[NIMRS_DATA] {{\"tgt\":{},\"cur\":{}}}", target, current);
```

### Arduino `millis()` -> esp_idf_sys

```rust
fn millis() -> u32 {
    unsafe { esp_idf_sys::esp_timer_get_time() as u32 / 1000 }
}
```

### FreeRTOS task -> std::thread

```cpp
// C++
xTaskCreatePinnedToCore(_taskEntry, "MotorTask", 4096, this, 10, &_taskHandle, 1);
```
```rust
// Rust
std::thread::Builder::new()
    .name("MotorTask".into())
    .stack_size(4096)
    .spawn(move || motor_task_loop())
    .unwrap();
// For core pinning, use raw FFI:
// xTaskCreatePinnedToCore via esp_idf_sys
```

### ESP-IDF error checking

```cpp
// C++
ESP_ERROR_CHECK(nvs_flash_init());
```
```rust
// Rust — esp_idf_sys returns esp_err_t (i32); check == ESP_OK
let ret = unsafe { esp_idf_sys::nvs_flash_init() };
if ret != esp_idf_sys::ESP_OK as i32 {
    panic!("nvs_flash_init failed: {}", ret);
}
```

### CV access (EEPROM -> NVS)

```cpp
// C++ — Arduino EEPROM
EEPROM.write(CV, Value); EEPROM.commit();
uint8_t val = EEPROM.read(CV);
```
```rust
// Rust — use esp_idf_svc::nvs::Nvs or raw nvs_flash APIs
// CVs are 1-1024, stored in NVS namespace "nvs" (matches partition)
// Simplest: keep a byte array in RAM, persist to NVS on write
```

---

## Rust Crate Structure

```
src/
  main.rs               — entry point (replaces main.cpp)
  context.rs            — SystemState + Mutex (replaces SystemContext.h)
  cv.rs                 — CV constants + CvDef table (replaces CvRegistry.h)
  pinout.rs             — GPIO pin constants (replaces nimrs-pinout.h)
  motor/
    mod.rs              — re-exports
    hal.rs              — MCPWM + ADC (replaces MotorHal.cpp)
    task.rs             — PI control loop (replaces MotorTask.cpp)
    controller.rs       — momentum + telemetry (replaces MotorController.cpp)
    bemf.rs              — back-EMF estimator (replaces BemfEstimator.cpp)
    ripple.rs            — ripple frequency detector (replaces RippleDetector.cpp)
    dsp.rs               — EmaFilter + DcBlocker (replaces DspFilters.cpp)
  net/
    mod.rs
    wifi.rs             — WiFi STA + reconnect (part of ConnectivityManager)
    http_server.rs      — HTTP routes (part of ConnectivityManager)
    ota.rs              — OTA upload + rollback (part of ConnectivityManager)
    fs.rs               — LittleFS file manager (part of ConnectivityManager)
    webassets.rs        — embedded HTML/CSS/JS via include_str!
  dcc/
    mod.rs              — DccController + extern "C" callbacks
    shim.rs             — FFI declarations for NmraDcc shim
  audio/
    mod.rs              — AudioController
    player.rs           — I2S + volume + copier
    mp3_decoder.rs      — libhelix FFI wrapper
    wav_decoder.rs      — pure Rust WAV decoder
    assets.rs           — sound_assets.json loader
  boot.rs               — BootLoopDetector + OTA rollback (replaces BootLoopDetector.cpp)
  lighting.rs           — LightingController
  logger.rs             — log facade + WiFi log streaming (replaces Logger.cpp)
  ota_overrides.rs      — verifyRollbackLater (#[no_mangle] extern "C")
```

---

## Phase 1: Foundation Modules (Pure Logic, No Hardware)

**Goal:** Port all pure-math and data modules with unit tests. No ESP-IDF hardware touched.

### Module 1.1: `src/motor/dsp.rs` (replaces `DspFilters.h/.cpp`)

**Source:** `main/src/DspFilters.h` (29 lines), `main/src/DspFilters.cpp` (41 lines)

Create two structs: `EmaFilter` and `DcBlocker`.

```rust
// EmaFilter: _value = alpha * input + (1 - alpha) * _value
pub struct EmaFilter { alpha: f32, value: f32 }
impl EmaFilter {
    pub fn new(alpha: f32) -> Self { Self { alpha: alpha.clamp(0.0, 1.0), value: 0.0 } }
    pub fn set_alpha(&mut self, alpha: f32) { self.alpha = alpha.clamp(0.0, 1.0); }
    pub fn update(&mut self, input: f32) -> f32 {
        self.value = self.alpha * input + (1.0 - self.alpha) * self.value;
        self.value
    }
    pub fn value(&self) -> f32 { self.value }
    pub fn reset(&mut self, initial: f32) { self.value = initial; }
}
// DcBlocker: output = alpha * prev_output + alpha * (input - prev_input)
pub struct DcBlocker { alpha: f32, prev_input: f32, prev_output: f32 }
impl DcBlocker {
    pub fn new(alpha: f32) -> Self { Self { alpha, prev_input: 0.0, prev_output: 0.0 } }
    pub fn process(&mut self, input: f32) -> f32 {
        let output = self.alpha * self.prev_output + self.alpha * (input - self.prev_input);
        self.prev_input = input; self.prev_output = output;
        output
    }
    pub fn reset(&mut self) { self.prev_input = 0.0; self.prev_output = 0.0; }
}
```

**Verify:** `cargo test` with tests comparing known inputs to expected outputs.

### Module 1.2: `src/motor/bemf.rs` (replaces `BemfEstimator.h/.cpp`)

**Source:** `main/src/BemfEstimator.h` (52 lines), `main/src/BemfEstimator.cpp` (123 lines)

Port the `BemfEstimator` struct exactly as-is. Key fields:
- `_r_armature: f32` (default 35.0)
- `_poles: i32` (default 5)
- `_bemf_constant: f32` (default 0.015 V/RPM)
- `_bemf_k_filter: EmaFilter` (alpha=0.001)
- `_rpm_filter: EmaFilter` (alpha=0.05)

The `calculate_estimate()` algorithm (from BemfEstimator.cpp:37):
1. `i_stall = v_applied / r_armature` (guard r > 0.1)
2. `physically_stalled = v_applied > 0.5 && i_avg > 0.01 && i_avg > i_stall * 0.98`
3. `v_bemf = max(0, v_applied - i_avg * r_armature)`
4. `est_rpm_bemf = v_bemf / bemf_constant` (guard bemf_constant > 0)
5. `ripple_rpm = (ripple_freq * 60) / (2 * poles)` if ripple_freq > 0
6. `ripple_valid = ripple_rpm > 0 && i_avg > 0.20 && v_applied > 3.0`
7. If ripple_valid: use ripple_rpm; optionally learn Ke (accept if 0.005 < instant_k < 0.03)
8. Else if v_applied > 2.0: use BEMF estimate
9. Else: raw = 0
10. If physically_stalled or v_applied < 0.4: raw = 0
11. `_estimated_rpm = _rpm_filter.update(raw)`
12. If v_applied < 0.05: estimated_rpm = 0, filter.reset(0)

**Verify:** Add `#[test]` cases porting `tests/test_PID_Simulator.cpp` scenarios.

### Module 1.3: `src/motor/ripple.rs` (replaces `RippleDetector.h/.cpp`)

**Source:** `main/src/RippleDetector.h` (27 lines), `main/src/RippleDetector.cpp` (63 lines)

Port the Schmitt-trigger ripple detector. Fields:
- `_dc_blocker: DcBlocker` (alpha=0.9)
- `_state: bool` (false)
- `_threshold_high: f32` (0.05), `_threshold_low: f32` (-0.05)
- `_samples_since_pulse: u32`
- `_current_freq: f32`
- `_freq_filter: EmaFilter` (alpha=0.3)

Algorithm per sample: increment counter, DC-block, rising edge detection, compute dt,
accept freq if 2000us < dt < 200000us (5-500 Hz range).

**Verify:** `cargo test` with synthetic sine-wave input.

### Module 1.4: `src/cv.rs` (replaces `CvRegistry.h`)

**Source:** `main/src/CvRegistry.h` (166 lines)

Port all `CV::` constants as `pub const CV_XXX: u16` values. Create the `CvDef` struct and
`CV_DEFS` array exactly as in C++ (50 entries with id, default, name, desc).

```rust
pub mod cv {
    pub const ADDR_SHORT: u16 = 1;
    pub const V_START: u16 = 2;
    pub const ACCEL: u16 = 3;
    // ... all 50+ constants (copy from CvRegistry.h)

    pub struct CvDef { pub id: u16, pub default_value: u8, pub name: &'static str, pub desc: &'static str }
    pub static CV_DEFS: &[CvDef] = &[
        CvDef { id: 1, default_value: 3, name: "Primary Address", desc: "..." },
        // ... all 50 entries
    ];
}
```

**Verify:** `cargo test` that all CV values match the C++ constants.

### Module 1.5: `src/pinout.rs` (replaces `nimrs-pinout.h`)

**Source:** `main/src/nimrs-pinout.h` (47 lines)

```rust
pub mod pinout {
    pub const TRACK_RIGHT_3V3: u8 = 0;
    pub const TRACK_LEFT_3V3: u8 = 1;
    pub const MOTOR_IN1: u8 = 41;
    pub const MOTOR_IN2: u8 = 40;
    // ... all 24 pin constants
}
```

### Module 1.6: `src/context.rs` (replaces `SystemContext.h`)

**Source:** `main/src/SystemContext.h` (58 lines)

```rust
use std::sync::Mutex;
use once_cell::sync::Lazy;

#[derive(Clone, Copy, Default)]
pub enum ControlSource { #[default] Dcc, Web }

#[derive(Clone, Copy, Default)]
pub struct SystemState {
    pub dcc_address: u16,
    pub speed: u8,
    pub direction: bool,  // true = forward
    pub functions: [bool; 29],
    pub speed_source: ControlSource,
    pub last_dcc_speed: u8,
    pub last_dcc_direction: bool,
    pub wifi_connected: bool,
    pub last_dcc_packet_time: u32,  // millis()
    pub load_factor: f32,
}

pub static SYSTEM_STATE: Lazy<Mutex<SystemState>> =
    Lazy::new(|| Mutex::new(SystemState {
        dcc_address: 3,
        direction: true,
        ..Default::default()
    }));
```

### Module 1.7: `src/net/webassets.rs` (replaces `WebAssets.h`)

**Source:** `main/src/WebAssets.h` (1430 lines of inline HTML/CSS/JS strings)

Move the HTML/CSS/JS content into real files under `resources/` and embed them:

```rust
pub const INDEX_HTML: &str = include_str!("../resources/index.html");
pub const STYLE_CSS: &str = include_str!("../resources/style.css");
pub const APP_JS: &str = include_str!("../resources/app.js");
pub const LAME_MIN_JS: &str = include_str!("../resources/lame.min.js");
```

Extract the actual content from `WebAssets.h` — it's the string literal contents.

### Module 1.8: `src/audio/assets.rs` — sound asset loader

**Source:** `AudioController.cpp` `loadAssets()` (parses `/sound_assets.json`)

```rust
#[derive(serde::Deserialize)]
struct SoundAssets { assets: Vec<SoundAsset> }
#[derive(serde::Deserialize)]
pub struct SoundAsset {
    pub id: u8, pub name: String, pub r#type: String,
    pub files: AssetFiles,
}
#[derive(serde::Deserialize)]
pub struct AssetFiles {
    pub intro: Option<String>, pub r#loop: Option<String>, pub outro: Option<String>,
}
```

### Phase 1 Verification

```bash
cargo test                          # All logic tests pass
cargo build --target xtensa-esp32s3-espidf  # Cross-compiles (will fail at link — no main yet)
nix build .#rust-firmware           # Should still build (modules unused, dead-stripped)
```

---

## Phase 2: Hardware Abstraction & Motor Control

**Goal:** Port motor HAL, control loop, and lighting. Requires ESP-IDF hardware APIs.

### Module 2.1: `src/motor/hal.rs` (replaces `MotorHal.h/.cpp`)

**Source:** `main/src/MotorHal.h` (57 lines), `main/src/MotorHal.cpp` (208 lines)

This is the hardest module — MCPWM V5 + ADC1 + ISR. Uses raw `esp_idf_sys` FFI (esp-idf-hal
lacks MCPWM V5).

**Key APIs to call (all via `esp_idf_sys::`):**
- `mcpwm_new_timer`, `mcpwm_new_operator`, `mcpwm_operator_connect_timer`
- `mcpwm_new_comparator`, `mcpwm_new_generator`
- `mcpwm_timer_register_event_callbacks`, `mcpwm_timer_enable`, `mcpwm_timer_start_stop`
- `mcpwm_comparator_set_compare_value`
- `mcpwm_generator_set_action_on_timer_event`, `mcpwm_generator_set_action_on_compare_event`
- `adc1_config_width`, `adc1_config_channel_atten`, `adc1_get_raw`
- `xStreamBufferCreate`, `xStreamBufferSendFromISR`, `xStreamBufferReceive`

**MCPWM config (exact values from C++):**
- Group 0, resolution 1 MHz, count mode UP_DOWN, period 25 ticks -> 20 kHz PWM
- TEZ ISR reads ADC1_CH5 (GPIO5), pushes float to stream buffer

**ISR callback:**
```rust
// Must be unsafe extern "C" (replaces IRAM_ATTR callback)
use esp_idf_sys::mcpwm_timer_handle_t;

static mut LAST_CURRENT: f32 = 0.0;
static mut ADC_STREAM_BUFFER: esp_idf_sys::StreamBufferHandle_t = std::ptr::null_mut();

#[no_mangle]
pub unsafe extern "C" fn motor_hal_mcpwm_cb(
    _timer: esp_idf_sys::mcpwm_timer_handle_t,
    _edata: *const esp_idf_sys::mcpwm_timer_event_data_t,
    _user_ctx: *mut core::ffi::c_void,
) -> bool {
    let raw = esp_idf_sys::adc1_get_raw(esp_idf_sys::adc1_channel_t_ADC1_CHANNEL_5);
    let sample = raw as f32;
    LAST_CURRENT = sample;
    if !ADC_STREAM_BUFFER.is_null() {
        let val = sample;
        esp_idf_sys::xStreamBufferSendFromISR(
            ADC_STREAM_BUFFER,
            &val as *const f32 as *const _,
            std::mem::size_of::<f32>() as u32,
            &mut false as *mut _ as *mut _,
        );
    }
    false
}
```

**`set_duty(duty: f32)`:** Same three modes as C++: brake (|duty|<0.01), forward (duty>0),
reverse (duty<0). Use `mcpwm_generator_set_action_on_timer_event` with same action configs.

**`set_hardware_gain(mode: u8):** GPIO34 LOW/INPUT/HIGH.

**`get_current_scalar()`:** Same DRV8213 constants:
- Mode 0 (Low): V_PER_STEP / 0.492
- Mode 1 (Med): V_PER_STEP / 2.520
- Mode 2 (High): V_PER_STEP / 11.760
- V_PER_STEP = 3.3 / 4095.0

### Module 2.2: `src/motor/task.rs` (replaces `MotorTask.h/.cpp`)

**Source:** `main/src/MotorTask.h` (108 lines), `main/src/MotorTask.cpp` (367 lines)

The 50 Hz PI control loop. Port the entire `_loop()` algorithm:

1. Drain ADC stream buffer, compute avg/max/auto-zero offset
2. Call `ripple_detector.process_buffer()`
3. Run adaptive di/dt stall detector state machine (STOPPED -> STARTUP -> BASELINING -> RUNNING)
4. Three control zones:
   - Zone 0 (target=0): duty=0, persist Ke
   - Zone 2 (torque control, no ripple & target<20): vTarget = (target/255)*0.5*R + Vstart + kick
   - Zone 3 (PI velocity): vPi = Kp*error + Ki*integral
5. Asymmetric slew rate: maxIncrease=0.4, maxDecrease=0.02 per 20ms iteration
6. PWM dither for low speed (CV64)
7. Call `MotorHal::set_duty()`

**CV reload mapping (from MotorTask.cpp `reloadCvs()`):**
| Field | CV | Conversion |
|-------|-----|------------|
| r_armature | 149 | `cv==0 ? 175.0 : cv * 0.2` |
| poles | 143 | `cv ? cv as i32 : 5` |
| bemf_constant | 150 | `cv>0 ? cv*0.001 : 0.015` |
| track_voltage | 145 | `cv>50 ? cv*0.1 : 14.0` |
| v_start | 2 | `(cv/255.0) * track_voltage` |
| kp | 112 | `cv * 0.001` |
| ki | 114 | `cv * 0.0001` |

**Run as `std::thread`** (or raw `xTaskCreatePinnedToCore` for Core 1 pinning).

### Module 2.3: `src/motor/controller.rs` (replaces `MotorController.h/.cpp`)

**Source:** `main/src/MotorController.h` (51 lines), `main/src/MotorController.cpp` (98 lines)

Thin orchestrator: momentum (CV3-based accel/decel) + telemetry CSV output.

Momentum: every 10ms, nudge `_current_speed` toward target by `dt / (accel_delay)`
where `accel_delay = max(1, cv_accel) * 5.0`.

Telemetry: every 150ms, log `[NIMRS_DATA]` CSV line.

### Module 2.4: `src/lighting.rs` (replaces `LightingController.h/.cpp`)

**Source:** `main/src/LightingController.h` (18 lines), `main/src/LightingController.cpp` (101 lines)

10 GPIO outputs driven by function state + direction. Maps CV33-42 to function indices.

```rust
pub struct LightingController;
impl LightingController {
    pub fn setup(&self) { /* pinMode OUTPUT for 10 pins */ }
    pub fn loop(&self) {
        let state = SYSTEM_STATE.lock().unwrap();
        // For each output: check CV map -> function index -> drive pin
        // Front light: active when direction=fwd (if mapped to F0)
        // Rear light: active when direction=rev (if mapped to F0)
    }
}
```

### Phase 2 Verification

```bash
cargo test                          # DSP + math tests
nix build .#rust-firmware           # Full cross-compile + link
# On hardware: motor spins, telemetry streamed via WiFi
```

---

## Phase 3: System Services (WiFi, WebServer, OTA, Logging, Boot)

**Goal:** Port networking, OTA rollback, and logging infrastructure.

### Module 3.1: `src/logger.rs` (replaces `Logger.h/.cpp`)

**Source:** `main/src/Logger.h` (82 lines), `main/src/Logger.cpp` (294 lines)

Use `log` crate + `esp_idf_svc::log::EspLogger` as backend. Keep the in-memory ring buffer
for WiFi log streaming.

```rust
use std::sync::Mutex;
use std::collections::VecDeque;
use once_cell::sync::Lazy;

static LOG_LINES: Lazy<Mutex<VecDeque<String>>> = Lazy::new(|| Mutex::new(VecDeque::with_capacity(128)));
static DATA_LINES: Lazy<Mutex<VecDeque<String>>> = Lazy::new(|| Mutex::new(VecDeque::with_capacity(32)));

// Custom log logger that captures into the ring buffers
// + a WiFi log streaming thread (std::thread) that serves /api/logs
```

### Module 3.2: `src/boot.rs` (replaces `BootLoopDetector.h/.cpp`)

**Source:** `main/src/BootLoopDetector.h` (31 lines), `main/src/BootLoopDetector.cpp` (177 lines)

Use `esp_idf_svc::ota` and `esp_idf_svc::nvs` for OTA rollback.
- `check()`: if PENDING_VERIFY, start 30s stability timer
- `timer_callback()`: if health check passes, `esp_ota_mark_app_valid_cancel_rollback()`
- `perform_factory_reset()`: rewrite all CVs to defaults, disconnect WiFi, restart

### Module 3.3: `src/ota_overrides.rs` (replaces `ota_overrides.c`)

```rust
#[no_mangle]
pub extern "C" fn verifyRollbackLater() -> bool { true }

#[no_mangle]
pub extern "C" fn verifyOta() -> bool { false }
```

### Module 3.4: `src/net/wifi.rs` (WiFi STA + AP fallback)

**Source:** `main/src/ConnectivityManager.cpp` setup() + WiFi state machine

Use `esp_idf_svc::wifi::Wifi`. Try STA mode with stored credentials, 10s timeout,
fall back to AP mode if connection fails.

### Module 3.5: `src/net/http_server.rs` — HTTP routes

**Source:** `main/src/ConnectivityManager.cpp` (1110 lines, ~30 routes)

Use `esp_idf_svc::http::server::EspHttpServer`. Port each route handler. Key routes:

Priority order:
1. `/api/status` — returns JSON state snapshot
2. `/api/cv/all` + `/api/cv/set` — CV read/write
3. `/api/control` — JSON body actions (stop, toggle_lights, set_function, set_speed, etc.)
4. `/api/telemetry` — live motor data
5. `/api/files/*` — LittleFS file manager (list, upload, delete, format)
6. `/api/wifi/*` — WiFi scan, save, reset
7. `/api/motor/test` + `/api/motor/calibrate` — motor diagnostics
8. `/update` — OTA firmware upload
9. Static files: `/`, `/style.css`, `/app.js`, `/lame.min.js`

Use `serde_json` for all JSON serialization (replaces ArduinoJson). Use
`embedded_svc::utils::io::http` for request body reading.

### Module 3.6: `src/net/ota.rs` — OTA upload handler

**Source:** `ConnectivityManager.cpp` `handleFirmwareUpdate()`

Use `esp_idf_svc::ota::Ota` wrapper. Stream the upload body directly into
`Ota::write()`.

### Module 3.7: `src/net/fs.rs` — LittleFS file manager

**Source:** `ConnectivityManager.cpp` file handlers

Use `embedded_svc::fs` or raw `esp_idf_sys::esp_littlefs_*` APIs. Whitelist
`.json/.wav/.mp3` uploads. Path traversal guards (reject `..` and null bytes).

### Module 3.8: `src/main.rs` (replaces `main.cpp`)

**Source:** `main/main.cpp` (120 lines)

```rust
fn main() {
    // 1. nvs_flash_init
    // 2. logger setup (EspLogger + ring buffer + WiFi thread)
    // 3. boot_loop_detector::check()
    // 4. cv load (from NVS)
    // 5. connectivity_manager.setup() (WiFi + HTTP server + OTA)
    // 6. motor_controller.setup() (starts MotorTask on Core 1)
    // 7. lighting_controller.setup()
    // 8. audio_controller.setup()
    // 9. Spawn control_plane_task on Core 0 (DCC loop + motor loop + lighting loop)
    // 10. Main loop: connectivity_manager.loop() + audio_controller.loop() + heartbeat
}
```

### Phase 3 Verification

```bash
cargo test
nix build .#rust-firmware
# On hardware: WiFi web UI works, OTA upload tested, CV read/write works
```

---

## Phase 4: FFI Bridges (DCC + Audio)

**Goal:** Port DCC and Audio using FFI to existing C libraries.

### Module 4.1: `src/dcc/mod.rs` + `src/dcc/shim.rs` (replaces `DccController.h/.cpp`)

**Source:** `main/src/DccController.h` (49 lines), `main/src/DccController.cpp` (292 lines)

**FFI strategy (see `docs/rust-ffi-roadblocks.md` for details):**

1. Keep `NmraDcc` as a C++ `extra_component` with a thin shim (`dcc_shim.cpp`):
   ```cpp
   // dcc_shim.cpp — ~40 lines
   #include <NmraDcc.h>
   static NmraDcc dcc;
   extern "C" void dcc_init(uint8_t pin, uint8_t mfr, uint8_t ver, uint8_t flags) {
       dcc.pin(pin, 1); dcc.init(mfr, ver, flags, 0);
   }
   extern "C" uint8_t dcc_process() { return dcc.process(); }
   extern "C" uint8_t dcc_get_cv(uint16_t cv) { return dcc.getCV(cv); }
   extern "C" uint8_t dcc_set_cv(uint16_t cv, uint8_t v) { return dcc.setCV(cv, v); }
   extern "C" uint16_t dcc_get_addr() { return dcc.getAddr(); }
   ```

2. Rust side defines the **weak callbacks** (replaces the C++ free functions):
   ```rust
   #[no_mangle]
   pub extern "C" fn notifyDccSpeed(addr: u16, _addr_type: u8, speed: u8, dir: u8, _steps: u8) {
       let mut state = SYSTEM_STATE.lock().unwrap();
       state.speed = if speed > 1 { speed } else { 0 };
       state.direction = dir == 1;
       state.last_dcc_packet_time = millis();
       state.speed_source = ControlSource::Dcc;
       state.dcc_address = addr;
   }
   #[no_mangle]
   pub extern "C" fn notifyCVWrite(cv: u16, value: u8) -> u8 {
       // Port of DccController.cpp notifyCVWrite:
       // - CV 8: factory reset (or save manufacturer ID if <3s from boot)
       // - CV 151: SuperCap enable live update
       // - Else: persist to NVS
       crate::cv::write(cv, value)
   }
   // ... notifyDccFunc, notifyCVAck, notifyCVResetFactoryDefault
   ```

3. Add to `Cargo.toml`:
   ```toml
   [[package.metadata.esp-idf-sys.extra_components]]
   component_dirs = ["vendor/nmradcc"]
   bindings_header = "vendor/nmradcc/bindings.h"
   bindings_module = "dcc"
   ```

**Port the following callback bodies from DccController.cpp:**
- `notifyDccSpeed` (lines ~226-256): delta-check, speedSource logic, SystemContext lock
- `notifyDccFunc` (lines ~257-285): FN_GROUP decode into `functions[29]` array
- `notifyCVWrite` (lines ~140-200): CV8 factory reset, live pin updates, NVS persist
- `notifyCVAck` (lines ~110-130): SuperCap disable, 6ms duty pulse, SuperCap restore
- `notifyCVResetFactoryDefault` (lines ~200-220): iterate CV_DEFS, write defaults

### Module 4.2: `src/audio/player.rs` + `src/audio/mp3_decoder.rs`

**Source:** `main/src/AudioController.h` (54 lines), `main/src/AudioController.cpp` (194 lines)

Keep `esp-libhelix-mp3` as C `extra_component`. Write Rust pipeline (~300 lines):

1. **`Mp3Decoder`** — libhelix FFI wrapper (~30 lines, see `docs/rust-ffi-roadblocks.md`):
   ```rust
   pub struct Mp3Decoder { handle: *mut core::ffi::c_void }
   impl Mp3Decoder {
       pub fn new() -> Option<Self> {
           let h = unsafe { esp_idf_sys::mp3::MP3InitDecoder() };
           if h.is_null() { None } else { Some(Self { handle: h }) }
       }
       pub fn decode(&mut self, input: &[u8], output: &mut [i16]) -> Result<usize, i32> { ... }
   }
   impl Drop for Mp3Decoder { fn drop(&mut self) { unsafe { esp_idf_sys::mp3::MP3FreeDecoder(self.handle); } } }
   ```

2. **`WavDecoder`** — pure Rust (44-byte header parser + PCM passthrough). ~50 lines.

3. **`Player`** struct with:
   - I2S output (via `esp-idf-hal::i2s` or raw `esp_idf_sys::i2s_*`)
   - Volume scaling (multiply PCM samples by `CV_MASTER_VOL / 255.0`)
   - File source from LittleFS
   - `play_file(path)`, `stop()`, `loop_step()` (copy + decode)

4. Pin config: `AMP_BCLK=38, AMP_LRCLK=36, AMP_DIN=37`, 44100 Hz mono, `AMP_SD_MODE=33` output.

5. Port the function-key->asset mapping from `AudioController.cpp` `loop()`:
   - Read `functions[29]` from `SystemState`
   - For each asset, check `CV_AUDIO_MAP_BASE + id` for function index
   - Rising/falling edge triggers play/stop based on asset type

### Module 4.3: `src/audio/wav_decoder.rs`

Pure Rust WAV decoder. Parse 44-byte RIFF header, extract sample rate + bits per sample,
pass through PCM samples. ~50 lines.

### Phase 4 Verification

```bash
cargo test
nix build .#rust-firmware
# On hardware: DCC packets control speed/functions, sound plays on function keys
```

---

## Phase 5: Cutover & Cleanup

**Goal:** Remove all C++ code. Single Rust firmware.

### Tasks

1. Delete `main/` directory entirely
2. Remove `nix/arduino-components.nix`, `nix/common-libs.nix`, `nix/dependencies.nix`
3. Remove `arduino-nix`, `arduino-indexes` flake inputs
4. Delete `tools/generate_lamejs_header.py`, `tools/test_runner.py`
5. Delete `tests/mocks/`, `tests/test_*.cpp`, `tests/simulator/`
6. Update `treefmt.toml`: drop C++ formatters, keep `rustfmt`
7. Update `AGENTS.md`: replace C++ commands with Rust commands
8. Update `devenv.nix`: replace `build-firmware` with `cargo build`
9. Update `sdkconfig.rust.defaults`: now used directly by esp-idf-sys
10. Update `Cargo.toml` with `[package.metadata.esp-idf-sys]` extra_components for NmraDcc + libhelix
11. Single `agent-check` that runs:
    - `cargo fmt --check`
    - `cargo clippy -- -D warnings`
    - `cargo test`
    - `nix build .#rust-firmware`
    - Merge conflict check

### Final Verification

```bash
nix develop --command agent-check    # Must pass
nix build .#rust-firmware            # Fully sandboxed, produces ELF binary
file $(nix path-info .#rust-firmware)/nimrs-firmware
# -> ELF 32-bit LSB executable, Tensilica Xtensa, statically linked
```

---

## Appendix: Source File Quick Reference

| C++ File | Lines | Rust Target | Phase |
|----------|-------|-------------|-------|
| `main/src/DspFilters.cpp` | 41 | `src/motor/dsp.rs` | 1 |
| `main/src/BemfEstimator.cpp` | 123 | `src/motor/bemf.rs` | 1 |
| `main/src/RippleDetector.cpp` | 63 | `src/motor/ripple.rs` | 1 |
| `main/src/CvRegistry.h` | 166 | `src/cv.rs` | 1 |
| `main/src/nimrs-pinout.h` | 47 | `src/pinout.rs` | 1 |
| `main/src/SystemContext.h` | 58 | `src/context.rs` | 1 |
| `main/src/WebAssets.h` | 1430 | `src/net/webassets.rs` + `resources/` | 1 |
| `main/src/AudioUtils.h` | 23 | `src/audio/assets.rs` | 1 |
| `main/src/MotorHal.cpp` | 208 | `src/motor/hal.rs` | 2 |
| `main/src/MotorTask.cpp` | 367 | `src/motor/task.rs` | 2 |
| `main/src/MotorController.cpp` | 98 | `src/motor/controller.rs` | 2 |
| `main/src/LightingController.cpp` | 101 | `src/lighting.rs` | 2 |
| `main/src/Logger.cpp` | 294 | `src/logger.rs` | 3 |
| `main/src/BootLoopDetector.cpp` | 177 | `src/boot.rs` | 3 |
| `main/src/ota_overrides.c` | 28 | `src/ota_overrides.rs` | 3 |
| `main/src/ConnectivityManager.cpp` | 1110 | `src/net/*.rs` | 3 |
| `main/main.cpp` | 120 | `src/main.rs` | 3 |
| `main/src/DccController.cpp` | 292 | `src/dcc/*.rs` | 4 |
| `main/src/AudioController.cpp` | 194 | `src/audio/*.rs` | 4 |
