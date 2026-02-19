#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <Preferences.h>

class MotorController {
public:
  MotorController();
  void setup();
  void loop();
  void streamTelemetry();

private:
  // PWM Config
  const uint8_t _pwmChannel1 = 2;
  const uint8_t _pwmChannel2 = 3;
  uint8_t _pwmResolution = 10;
  uint32_t _maxPwm = 1023;
  uint32_t _pwmFreq = 20000;

  // CV Mappings (Defaults from Paragon 4 Spec)
  uint8_t _cvVStart = 10;      // CV2: Starting PWM offset
  uint8_t _cvKickStart = 45;   // CV65: Instant burst on start
  uint8_t _cvKp = 25;          // CV112/113: Proportional Gain
  uint8_t _cvKi = 15;          // CV114/115: Integral Gain
  uint8_t _cvKpSlow = 180;     // CV118: Slow Speed Gain multiplier
  uint8_t _cvLoadFilter = 120; // CV189: Smoothing factor (0-255)
  uint8_t _cvAccel = 2;        // CV3

  // Internal State
  float _piErrorSum = 0.0f;
  unsigned long _kickStartTimer = 0;
  bool _isMoving = false;
  
  float _currentSpeed = 0.0f;
  unsigned long _lastMomentumUpdate = 0;
  uint16_t _lastPwmValue = 0; // For telemetry

  // Current Sensing
  float _avgCurrent = 0.0f;
  float _currentOffset = 0.0f; // Kept for offset calibration

  // CV Cache
  unsigned long _lastCvUpdate = 0;

  void _drive(uint16_t speed, bool direction);
  void _updateCvCache();
  uint32_t _getPeakADC();
};
#endif
