# Building without Nix

While we recommend the Nix environment for reproducibility, you can also build the firmware using standard tools like PlatformIO or the Arduino IDE.

## Method 1: PlatformIO (Recommended)

This repository includes a `platformio.ini` file configured for the ESP32-S3 and the project's specific partition layout.

### Prerequisites

1.  **Install VS Code** and the **PlatformIO IDE** extension.
2.  **Clone the repository.**

### Steps

1.  Open the project folder in VS Code.
2.  Wait for PlatformIO to initialize and download dependencies (`NmraDcc`, `ArduinoJson`, etc.).
3.  Click the **Build** (check) or **Upload** (arrow) button in the bottom status bar.

### Configuration Notes

- **Board:** `esp32-s3-devkitc-1`
- **Partitions:** The build automatically uses `partitions.csv` (8MB Flash, 2x 2MB App slots, ~4MB LittleFS).
- **Filesystem:** Uses LittleFS.

## Method 2: Arduino IDE

You can use the legacy Arduino IDE, but you must manually manage libraries and settings.

### Prerequisites

1.  **Install Arduino IDE 2.0+**.
2.  **Install ESP32 Board Support:**
    - Add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to **Additional Board Manager URLs**.
    - Install `esp32` by Espressif Systems via the Board Manager.

### Library Dependencies

Install the following libraries via **Sketch > Include Library > Manage Libraries**:

- `NmraDcc` (v2.0.17+)
- `WiFiManager` (v2.0.17+)
- `ArduinoJson` (v7.3.0+)
- `ESP8266Audio` (v1.9.7+)

### Board Settings

- **Board:** "ESP32S3 Dev Module"
- **Flash Size:** "8MB (64Mb)"
- **Partition Scheme:** You must select a scheme that supports at least **2MB** for the application.
  - _Note: The project uses a custom `partitions.csv`. To use it in Arduino IDE, you may need to place it in the variant folder or select "Custom" if your board definition supports it. Otherwise, select "8M with SPIFFS" (ensure APP is >1.8MB)._
- **USB Mode:** "Hardware CDC and JTAG"
- **USB CDC On Boot:** "Enabled" (Required for Serial Monitor)

### Building

1.  Open `NIMRS-Firmware.ino`.
2.  Click **Verify** or **Upload**.
