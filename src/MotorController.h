#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>
#include "SystemContext.h"
#include "DccController.h"
#include "nimrs-pinout.h"

class MotorController {
public:
    MotorController();
    void setup();
    void loop();

    // Configuration
    void setGain(bool high);

private:
    // PWM Configuration
    const uint32_t _pwmFreq = 20000; // 20kHz (Silent)
    const uint8_t _pwmResolution = 8;
    const uint8_t _pwmChannel1 = 0;
    const uint8_t _pwmChannel2 = 1;

    // Momentum & Direction State
    float _currentSpeed = 0.0f;
    bool _currentDirection = true; // Tracks physical bridge state
    unsigned long _lastMomentumUpdate = 0;

    void _drive(uint8_t speed, bool direction);
};

#endif
