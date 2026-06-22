# NIMRS-Firmware: C++ → Rust Migration Feasibility Report & Plan

> **Status:** Feasible (hybrid approach recommended)
> **Date:** 2026-06-22
> **Scope:** Full conversion of the firmware from Arduino/C++ on ESP-IDF to Rust on ESP-IDF, while preserving the Nix-based reproducible dependency structure and enabling a future `devenv` migration.

---

## 1. Executive Summary

**Verdict: FEASIBLE — with two well-defined porting challenges (DCC and Audio).**

The firmware (~4,000 lines of C++ across 18 modules) can be migrated to Rust using the
`esp-idf-sys` / `esp-idf-svc` / `esp-idf-hal` crate family, which provides safe and raw
bindings to the **same ESP-IDF v5.x SDK** already provided by the Nix `esp-dev` flake.
~80% of the codebase maps directly onto the Rust ESP-IDF ecosystem or is pure math that
ports trivially.

The remaining 20% hinges on **two C++ Arduino libraries with no mature Rust equivalent**:

| Library                                                                 | Role                                   | Rust Equivalent                               | Recommended Path                                                               |
| ----------------------------------------------------------------------- | -------------------------------------- | --------------------------------------------- | ------------------------------------------------------------------------------ |
| **NmraDcc** 2.0.17                                                      | DCC protocol decode (interrupt-driven) | None                                          | Keep as C component via FFI; port to Rust later                                |
| **ESP8266Audio / arduino-audio-tools / libhelix**                       | I2S, MP3/WAV decode, stream pipeline   | None (I2S HAL exists; no streaming framework) | Reimplement a thin Rust audio pipeline over `esp-idf-hal` I2S + `libhelix` FFI |
| **ArduinoJson** 7.3.0                                                   | JSON                                   | `serde_json` / `serde`                        | Direct replacement                                                             |
| **arduino-esp32 core** (WiFi, LittleFS, Preferences, WebServer, EEPROM) | Arduino API shim                       | `esp-idf-svc` + `esp-idf-hal`                 | Direct replacement                                                             |

The **Nix dependency structure can be preserved**: `esp-idf-sys`'s native builder accepts
`IDF_PATH` + `ESP_IDF_TOOLS_INSTALL_DIR=fromenv` to consume the already-pinned ESP-IDF
from the `esp-dev` flake instead of downloading anything. The one genuine Nix gap is the
**Xtensa Rust toolchain** (not in `nixpkgs`, not in `esp-dev`); it must be supplied via an
`espup`-based Fixed-Output Derivation or a community Xtensa-Rust flake.

A **hybrid migration** is recommended: port logic and ESP-IDF-native modules to Rust
first, keep the two problematic C/C++ libraries as `extra_components` called through FFI,
and incrementally retire the FFI bridges once Rust replacements are validated.

---

## 2. Current System Analysis

### 2.1 Codebase Inventory

```
main/main.cpp                 120  Entry point (app_main, task pinning)
main/src/
  ConnectivityManager.cpp    1109  WiFi, WebServer, OTA, LittleFS file mgr   <-- largest
  WebAssets.h                1430  Generated HTML/CSS/JS strings (embedded)
  Logger.cpp                  294  Buffered logging + WiFi log task
  DccController.cpp           292  DCC packet handling, CV storage, callbacks
  MotorTask.cpp               367  Core-1 real-time PI loop (50Hz)
  MotorHal.cpp                208  MCPWM, ADC, stream buffer, ISR callback
  BootLoopDetector.cpp        177  Crash counter, factory reset
  AudioController.cpp         193  I2S, MP3/WAV playback, asset mapping
  BemfEstimator.cpp           123  Back-EMF math model          (pure math)
  DspFilters.cpp               41  EMA / DC-bias filters         (pure math)
  RippleDetector.cpp           63  Commutation spike counting    (pure math)
  CvRegistry.h                166  CV definition table           (pure data)
  LightingController.cpp      101  PWM lighting effects
  MotorController.cpp          98  Speed→torque bridge
  ota_overrides.c              28  verifyRollbackLater() weak override (C)
  + headers, pinout, SystemContext, AudioUtils
─────────────────────────────────────
~4,000 lines firmware (excl. generated WebAssets.h / LameJs.h)
```

### 2.2 External Dependencies (C++)

From `nix/common-libs.nix` + `main/idf_component.yml` + `dependencies.lock`:

1. **arduino-esp32 core v3.x** (ESP-IDF managed component) — Arduino API on ESP-IDF.
2. **NmraDcc 2.0.17** (Arduino lib via `arduino-nix`) — DCC protocol decoder.
3. **ArduinoJson 7.3.0** (Arduino lib via `arduino-nix`) — JSON.
4. **arduino-audio-tools** (GitHub pin, `pschatzmann`) — I2S / stream framework.
5. **arduino-libhelix** (GitHub pin, `pschatzmann`) — MP3 decoder.
6. **chmorgan/esp-libhelix-mp3 1.0.3** (ESP-IDF registry) — libhelix as IDF component.
7. **ESP-IDF v5.x** (via `esp-dev.packages.esp-idf-full`).

### 2.3 ESP-IDF / Arduino API Surface (usage concentration)

| File                      | API calls | Notes                                                                        |
| ------------------------- | --------- | ---------------------------------------------------------------------------- |
| `ConnectivityManager.cpp` | 90        | WiFi, WebServer (~30 routes), OTA, LittleFS, Preferences, ArduinoJson        |
| `MotorHal.cpp`            | 49        | MCPWM v5 API, ADC1, stream buffers, ISR (`IRAM_ATTR`), GPIO drive capability |
| `BootLoopDetector.cpp`    | 22        | NVS, esp_ota, esp_partition                                                  |
| `LightingController.cpp`  | 21        | ledc PWM, GPIO                                                               |
| `DccController.cpp`       | 16        | NmraDcc, EEPROM, GPIO, SuperCap control                                      |
| `main.cpp`                | 10        | FreeRTOS `xTaskCreatePinnedToCore`, NVS, `initArduino()`, watchdog           |

### 2.4 Nix Build Architecture (current)

```
flake.nix
 ├ esp-dev.packages.esp-idf-full      → ESP-IDF v5.x + Xtensa GCC toolchain (Nix store)
 ├ nix/dependencies.nix (FOD)         → downloads managed_components (arduino-esp32, esp-libhelix-mp3)
 ├ nix/arduino-components.nix         → wraps Arduino libs (NmraDcc, ArduinoJson, audio) as IDF components
 ├ nix/common-libs.nix                → declares Arduino lib versions + GitHub pins
 ├ nix/scripts.nix                    → setup-project (symlinks), build-firmware, agent-check, flash tools
 └ default package                    → idf.py build (offline, IDF_COMPONENT_MANAGER_OFFLINE=1)
```

Key invariants to preserve: **offline, hermetic, FOD-hashed dependencies**, symlink-based
component injection, and the `agent-check` pre-submission gate.

---

## 3. Rust ESP-IDF Ecosystem Assessment

### 3.1 The `esp-rs` Stack

| Crate                                                     | Purpose                                                                | Coverage of NIMRS needs                             |
| --------------------------------------------------------- | ---------------------------------------------------------------------- | --------------------------------------------------- |
| `esp-idf-sys` (vendored at `vendor/esp-idf-sys`, v0.37.2) | Raw FFI bindings to full ESP-IDF C SDK via bindgen                     | 100% — any ESP-IDF API is reachable (unsafe)        |
| `esp-idf-svc`                                             | Safe wrappers: WiFi, HTTP server, NVS, OTA, event loop, MQTT, sockets  | ~90% of networking/storage needs                    |
| `esp-idf-hal`                                             | Safe peripheral wrappers: GPIO, ADC, I2S, I2C, SPI, LEDC, delay, units | ~70% (MCPWM + ADC-continuous-DMA need raw bindings) |
| `embedded-svc`                                            | Traits for WiFi/HTTP/fs that `esp-idf-svc` implements                  | Abstraction layer                                   |
| `anyhow` / `log` / `serde`                                | Error handling, logging, (de)serialization                             | Replaces Arduino `Log` + ArduinoJson                |

`std` **is supported** on `esp-idf` targets (unlike `no_std` `esp-hal`). The firmware uses
`std::thread`, `std::sync::Mutex`, heap allocation, and `std::fs`-style APIs today
(equivalents exist in C++ via Arduino). The `std_basics.rs` example in the vendored
`esp-idf-sys` confirms threads, TLS, and filesystem work.

### 3.2 Target & Linker Configuration (ESP32-S3 = Xtensa)

Required `.cargo/config.toml` (replaces the vendored repo's riscv default):

```toml
[build]
target = "xtensa-esp32s3-espidf"

[target.'cfg(target_os = "espidf")']
linker = "ldproxy"
rustflags = ["--cfg", "espidf_time64"]

[unstable]
build-std = ["std", "panic_abort"]
```

- `xtensa-esp32s3-espidf` is a **custom target** provided by the Xtensa Rust fork
  (`espup`-installed toolchain), not nixpkgs `rustc`.
- `ldproxy` handles ESP-IDF's linker script machinery; it ships with the `esp` toolchain.
- `build-std` is mandatory because Rust does not ship `std` for Xtensa ESP targets.
- `espidf_time64` is required for ESP-IDF ≥ 5.0 (`time_t` is 64-bit).

### 3.3 Pointing `esp-idf-sys` at the Nix ESP-IDF (the key integration)

`vendor/esp-idf-sys/build/build.rs` + `BUILD-OPTIONS.md` confirm two mechanisms that let
the Rust build **reuse the Nix-provided ESP-IDF** instead of downloading:

1. **`IDF_PATH` env var** (`idf_path` cargo metadata) — path to a pre-installed ESP-IDF.
   `esp-dev.packages.esp-idf-full` provides exactly this in the Nix store.
2. **`ESP_IDF_TOOLS_INSTALL_DIR=fromenv`** — use the _activated_ ESP-IDF environment
   (the devShell already activates it). This is the cleanest path: the Nix devShell
   exports `IDF_PATH` + toolchain, and `esp-idf-sys` consumes it verbatim.

Additional env vars to set in the devShell/build:

- `LIBCLANG_PATH` → Nix `clang` (for bindgen)
- `ESP_IDF_SDKCONFIG_DEFAULTS` → `sdkconfig.defaults` (reuse existing file)
- `ESP_IDF_COMPONENT_MANAGER=1` (keep on; needed for managed components)
- `MCU=esp32s3`

This means the **existing `sdkconfig.defaults` and `partitions.csv` are reused
unchanged** — the bootloader rollback, partition table, and 8MB flash config carry over
verbatim. (`esp-idf-sys` warns not to set `CONFIG_PARTITION_TABLE_CUSTOM_*` in
sdkconfig.defaults when using `partitions.csv`; the current file already does this
correctly and passes it via `espflash.toml` instead.)

### 3.4 Extra Components / FFI (keeping C/C++ libraries)

`esp-idf-sys` supports `extra_components` in `Cargo.toml` metadata:
`component_dirs`, `remote_component`, `bindings_header`, `bindings_module`. This lets us:

- Keep **NmraDcc** as a C++ component dir and generate bindings to its C callback API.
- Keep **esp-libhelix-mp3** as a remote component (already in `dependencies.lock`) and
  call `mp3_decoder` functions from Rust via the generated `mp3` bindings module.
- Keep **`ota_overrides.c`** as a tiny C component (the `verifyRollbackLater` weak
  override) — or, preferably, implement it directly in Rust via `esp_idf_sys` symbols.

---

## 4. Component-by-Component Feasibility

Legend: ✅ direct port · 🟡 port with effort · 🔴 FFI/rewrite required · ⚪ generated/trivial

| Module                | LOC  | Feasibility | Rust target                                  | Notes                                                                                                                                                                                                                         |
| --------------------- | ---- | ----------- | -------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `BemfEstimator`       | 123  | ✅          | pure Rust                                    | Pure f32 math; add `#[test]`s from the existing simulator.                                                                                                                                                                    |
| `DspFilters`          | 41   | ✅          | pure Rust                                    | EMA + DC-bias; trivial.                                                                                                                                                                                                       |
| `RippleDetector`      | 63   | ✅          | pure Rust                                    | Schmitt trigger on buffer; trivial.                                                                                                                                                                                           |
| `CvRegistry`          | 166  | ✅          | pure Rust                                    | Const table + `enum`; trivial.                                                                                                                                                                                                |
| `AudioUtils`          | 23   | ✅          | pure Rust                                    | Filename extension helper.                                                                                                                                                                                                    |
| `SystemContext`       | 58   | ✅          | `std::sync::Mutex<SystemState>`              | Replace ScopedLock with a Mutex guard.                                                                                                                                                                                        |
| `Logger`              | 294  | ✅          | `log` + `esp_idf_svc::log`                   | Buffered log + WiFi log task → `std::thread`.                                                                                                                                                                                 |
| `LightingController`  | 101  | ✅          | `esp-idf-hal::ledc`                          | ledc PWM; direct.                                                                                                                                                                                                             |
| `MotorController`     | 98   | ✅          | thin orchestrator                            | Wraps `MotorTask`/`MotorHal`; ports directly.                                                                                                                                                                                 |
| `BootLoopDetector`    | 177  | ✅          | `esp-idf-svc::nvs` + `ota`                   | NVS crash counter + OTA rollback; direct.                                                                                                                                                                                     |
| `main.cpp`            | 120  | ✅          | `fn main()` + `std::thread`                  | `app_main` is auto-generated by `binstart`; task pinning via `core_affinity` or raw `xTaskCreatePinnedToCore`.                                                                                                                |
| `MotorTask`           | 367  | 🟡          | `std::thread` + raw FFI                      | PI loop ports cleanly; FreeRTOS task pinning + watchdog need raw `esp_idf_sys` calls.                                                                                                                                         |
| `MotorHal`            | 208  | 🟡          | raw `esp_idf_sys`                            | MCPWM v5 + ADC + stream buffer + `IRAM_ATTR` ISR. `esp-idf-hal` lacks MCPWM v5; use raw bindings (unsafe). ISR must be `unsafe extern "C"` with no allocation.                                                                |
| `ConnectivityManager` | 1109 | 🟡          | `esp-idf-svc::wifi` + `http::server` + `ota` | Largest port. ~30 HTTP routes → `EspHttpServer` handlers. WiFi STA + reconnect → `Wifi` driver. OTA → `Ota` wrapper. LittleFS file manager → `embedded-svc::fs` or raw `esp_littlefs`.                                        |
| `DccController`       | 292  | 🔴          | Rust shell + NmraDcc FFI                     | Keep `NmraDcc` as C++ component; register C callbacks (`notifyDccSpeed` etc.) as `extern "C"` fns that update a Rust `Mutex<SystemState>`. `EEPROM` → NVS.                                                                    |
| `AudioController`     | 193  | 🔴          | Rust pipeline + libhelix FFI                 | No Rust equivalent of `arduino-audio-tools`. Build a thin Rust pipeline: `esp-idf-hal::I2s` (TX) + `esp_idf_sys::mp3` (libhelix FFI) + a Rust WAV decoder. Volume = scalar on PCM samples. `LittleFS` file source via raw FS. |
| `WebAssets.h`         | 1430 | ⚪          | `include_str!` / `include_bytes!`            | Generated HTML/CSS/JS → compile-time embedding. Cleaner than C string literals.                                                                                                                                               |
| `LameJs.h`            | gen  | ⚪          | `include_str!("../resources/lame.min.js")`   | Already fetched by Nix `lamejs` FOD; embed directly.                                                                                                                                                                          |
| `ota_overrides.c`     | 28   | ⚪          | Rust `#[no_mangle]` or C component           | `verifyRollbackLater` weak override — implement in Rust via `esp_idf_sys::bootloader` symbols, or keep as 28-line C component.                                                                                                |

### 4.1 The Two Hard Problems

#### A. DCC Protocol (NmraDcc)

NmraDcc is a ~2,000-line C++ library that attaches a GPIO interrupt, decodes the
bipolar DCC bitstream, parses packets, and invokes C callbacks
(`notifyDccSpeed`, `notifyDccFunc`, `notifyDccMsg`, `notifyCVWrite`, `notifyCVAck`).
There is **no mature Rust DCC crate for ESP32** (only hobbyist `no_std` experiments).

**Recommended: FFI bridge (Phase 2), native Rust port (Phase 5, optional).**

- Keep `NmraDcc` as an `extra_component` (C++), compiled by ESP-IDF alongside the Rust
  binary. `esp-idf-sys` builds all `extra_components` with the C toolchain.
- Expose a thin C ABI from the C++ side (`extern "C"` wrapper around `NmraDcc` methods).
- Rust side declares `extern "C"` fns for the callbacks and a `DccController` struct that
  holds a `*mut NmraDcc` handle.
- Bindings via a small `bindings.h` + `bindings_module = "dcc"`.

A future pure-Rust port is well-scoped: the DCC spec (NMRA S-9.1/S-9.2) is public and
the reference is a single-file library. ~1–2 weeks for a faithful port with tests.

#### B. Audio Pipeline (arduino-audio-tools + libhelix)

`arduino-audio-tools` provides a C++ class hierarchy (`I2SStream`, `VolumeStream`,
`EncodedAudioStream`, `MP3DecoderHelix`, `WAVDecoder`, `StreamCopy`,
`AudioFileSource`). Calling this C++ class hierarchy from Rust via FFI is painful
(virtual dispatch, `new`/`delete`, object lifetimes).

**Recommended: Reimplement a minimal Rust pipeline over `esp-idf-hal` I2S + libhelix FFI.**
The firmware uses a tiny subset of the framework:

- One I2S TX output (MAX98357A, 44.1kHz mono).
- Volume scaling (scalar on samples).
- MP3 decode (libhelix — already an ESP-IDF component) and WAV decode (trivial in Rust).
- File source from LittleFS + a copy loop.

A focused Rust replacement is ~300–400 lines:

- `I2sOutput` wrapping `esp-idf-hal::I2s` (or raw `i2s_*` if HAL lacks the needed config).
- `Mp3Decoder` wrapping `esp_idf_sys::mp3` (libhelix) via FFI.
- `WavDecoder` in pure Rust (44-byte header parser + PCM passthrough).
- `Player` struct owning the pipeline + a `play_file` / `stop` API matching the current
  `AudioController` interface.

libhelix stays as the MP3 backend (no need to port the codec); only the glue changes.

---

## 5. Nix Integration Strategy (preserving the dependency structure)

### 5.1 What stays the same

- **`esp-dev` flake input** → still provides ESP-IDF v5.x + Xtensa GCC toolchain.
- **`sdkconfig.defaults` + `partitions.csv` + `espflash.toml`** → reused verbatim.
- **`dependencies.nix` FOD** → still pins `arduino-esp32` + `esp-libhelix-mp3` managed
  components (now consumed by `esp-idf-sys`'s component manager instead of `idf.py`).
- **`agent-check` gate** → reformulated as `cargo fmt --check` + `cargo test` +
  `cargo build` + `nix build` + merge check.

### 5.2 What changes

- **`arduino-nix` + `arduino-components.nix` + `common-libs.nix`** → removed.
  Arduino libraries are no longer wrapped as IDF components. NmraDcc becomes a plain
  vendored C++ source dir (or a `fetchFromGitHub` FOD) consumed as an
  `extra_component`. ArduinoJson is replaced by `serde_json` (a Cargo crate).
- **`nix/dependencies.nix`** → repurposed: still a FOD, but it runs `esp-idf-sys`'s
  component manager to fetch managed components into the Nix store, OR we let
  `esp-idf-sys` fetch them at build time with a hashed lock file
  (`components_esp32s3.lock`). The FOD approach is more hermetic.
- **`nix/scripts.nix`** → `setup-project` simplified (no more component symlinks);
  `build-firmware` becomes `cargo espflash build` or `cargo build`; flash scripts use
  `cargo-espflash` (from Nix) instead of `idf.py`.
- **Default package** → a `crane`-based `buildRustPackage` targeting
  `xtensa-esp32s3-espidf` with `IDF_PATH` from `esp-dev`.

### 5.3 The Xtensa Rust Toolchain (critical Nix gap)

`nixpkgs` and `esp-dev` do **not** ship a Xtensa-capable `rustc`. Options, in order of
preference:

1. **`espup`-based FOD** (recommended): a Nix derivation that runs `espup install`
   into a fixed output dir, producing a Xtensa Rust toolchain + `ldproxy` + clang.
   Pinned by `outputHash`. This is the most self-contained and reproducible.
2. **Community Xtensa-Rust flake** (e.g. `qwand/esp-rust` or similar) as a flake input
   if a maintained one exists at migration time. Faster to adopt but adds a dependency.
3. **`fenix` with custom target** — not viable; fenix tracks upstream rustc which does
   not yet support Xtensa `std` targets.

The devShell must export, in addition to the existing ESP-IDF env:

- `RUSTUP_TOOLCHAIN` → the espup Xtensa toolchain
- `LIBCLANG_PATH` → Nix `clang.lib`
- `CARGO_HOME` / `RUST_SRC_PATH` as needed for `build-std`

### 5.4 Proposed `flake.nix` shape (post-migration, pre-devenv)

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    crane.url = "github:ipetkov/crane";
    # Xtensa Rust toolchain (espup FOD or community flake)
    esp-rust.url = "github:.../esp-rust";   # or a local nix/esp-rust.nix FOD
  };
  outputs = { self, nixpkgs, flake-utils, esp-dev, crane, esp-rust, ... }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        idf = esp-dev.packages.${system}.esp-idf-full;
        rust = esp-rust.packages.${system}.xtensa-toolchain;  # rustc + ldproxy
        craneLib = (crane.mkLib pkgs).overrideToolchain rust;
        commonArgs = {
          src = craneLib.cleanCargoSource ./.;
          nativeBuildInputs = [ idf pkgs.clang pkgs.pkg-config ];
          env = {
            IDF_PATH = "${idf}/esp-idf";
            ESP_IDF_TOOLS_INSTALL_DIR = "fromenv";
            LIBCLANG_PATH = "${pkgs.clang.lib}/lib";
            MCU = "esp32s3";
          };
          CARGO_BUILD_TARGET = "xtensa-esp32s3-espidf";
        };
        cargoArtifacts = craneLib.buildDepsOnly commonArgs;
      in {
        packages.default = craneLib.buildPackage (commonArgs // {
          inherit cargoArtifacts;
          # esp-idf-sys builds ESP-IDF; produces .bin/.elf via build script
        });
        devShells.default = esp-dev.devShells.${system}.esp-idf-full.overrideAttrs (_: {
          buildInputs = [ rust craneLib.cargo pkgs.cargo-espflash pkgs.clang ];
          env = commonArgs.env;
        });
      });
}
```

---

## 6. Devenv Path (future)

The `vendor/golden-template` shows two relevant templates that should be **merged** for
this project:

- `templates/embedded` — ESP-IDF + `esp-dev` devShell (current architecture).
- `templates/rust-crane` — `flake-parts` + `devenv.flakeModule` + `crane` + `fenix`.

The migration to `devenv` is **orthogonal and can follow** the language migration. Per
the golden-template `DEVENV_MIGRATION_PLAN.md` and the `rust-crane` template:

- Switch `flake.nix` to `flake-parts` + `inputs.devenv.flakeModule`.
- Add a `devenv.nix` with:
  - `languages.rust.enable = true;` + the Xtensa toolchain override
  - `env.IDF_PATH`, `env.LIBCLANG_PATH`, `env.ESP_IDF_TOOLS_INSTALL_DIR = "fromenv";`
    (per the AGENTS.md pitfall: `inputsFrom` does **not** propagate env vars — set them
    explicitly in `devenv.nix` `env`).
  - `git-hooks.hooks.{rustfmt,clippy}.enable = true;`
  - `cachix.enable = false;` (per pitfall #3, if the system nix daemon manages caches).
- Keep `crane` for `nix build` / `checks`; `devenv` for the interactive shell.

**Recommendation:** do the C++→Rust language migration first on the existing
`flake-utils` + `esp-dev` shell (one variable at a time), then adopt `devenv` as a
second, mechanical step. This avoids debugging language + tooling simultaneously.

---

## 7. Risks & Mitigations

| Risk                                                                               | Severity | Mitigation                                                                                                                                                                      |
| ---------------------------------------------------------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Xtensa Rust toolchain in Nix** is fragile / unmaintained                         | High     | Pin via FOD with `outputHash`; mirror the toolchain tarball in a cache. Fallback to `espup` outside Nix for dev, FOD for CI.                                                    |
| **`esp-idf-sys` build in Nix sandbox** may try to download tools despite `fromenv` | High     | Set `ESP_IDF_TOOLS_INSTALL_DIR=fromenv` + `IDF_PATH` explicitly; verify with `--offline`-style cargo flags. Mirror any remaining network fetches in a FOD.                      |
| **MCPWM v5 raw bindings** are verbose / unsafe-heavy in Rust                       | Medium   | Encapsulate in a `motor_hal` module with a safe wrapper; keep the ISR in `unsafe extern "C"`; port the existing `motor-sim` test to validate behavior.                          |
| **NmraDcc FFI** callback wiring is awkward                                         | Medium   | Minimal C++ shim exposing `extern "C"` init/process + register-callback fns; Rust owns the `SystemState` mutex the callbacks mutate.                                            |
| **`ConnectivityManager` (1109 LOC)** is the single largest port                    | Medium   | Port route-by-route behind a feature flag; run the existing `test_ConnectivityManager.cpp` logic as Rust integration tests against `EspHttpServer`.                             |
| **OTA rollback weak override** (`verifyRollbackLater`)                             | Low      | Implement as a Rust `#[no_mangle] unsafe extern "C" fn` that calls `esp_idf_sys::esp_ota_mark_app_valid_cancel_rollback`, or keep the 28-line `ota_overrides.c` as a component. |
| **Binary size** — Rust `std` + ESP-IDF may exceed 2MB app slot                     | Medium   | Monitor with the existing `tools/check_firmware_size.py`; tune `lto = "fat"` + `codegen-units = 1` + `opt-level = "z"`; trim `esp_idf_components` to only what's needed.        |
| **`build-std` + crane caching** — incremental builds slower                        | Low      | `craneLib.buildDepsOnly` handles dependency caching; accept that ESP-IDF itself is only built once per config.                                                                  |
| **bindgen/LIBCLANG_PATH** mismatches in Nix sandbox                                | Low      | Set `LIBCLANG_PATH` to `${pkgs.llvmPackages.libclang.lib}/lib` in both `nativeBuildInputs` and devShell env (per AGENTS pitfall #5).                                            |

---

## 8. Recommended Approach: Hybrid Phased Migration

**Do not** attempt a single-revision rewrite. Migrate in vertical slices, each producing
a flashable, testable firmware. Keep the C++ build green until the final cutover by
maintaining a `cpp/` legacy tree and a `rust/` tree in parallel during the transition,
gated by a flake output switch.

### Guiding principles

1. **Pure logic first** — port math/data modules with unit tests; zero hardware risk.
2. **ESP-IDF-native next** — WiFi, NVS, OTA, logging via `esp-idf-svc`.
3. **FFI bridges for the two hard libs** — keep them C/C++ until Rust replacements are
   validated on hardware.
4. **Reuse `sdkconfig.defaults` / `partitions.csv`** throughout — only the build driver
   changes (idf.py → cargo + esp-idf-sys build script).
5. **Preserve `agent-check`** semantics at every phase.

---

## 9. Full Migration Plan

### Phase 0 — Spike & Tooling Validation (1 week)

**Goal:** prove the Nix + Xtensa Rust + `esp-idf-sys` + Nix-ESP-IDF chain end-to-end with
a "hello world" binary that blinks an LED and prints over USB.

- [ ] Create `nix/esp-rust.nix` FOD (or add an `esp-rust` flake input) producing the
      Xtensa Rust toolchain + `ldproxy`.
- [ ] Add `.cargo/config.toml` with `xtensa-esp32s3-espidf` target, `ldproxy`, `build-std`,
      `espidf_time64`.
- [ ] Add a minimal `Cargo.toml` with `esp-idf-sys` (path = `vendor/esp-idf-sys` or a
      crates.io pin) + `[package.metadata.esp-idf-sys]` pointing at `sdkconfig.defaults`.
- [ ] Wire `flake.nix` to export `IDF_PATH` / `ESP_IDF_TOOLS_INSTALL_DIR=fromenv` /
      `LIBCLANG_PATH` from `esp-dev` + Nix clang.
- [ ] `nix build` produces a flashable `xtensa` binary; flash with `cargo-espflash`.
- [ ] Validate `std` threads + `println!` work (mirror `std_basics.rs` example).

**Exit criteria:** `nix build` + `cargo-espflash flash` yields a running LED-blink binary
built entirely from Nix-pinned inputs.

### Phase 1 — Foundation Modules (1 week)

Port the pure-logic and data modules with unit tests. No hardware touched.

- [ ] `CvRegistry` → `crate::cv::{CvDef, CV_DEFS, Cv enum}`
- [ ] `DspFilters` → `crate::dsp::{EmaFilter, DcBiasRemover}` + `#[test]`s
- [ ] `BemfEstimator` → `crate::motor::bemf::BemfEstimator` + `#[test]`s (reuse
      `tests/test_PID_Simulator.cpp` scenarios)
- [ ] `RippleDetector` → `crate::motor::ripple::RippleDetector` + `#[test]`s
- [ ] `AudioUtils` → `crate::audio::utils`
- [ ] `SystemContext` → `crate::ctx::{SystemState, SystemContext}` using
      `std::sync::Mutex`
- [ ] `WebAssets` → `include_str!` / `include_bytes!` from `resources/` (move the
      generated HTML/CSS/JS out of the header into real files)
- [ ] `LameJs` → `include_str!` of the Nix-fetched `lame.min.js`
- [ ] Set up `cargo test` as the host test runner; update `agent-check`.

**Exit criteria:** `cargo test` green; `nix flake check` green; modules compile for host
and for `xtensa-esp32s3-espidf`.

### Phase 2 — Hardware Abstraction & Motor Control (2 weeks)

- [ ] `MotorHal` → `crate::motor::hal` using raw `esp_idf_sys` MCPWM v5 + ADC1 +
      stream buffer. Safe wrapper around the unsafe ISR (`IRAM_ATTR` →
      `unsafe extern "C" fn` registered via `mcpwm_timer_register_event_callbacks`).
- [ ] `MotorTask` → `crate::motor::task` as a `std::thread` pinned to core 1
      (raw `xTaskCreatePinnedToCore` or `core-affinity` crate). PI loop reuses Phase 1
      estimators.
- [ ] `MotorController` → thin orchestrator.
- [ ] `LightingController` → `crate::lighting` via `esp-idf-hal::ledc`.
- [ ] Port `motor-sim` to a Rust binary (`examples/motor_sim.rs`) driven by the same
      `#[test]` scenarios.

**Exit criteria:** motor spins under Rust control; telemetry dashboard works; `motor-sim`
matches the C++ simulator results within tolerance.

### Phase 3 — System Services (2 weeks)

- [ ] `Logger` → `log` facade + `esp_idf_svc::log::EspLogger` + a WiFi log streaming
      `std::thread` (replaces `Logger.cpp`'s log task).
- [ ] `BootLoopDetector` → `crate::boot` using `esp-idf-svc::nvs::Nvs` + `ota` APIs.
- [ ] `ConnectivityManager` → `crate::net`:
  - WiFi STA + reconnect via `esp-idf-svc::wifi::Wifi`
  - `EspHttpServer` with the ~30 routes (port the `@api` docs alongside)
  - OTA upload handler via `esp-idf-svc::ota::Ota`
  - LittleFS file manager via raw `esp_littlefs` (`esp_idf_sys`) or `embedded-svc::fs`
  - Auth/digest via a small Rust middleware
- [ ] `main` → `fn main()` with `std::thread::spawn` for the control-plane task
      (core 0) and the Arduino-loop equivalent.

**Exit criteria:** full WiFi web UI + OTA + config persistence working from Rust; the
`test_Security*.cpp` scenarios ported to Rust integration tests.

### Phase 4 — FFI Bridges: DCC + Audio (2 weeks)

- [ ] **DCC:** vendor `NmraDcc` (fetchFromGitHub FOD) as an `extra_component` with a
      `bindings.h` exposing `extern "C"` init/process/register-callbacks. Rust
      `crate::dcc` declares the `extern "C"` callbacks that lock the `SystemContext`
      mutex. `EEPROM` semantics move to NVS.
- [ ] **Audio:** add `esp-libhelix-mp3` as a `remote_component` extra component; write
      `crate::audio::player` with:
  - `I2sOutput` over `esp-idf-hal::I2s` (or raw `i2s_*`)
  - `Mp3Decoder` FFI wrapper around libhelix
  - `WavDecoder` in pure Rust
  - `Player::play_file` / `stop` + the function-key→asset mapping (ported from
    `AudioController.cpp`'s `loop()`)
- [ ] `ota_overrides.c` → Rust `#[no_mangle]` or keep as a 28-line C component.

**Exit criteria:** DCC packets control speed/functions; sound assets play on function
keys; OTA rollback still functions.

### Phase 5 — Cutover & Cleanup (1 week)

- [ ] Remove `main/` (C++), `nix/arduino-*.nix`, `arduino-nix`/`arduino-indexes` inputs.
- [ ] Delete `temp_main_arduinojson.h`, `NIMRS-Firmware.ino`, old `MIGRATION_PLAN.md`.
- [ ] Update `README.md`, `DESIGN.md`, `AGENTS.md` to reflect Rust build
      (`cargo build`, `cargo-espflash`, `cargo test`).
- [ ] Update `treefmt.toml` (drop C++ formatters, add `rustfmt`).
- [ ] Retire the Python `test_runner.py` in favor of `cargo test`.
- [ ] Single `agent-check` that runs `cargo fmt --check` + `cargo clippy` +
      `cargo test` + `nix build` + merge check.

**Exit criteria:** `agent-check` green; no C++ source remains (except the intentionally
retained NmraDcc component and optional `ota_overrides.c`); `nix build` produces the
production firmware.

### Phase 6 — (Optional) Pure-Rust DCC Port (later)

- [ ] Port NmraDcc to `crate::dcc::native` (ESP32 GPIO interrupt via `esp-idf-hal` +
      a bit-decode state machine). Remove the C++ `extra_component`.
- [ ] Remove the last C/C++ source from the tree.

### Phase 7 — (Optional) Devenv Adoption (later)

- [ ] Migrate `flake.nix` to `flake-parts` + `devenv.flakeModule` per the golden-template
      `rust-crane` template, preserving the `esp-dev` ESP-IDF env via explicit `env`
      blocks (per the AGENTS.md pitfall notes).
- [ ] Add `devenv.nix` with `languages.rust`, git-hooks, and the ESP-IDF env vars.

---

## 10. Effort Estimate

| Phase                              | Duration     | Risk                              |
| ---------------------------------- | ------------ | --------------------------------- |
| 0 — Spike & tooling                | 1 week       | High (Nix + Xtensa Rust unknowns) |
| 1 — Foundation modules             | 1 week       | Low                               |
| 2 — Motor/HAL                      | 2 weeks      | Medium (MCPWM raw bindings + ISR) |
| 3 — System services (WiFi/OTA/Web) | 2 weeks      | Medium (largest LOC port)         |
| 4 — DCC + Audio FFI                | 2 weeks      | High (two hard libraries)         |
| 5 — Cutover & cleanup              | 1 week       | Low                               |
| **Total (Phases 0–5)**             | **~9 weeks** |                                   |
| 6 — Pure-Rust DCC (optional)       | +1–2 weeks   | Medium                            |
| 7 — Devenv (optional)              | +1 week      | Low                               |

Assumes one engineer with Rust + ESP-IDF + Nix familiarity, working on hardware
throughout. Phases 2–4 require periodic on-device validation.

---

## 11. Decisions Required

1. **NmraDcc:** FFI bridge now (recommended) vs. pure-Rust port immediately?
2. **Audio:** thin Rust pipeline over libhelix FFI (recommended) vs. keep
   `arduino-audio-tools` as a C++ component with heavy FFI?
3. **Xtensa Rust toolchain:** `espup` FOD in-repo (recommended) vs. adopt a community
   flake input?
4. **Devenv timing:** adopt now (concurrent with language migration) vs. after
   (recommended — decouple tooling churn from language churn)?
5. **Parallel trees during migration:** keep `main/` (C++) and `rust/` side-by-side with
   a flake output switch (recommended) vs. in-place rewrite on a long-lived branch?

---

## 12. Artifacts Referenced

- `vendor/esp-idf-sys/` — vendored `esp-idf-sys` v0.37.2 (raw bindings + build system)
- `vendor/esp-idf-sys/BUILD-OPTIONS.md` — authoritative config reference
  (`IDF_PATH`, `ESP_IDF_TOOLS_INSTALL_DIR=fromenv`, `extra_components`)
- `vendor/golden-template/templates/{embedded,rust-crane}/` — Nix/devenv templates to merge
- `vendor/golden-template/DEVENV_MIGRATION_PLAN.md` — devenv adoption playbook
- `flake.nix`, `nix/dependencies.nix`, `nix/arduino-components.nix` — current Nix structure
- `sdkconfig.defaults`, `partitions.csv`, `espflash.toml` — reused unchanged
- `main/src/*.cpp` — the 18 modules inventoried in §2.1
- `DESIGN.md` — motor-control architecture (carries over unchanged)
- `tests/` — existing host tests + motor simulator (port targets in Phases 1–2)
