#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "SystemContext.h"
#include "DccController.h"

class MotorController {
public:
    MotorController();
    void setup();
    void loop();

    // Configuration
    void setGain(bool high);

private:
    // Pin assignments from CSV
    const uint8_t _in1Pin = 41;
    const uint8_t _in2Pin = 40;
    const uint8_t _gainPin = 34;
    
    // PWM Configuration
    const uint32_t _pwmFreq = 20000; // 20kHz (Silent)
    const uint8_t _pwmResolution = 8;
    const uint8_t _pwmChannel1 = 0;
    const uint8_t _pwmChannel2 = 1;

    // Momentum State
    float _currentSpeed = 0.0f; // Float for smooth integration
    unsigned long _lastMomentumUpdate = 0;

    void _drive(uint8_t speed, bool direction);
};

#endif