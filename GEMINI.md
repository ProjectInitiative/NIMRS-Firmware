# NIMRS-21Pin-Decoder Firmware

## Overview
This project contains the firmware for the open-source NIMRS-21Pin-Decoder, based on the ESP32-S3 microcontroller. The firmware is built using the Arduino framework on top of ESP-IDF, leveraging a reproducible Nix-based development environment.

## Development Environment
We use `arduino-nix` to provide a hermetic and reproducible toolchain. This ensures that every developer uses the exact same version of the ESP32 core, compiler (`avr-gcc` equivalent for xtensa/risc-v), and tools.

### Prerequisites
-   **Nix**: The package manager.
-   **Direnv**: Automatically loads the environment when you enter the directory.

### Getting Started
1.  **Enter the directory:**
    ```bash
    cd vendor/NIMRS-Firmware
    direnv allow
    ```
2.  **Initialize Local Workspace:**
    This command copies the source code to a writable directory (`.direnv/src`) where you can make changes. It prevents polluting the clean source tree with build artifacts.
    ```bash
    setup-local-source
    ```

### Workflow
*   **Build:**
    Compiles the code in your local workspace.
    ```bash
    build-firmware
    ```
*   **Upload:**
    Flashes the compiled binary to the connected device.
    ```bash
    upload-firmware /dev/ttyACM0
    ```
*   **Clean Release Build:**
    Performs a clean build from the source directory using a sandboxed environment. This guarantees reproducibility.
    ```bash
    nix build
    ```

## Architecture
-   **Platform:** ESP32-S3 (`esp32:esp32:esp32s3`)
-   **Framework:** Arduino (via `arduino-esp32` core 2.0.14)
-   **Configuration:** `config.h` (copied from `config.example.h`)

## Plan
1.  **Basic IO:** Verify LED blinking and Serial output.
2.  **DCC Signal:** Implement `NmraDcc` library to read DCC packets.
3.  **Motor Control:** Implement PWM control for the motor driver.
4.  **WiFi/OTA:** Enable wireless debugging and Over-The-Air updates.
