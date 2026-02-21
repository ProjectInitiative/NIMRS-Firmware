# NIMRS-21Pin-Decoder Firmware

## Overview

This project contains the firmware for the open-source [NIMRS-21Pin-Decoder](https://github.com/CDFER/NIMRS-21Pin-Decoder), based on the ESP32-S3 microcontroller. The firmware is built using the Arduino framework on top of ESP-IDF, leveraging a reproducible Nix-based development environment.

## NIMRS-21Pin-Decoder Project Roadmap

### 1. Hardware Integration & Core Control

- **Motor Driver (DRV8213):** Implement PWM drive (20-40kHz). Integrate `IPROPI` analog sensing for load-responsive sound.
- **BEMF Sensing:** Develop the "Sampling Window" logic to read motor voltage for speed synchronization and "Virtual Cam" chuff timing.
- **Audio (MAX98357A):** Set up I2S communication. Implement an audio mixer to handle concurrent streams (WAV + MP3).
- **Smoke Unit:** Map Aux 1 (Heater) and Aux 2 (Fan). Create PWM logic for "Puff" velocity and idle drift.

### 2. Firmware Architecture & Libraries

- **DCC Protocol:** Port/optimize DCC signal decoding via interrupts.
- **Audio Engine:**
  - **Low Latency:** Use internal flash (LittleFS) for WAV triggers (Whistle, Chuffs).
  - **Streaming:** Implement MP3/AAC decoding for long-running ambient sounds/dialogue.
- **Logic Loop:** Create a state machine to sync BEMF speed -> Audio Pitch/Frequency -> Smoke Pulse intensity.

### 3. Memory & Connectivity

- **Partition Table:** Configure 8MB Flash (1.5MB OTA App0, 1.5MB OTA App1, ~5MB LittleFS Storage).
- **WiFi Web Server:**
  - **OTA Updates:** Secure firmware update path.
  - **File Manager:** Web interface for uploading/deleting sound files to LittleFS.
  - **Configuration:** Web-based CV editing and motor tuning.

### 4. Safety & Refinement

- **Thermal Management:** Monitor DRV8213 and Smoke heater duty cycles.
- **Dry-Run Protection:** Implement a software timer to shut off smoke if no fluid is detected/after X minutes.
- **BEMF Calibration:** Auto-tune routine for matching motor characteristics to scale speeds.

### 5. Advanced Standards & Safety (Brainstorming)

- **NixOS Build Pipeline:** Formalize a reproducible dev shell with ESP-IDF and PlatformIO for open-source contributors.
- **RailCom Integration:** Implement bi-directional feedback during DCC "cutouts".
- **Advanced Lighting:** State machine for Rule 17 (dimming) and Mars Light/Gyralight PWM patterns.
- **Keep-Alive Management:** Software logic to handle power-loss states and capacitor discharge management.
- **Consist Management:** Support for CV19 (Consist Addressing) and specialized physics-based control when multi-heading.
- **Thermal & Current Safety:** High-priority "Failsafe" task monitoring IPROPI and internal temp to prevent hardware damage.

## Development Environment

We use `arduino-nix` to provide a hermetic and reproducible toolchain.

### Prerequisites

- **Nix**: The package manager.
- **Direnv**: Automatically loads the environment.

### Getting Started

```bash
cd vendor/NIMRS-Firmware
direnv allow
setup-local-source
build-firmware
```

## Flashing & Monitoring

### Initial Flashing (Manual Bootloader)

If the device is blank or stuck in a loop, you may need to manually enter "Download Mode":

1.  Connect **IO0** (GPIO 0 / BOOT) to **GND**.
2.  Connect the programmer/Apply Power.
3.  Run the upload command:
    ```bash
    upload-firmware /dev/ttyUSB0
    ```
4.  Once the upload starts (or finishes), disconnect IO0 from GND and power cycle the board.

### Serial Monitor

To monitor the device without triggering the DTR/RTS reset signals (which can hold the chip in reset):

```bash
monitor-firmware /dev/ttyUSB0
```

This command automatically disables DTR and RTS.

## Architecture

- **Platform:** ESP32-S3 (`esp32:esp32:esp32s3`)
- **Flash Size:** 8MB (Quad SPI)
- **Framework:** Arduino (via `arduino-esp32` core 3.x)
- **Partitioning:** Custom 8MB layout (see `partitions.csv`)

### Partition Layout (8MB)

We use a custom partition table to support large firmware sizes and ample storage for audio files:

- **App Slots (2MB x 2):** Two identical partitions (`app0`, `app1`) enable safe A/B Over-The-Air (OTA) updates.
- **File System (~3.9MB):** A large `spiffs` partition (mounted as LittleFS) for storing WAV/MP3 files and configuration data.
- **NVS (20KB):** Non-volatile storage for WiFi credentials and persistent settings.

## Sensorless Motor Control

The NIMRS firmware now implements a hybrid sensorless motor control strategy combining Back-EMF estimation and Commutation Ripple counting.

### Configuration

To enable accurate speed control, you must configure the following CVs:

- **CV 149 (Armature Resistance):** Measure the resistance of your motor armature (in 10mOhm units).
  - Example: 2.0 Ohms = Value `200`.
  - **How to measure:** Use a multimeter across the motor terminals (disconnect decoder). Rotate the motor slowly and take the average of several readings.
- **CV 145 (Track Voltage):** Set the track voltage (in 100mV units).
  - Example: 14.0V = Value `140`.
- **CV 143 (Motor Poles):** Number of motor poles (usually 3 or 5). Default is 5.

See [DESIGN.md](DESIGN.md) for detailed architecture.
