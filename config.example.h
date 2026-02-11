#ifndef CONFIG_H
#define CONFIG_H

// --- Pin Definitions (From PCB v1.0) ---

// Status LED (Optional)
#define STATUS_LED_PIN 2

// DCC Input Sensing (Primary)
// CAUTION: GPIO 0 is a strapping pin and signal appears dead in burst tests.
// GPIO 1 (TRACK_LEFT) shows active signal and is safe from boot strapping issues.
#define DCC_PIN 1

// Motor Driver (DRV8213)
#define MOTOR_IN1 41
#define MOTOR_IN2 40
#define MOTOR_GAIN_SEL 34

// Lighting / AUX
#define PIN_LIGHT_FRONT 12
#define PIN_LIGHT_REAR 10
#define PIN_AUX1 8
#define PIN_AUX2 9

// Audio (I2S MAX98357A)
#define AUDIO_BCLK 38
#define AUDIO_DIN 37
#define AUDIO_LRCLK 36
#define AUDIO_SD_MODE 33

// Sensors
#define PIN_MOTOR_CURRENT 4
#define PIN_V_SENSE_3V3 5

// --- System Settings ---
#define HOSTNAME "NIMRS-Decoder"
#define MAN_ID_DIY 13

#endif
