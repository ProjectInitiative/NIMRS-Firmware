# NIMRS Hardware Pinout

## ESP32-S3 GPIO Mapping

This table maps the ESP32-S3 GPIO pins to the NIMRS-21Pin-Decoder functions and the 21-Pin MTC (NEM 660) interface.

| GPIO | Function | Net Name | Description |
| :--- | :--- | :--- | :--- |
| **0** | **DCC Input** | `TRACK_RIGHT_3V3` | Main DCC Signal Input (via Divider) |
| **1** | **DCC Input** | `TRACK_LEFT_3V3` | Secondary DCC Input (via Divider) |
| **4** | **Sensor** | `MOTOR_CURRENT` | Motor Current Sense (Analog) |
| **5** | **Sensor** | `+V_SENSE_3V3` | System Voltage Sense (Analog) |
| **8** | **AUX** | `AUX1` | Aux Output 1 (Transistor Driven) |
| **9** | **AUX** | `AUX2` | Aux Output 2 (Transistor Driven) |
| **10** | **Light** | `REAR` | Rear Headlight (Transistor Driven) |
| **12** | **Light** | `FRONT` | Front Headlight (Transistor Driven) |
| **34** | **Motor** | `MOTOR_GAIN_SEL` | DRV8213 Gain Select |
| **39** | **Power** | `VMOTOR_PG` | Motor Power Good Signal |
| **40** | **Motor** | `MOTOR_2` | Motor Drive IN2 |
| **41** | **Motor** | `MOTOR_1` | Motor Drive IN1 |
| **38** | **Audio** | `AMP_BCLK` | I2S Bit Clock |
| **37** | **Audio** | `AMP_DIN` | I2S Data In |
| **36** | **Audio** | `AMP_LRCLK` | I2S Word Select |
| **33** | **Audio** | `AMP_SD_MODE` | Amplifier Shutdown/Mode |

## 21-Pin MTC Interface

| Pin | Function | GPIO |
| :--- | :--- | :--- |
| 1 | Input 1 / Aux 7 | GPIO 7 |
| 2 | Input 2 / Aux 8 | GPIO 6 |
| 7 | Rear Light | GPIO 10 |
| 8 | Front Light | GPIO 12 |
| 14 | Aux 2 | GPIO 9 |
| 15 | Aux 1 | GPIO 8 |
