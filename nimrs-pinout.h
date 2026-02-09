#ifndef NIMRS_PINOUT_H
#define NIMRS_PINOUT_H

#include <stdint.h>

namespace Pinout {
    // DCC / Track Input Sensing
    static constexpr uint8_t TRACK_RIGHT = 0;   // GPIO0
    static constexpr uint8_t TRACK_LEFT  = 1;   // GPIO1

    // Voltage and Current Sensing (ADC)
    static constexpr uint8_t MOTOR_CURRENT = 4; // GPIO4
    static constexpr uint8_t V_SENSE_3V3   = 5; // GPIO5 (System Voltage)

    // Motor Control (DRV8213)
    static constexpr uint8_t MOTOR_IN1      = 41; // GPIO41
    static constexpr uint8_t MOTOR_IN2      = 40; // GPIO40
    static constexpr uint8_t MOTOR_GAIN_SEL = 34; // GPIO34
    static constexpr uint8_t VMOTOR_PG      = 39; // GPIO39 (Power Good)

    // I2S Audio (MAX98357A)
    static constexpr uint8_t AMP_BCLK    = 38; // GPIO38
    static constexpr uint8_t AMP_DIN     = 37; // GPIO37
    static constexpr uint8_t AMP_LRCLK   = 36; // GPIO36
    static constexpr uint8_t AMP_SD_MODE = 33; // GPIO33 (Shutdown/Mode)
    static constexpr uint8_t AMP_GAIN    = 21; // GPIO21

    // Lighting and AUX Outputs (Transistor Driven)
    static constexpr uint8_t LIGHT_FRONT = 12; // GPIO12
    static constexpr uint8_t LIGHT_REAR  = 10; // GPIO10
    static constexpr uint8_t AUX1        = 8;  // GPIO8
    static constexpr uint8_t AUX2        = 9;  // GPIO9

    // 21-Pin Direct Logic/Input Pins
    static constexpr uint8_t INPUT1_AUX7 = 12; // Mapped to GPIO7 in schematic
    static constexpr uint8_t INPUT2_AUX8 = 11; // Mapped to GPIO6 in schematic
    static constexpr uint8_t AUX3        = 20; // GPIO17 (Note: Schematic also shows Pin 40)
    static constexpr uint8_t AUX4        = 18; // GPIO13
    static constexpr uint8_t AUX5        = 19; // GPIO14
    static constexpr uint8_t AUX6        = 15; // GPIO11

    // Communication / RailCom / USB
    static constexpr uint8_t ZBCLK_D_MINUS = 24; // GPIO18
    static constexpr uint8_t ZBDTA_D_PLUS  = 25; // GPIO19
    static constexpr uint8_t UART_TX       = 48; // U0TXD
    static constexpr uint8_t UART_RX       = 49; // U0RXD

    // System
    static constexpr uint8_t SYS_EN_PG = 52; // GPIO46 (3V3 Power Good / Enable)
}

#endif // NIMRS_PINOUT_H
