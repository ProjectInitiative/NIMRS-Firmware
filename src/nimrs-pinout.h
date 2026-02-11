#ifndef NIMRS_PINOUT_H
#define NIMRS_PINOUT_H

#include <stdint.h>

namespace Pinout {
    // DCC / Track Input Sensing
    static constexpr uint8_t TRACK_RIGHT_3V3 = 0;   // GPIO0 (Strapping)
    static constexpr uint8_t TRACK_LEFT_3V3  = 1;   // GPIO1 (DCC_PIN)

    // Motor Control (DRV8213)
    static constexpr uint8_t MOTOR_IN1      = 41;   // GPIO 41 (MTDI)
    static constexpr uint8_t MOTOR_IN2      = 40;   // GPIO 40 (MTDO)
    static constexpr uint8_t MOTOR_GAIN_SEL = 34;   
    static constexpr uint8_t VMOTOR_PG      = 39;   // GPIO 49 (MTCK)

    // Lighting and AUX Outputs (User Verified)
    static constexpr uint8_t LIGHT_FRONT = 13;
    static constexpr uint8_t LIGHT_REAR  = 11;
    
    static constexpr uint8_t AUX1  = 9;
    static constexpr uint8_t AUX2  = 10;
    static constexpr uint8_t AUX3  = 35; // Note: GPIO35 is Input Only on S3?
    static constexpr uint8_t AUX4  = 14;
    static constexpr uint8_t AUX5  = 17;
    static constexpr uint8_t AUX6  = 12;

    // Logic Inputs (AUX 7/8 on Pin 1/2 of Header)
    static constexpr uint8_t INPUT1_AUX7 = 7;
    static constexpr uint8_t INPUT2_AUX8 = 8;

    // Sensors (ADC)
    static constexpr uint8_t MOTOR_CURRENT = 5; 
    static constexpr uint8_t V_SENSE_3V3   = 6; 

    // I2S Audio (MAX98357A)
    static constexpr uint8_t AMP_BCLK    = 38;
    static constexpr uint8_t AMP_DIN     = 37;
    static constexpr uint8_t AMP_LRCLK   = 36;
    static constexpr uint8_t AMP_SD_MODE = 33;
}

#endif // NIMRS_PINOUT_H
