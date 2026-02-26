# Migration Plan: Arduino CLI to ESP-IDF (Nix-driven)

## Goal

Convert the build system from `arduino-cli` to native ESP-IDF (`idf.py`) while maintaining the Nix-based reproducible environment. This enables advanced features like the "Rollback" bootloader.

## 1. Project Restructuring (In Progress)

- [x] Create `main/` directory (Standard IDF structure).
- [x] Move `NIMRS-Firmware.ino` to `main/main.cpp`.
- [x] Move `src/` to `main/src/`.
- [x] Move `config.h` to `main/config.h`.
- [ ] Create `components/` directory for vendored libraries.

## 2. CMake Build Configuration

- [ ] **Root `CMakeLists.txt`**: Standard IDF project entry.
- [ ] **`main/CMakeLists.txt`**:
  - Use `file(GLOB_RECURSE SOURCES "src/*.cpp")` to automatically find source files (requested by user).
  - Register `main` component with these sources.
  - Define dependencies (`REQUIRES arduino-esp32 NmraDcc ArduinoJson`).

## 3. Component & Library Management (Completed)

- [x] **Nix-Native Dependency Management**:
  - **Old Way (arduino-cli)**: `arduino-nix` was used to download libraries to a location where the `arduino-cli` wrapper could find them. The build orchestration was left entirely to `arduino-cli`, which lacks native CMake integration and fine-grained control over the build process (like menuconfig).
  - **New Way (ESP-IDF Native)**: We leverage the official ESP-IDF build system (`idf.py` / CMake).
    - **Arduino Libraries**: `nix/arduino-components.nix` downloads standard Arduino libraries (like NmraDcc, ArduinoJson, ESP8266Audio) directly from the Nix store. Crucially, it dynamically generates a `CMakeLists.txt` for each library and applies any necessary patches (like the I2S fix for ESP8266Audio) _before_ ESP-IDF sees them. This transforms standard Arduino libraries into native, hermetic ESP-IDF components.
    - **Managed Components**: ESP-IDF registry components (like `arduino-esp32`, `esp-libhelix-mp3`) are securely vendored in the Nix store via `nix/dependencies.nix` (a Fixed-Output Derivation).
    - **Symlink Strategy**: Instead of polluting the project directory with generated `dependencies.cmake` files or copied components, `nix/scripts.nix` (via `setup-project`) simply creates clean symlinks (`components/` and `managed_components/`) pointing directly to the immutable Nix store derivations.
    - **Offline Mode**: `idf.py` is configured to run entirely offline (`IDF_COMPONENT_MANAGER_OFFLINE=1`), ensuring it only uses the exact, hashed dependencies provided by Nix, guaranteeing strict reproducibility.

## 4. Configuration (`sdkconfig`)

- [ ] Enable `CONFIG_AUTOSTART_ARDUINO=y` in `sdkconfig.defaults`.
- [ ] Configure Partition Table (`partitions.csv`) in `sdkconfig.defaults` (`CONFIG_PARTITION_TABLE_CUSTOM=y`, etc.).
- [ ] Enable Rollback/OTA features.

## 5. Nix Integration (Completed)

- [x] Update `flake.nix` / `devShell`:
  - Ensure `esp-idf` tools (`idf.py`, `cmake`, `ninja`) are in the shell (provided by `esp-dev` input).
- [x] Unified Hermetic Build:
  - The default package derivation in `flake.nix` successfully runs the entire `idf.py build` process offline, utilizing the injected component paths and producing the final `.bin` and `.elf` artifacts cleanly in the `result/` directory.

## 6. Cleanup (In Progress)

- [x] Remove `build-command.nix` (old arduino-cli).
- [x] Remove `platformio.ini` (if no longer used).
- [x] Remove `dependencies.cmake` generation.
