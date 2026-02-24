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

## 3. Component & Library Management

- [ ] **Single Source of Truth**: Use `common-libs.nix` to fetch libraries for both Arduino and IDF builds.
  - Update `flake.nix` to import `common-libs.nix`.
  - In the Nix build/shell, auto-populate `components/` with libraries from `common-libs.nix`.
  - Auto-generate `CMakeLists.txt` for these libraries if missing.
- [ ] **NmraDcc**:
  - Will be handled via `common-libs.nix` (it is listed there).
- [ ] **ArduinoJson**:
  - Will be handled via `common-libs.nix`.
- [ ] **Arduino Core (`arduino-esp32`)**:
  - Use `main/idf_component.yml` to depend on `espressif/arduino-esp32` (standard IDF way for the core).
  - _Alternative:_ If `common-libs.nix` can provide the core, we might need to handle it. But usually IDF component is preferred for the core.

## 4. Configuration (`sdkconfig`)

- [ ] Enable `CONFIG_AUTOSTART_ARDUINO=y` in `sdkconfig.defaults`.
- [ ] Configure Partition Table (`partitions.csv`) in `sdkconfig.defaults` (`CONFIG_PARTITION_TABLE_CUSTOM=y`, etc.).
- [ ] Enable Rollback/OTA features.

## 5. Nix Integration

- [ ] Update `flake.nix` / `devShell`:
  - Ensure `esp-idf` tools (`idf.py`, `cmake`, `ninja`) are in the shell (provided by `esp-dev` input).
- [ ] Create/Update `build-idf.nix` (or similar):
  - Replicate the build command using `idf.py build`.
  - Ensure the build is hermetic (handling `IDF_PATH` correctly).

## 6. Cleanup

- [ ] Remove `build-command.nix` (old arduino-cli).
- [ ] Remove `platformio.ini` (if no longer used).
