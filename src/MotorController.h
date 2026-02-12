#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>

class MotorController {
public:
  MotorController();
  void setup();
  void loop();

  // Configuration
  void setGain(bool high);

private:
  const uint8_t _gainPin = 34;

  // PWM Configuration
  // Low frequency (100Hz) improves low-speed torque.
  // DRV8213 handles current regulation to prevent buzzing issues.
  const uint32_t _pwmFreq = 100;
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
