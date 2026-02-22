# NIMRS-21Pin-Decoder Firmware

## Overview

This project contains the firmware for the open-source [NIMRS-21Pin-Decoder](https://github.com/CDFER/NIMRS-21Pin-Decoder), based on the ESP32-S3 microcontroller. The firmware is built using the Arduino framework on top of ESP-IDF.

## Project Roadmap

### 1. Hardware Integration & Core Control

- **Motor Driver (DRV8213):** Implement PWM drive (20-40kHz). Integrate `IPROPI` analog sensing for load-responsive sound.
- **Estimated BEMF (Sensorless):** Due to hardware limitations (no voltage divider), we use a **Model-Based Estimator** using current sensing (`IPROPI`) to calculate Back-EMF and virtual speed.
- **Audio (MAX98357A):** Set up I2S communication. Implement an audio mixer to handle concurrent streams (WAV + MP3).
- **Smoke Unit:** Map Aux 1 (Heater) and Aux 2 (Fan). Create PWM logic for "Puff" velocity and idle drift.

### 2. Firmware Architecture & Libraries

- **DCC Protocol:** Port/optimize DCC signal decoding via interrupts.
- **Audio Engine:**
  - **Low Latency:** Use internal flash (LittleFS) for WAV triggers (Whistle, Chuffs).
  - **Streaming:** Implement MP3/AAC decoding for long-running ambient sounds/dialogue.
- **Logic Loop:** Create a state machine to sync Estimated Speed -> Audio Pitch/Frequency -> Smoke Pulse intensity.

### 3. Memory & Connectivity

- **Partition Table:** Configure 8MB Flash (2MB OTA App0, 2MB OTA App1, ~4MB LittleFS Storage).
- **WiFi Web Server:**
  - **OTA Updates:** Secure firmware update path.
  - **File Manager:** Web interface for uploading/deleting sound files to LittleFS.
  - **Configuration:** Web-based CV editing and motor tuning.

### 4. Safety & Refinement

- **Thermal Management:** Monitor DRV8213 and Smoke heater duty cycles.
- **Dry-Run Protection:** Implement a software timer to shut off smoke if no fluid is detected.
- **Auto-Tuning:** Routine for matching motor characteristics (Resistance, Ke) to the estimator.

## Building & Flashing

We provide detailed instructions for building the firmware using either the reproducible Nix environment (recommended) or standard tools like PlatformIO/Arduino IDE.

- **[Building with Nix](docs/build-with-nix.md)** (Recommended)
- **[Building without Nix (PlatformIO/Arduino)](docs/build-without-nix.md)**

## Flashing & Monitoring

See the build guides above for specific instructions.

### Initial Flashing (Manual Bootloader)

If the device is blank or stuck in a loop, you may need to manually enter "Download Mode":

1.  Connect **IO0** (GPIO 0 / BOOT) to **GND**.
2.  Connect the programmer/Apply Power.
3.  Upload using `upload-firmware` (Nix) or PlatformIO "Upload".
4.  Disconnect IO0 and power cycle.

## Architecture

- **Platform:** ESP32-S3 (`esp32:esp32:esp32s3`)
- **Flash Size:** 8MB (Quad SPI)
- **Framework:** Arduino (via `arduino-esp32` core 3.x)
- **Partitioning:** Custom 8MB layout (see `partitions.csv`)

### Partition Layout (8MB)

- **App Slots (2MB x 2):** Two identical partitions (`app0`, `app1`) enable safe A/B Over-The-Air (OTA) updates.
- **File System (~4MB):** A large `spiffs` partition (mounted as LittleFS) for storing WAV/MP3 files and configuration data.
- **NVS (20KB):** Non-volatile storage for WiFi credentials and persistent settings.

## Sensorless Motor Control

The NIMRS firmware implements a hybrid sensorless motor control strategy. Since the hardware lacks a voltage divider for direct BEMF measurement, we rely on **Current Sensing (IPROPI)** and a **Mathematical Motor Model** to estimate speed and load.

### Configuration

To enable accurate speed control, you must configure the following CVs:

- **CV 149 (Armature Resistance):** Measure the resistance of your motor armature (in 10mOhm units).
  - Example: 2.0 Ohms = Value `200`.
  - **How to measure:** Use a multimeter across the motor terminals (disconnect decoder). Rotate the motor slowly and take the average of several readings.
- **CV 145 (Track Voltage):** Set the track voltage (in 100mV units).
  - Example: 14.0V = Value `140`.
- **CV 143 (Motor Poles):** Number of motor poles (usually 3 or 5). Default is 5.

## API Reference

A full API reference is available in [docs/API.md](docs/API.md).

See [DESIGN.md](DESIGN.md) for detailed architecture.
