# Flashing Guide

This guide explains how to flash the NIMRS Firmware onto the decoder using pre-compiled binaries.

## Prerequisites

1.  **USB-to-Serial Adapter:** 3.3V Logic Level.
2.  **Drivers:** Installed for your adapter (CH340, CP210x, etc.).
3.  **Esptool:** Python-based flashing tool.
    - Install Python 3.
    - Run: `pip install esptool`

## Download Firmware

Go to the [GitHub Releases](../../releases) page and download the following files from the latest release:

1.  `nimrs-firmware.bin` (Application)
2.  `partitions.bin` (Partition Table)
3.  `bootloader.bin` (System Bootloader)

## Wiring

Connect the decoder to your USB Serial Adapter:

- **VCC** -> 3.3V (Do NOT use 5V)
- **GND** -> GND
- **TX** -> RX
- **RX** -> TX
- **IO0** -> GND (Held during power-up for bootloader mode)

## Flashing Command

Open a terminal/command prompt in the folder where you downloaded the files and run:

```bash
esptool.py -p COM3 -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x0000 bootloader.bin 0x8000 partitions.bin 0x10000 nimrs-firmware.bin
```

- Replace `COM3` with your actual serial port (e.g., `/dev/ttyUSB0` on Linux/Mac).
- If flashing fails, try a lower baud rate (`-b 115200`).

## Post-Flash

1.  Disconnect **IO0** from GND.
2.  Reset the board (Cycle power).
3.  The Blue LED (if present on adapter) should blink as data is sent.
4.  Connect to the `NIMRS-Decoder` WiFi Access Point.
