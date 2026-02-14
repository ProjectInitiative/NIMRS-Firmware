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

  // PWM Configuration (LEDC)
  uint8_t _pwmResolution;
  uint32_t _maxPwm;
  uint32_t _pwmFreq;
  const uint8_t _pwmChannel1 = 2; 
  const uint8_t _pwmChannel2 = 3;

  // Momentum & Direction State
  float _currentSpeed = 0.0f;
  bool _currentDirection = true;
  unsigned long _lastMomentumUpdate = 0;

  // CV Cache
  uint8_t _cvPwmFreq = 0;
  uint8_t _cvPwmFreqH = 0;
  uint8_t _cvAccel = 0;
  uint8_t _cvDecel = 0;
  uint8_t _cvVstart = 0;
  uint8_t _cvVhigh = 255;
  uint8_t _cvVmid = 128;
  uint8_t _cvConfig = 0; // CV 29
  uint8_t _speedTable[28]; // CV 67-94
  
  // Grade Compensation CVs
  uint8_t _cvLoadGain = 0; // Mapped from PID_P
  uint8_t _cvBaselineAlpha = 5;
  uint8_t _cvStictionKick = 50;
  uint8_t _cvDeltaCap = 180;
  uint8_t _cvPwmDither = 0;
  uint8_t _cvBaselineReset = 0;
  uint8_t _cvCurveIntensity = 0;
  
  unsigned long _lastCvUpdate = 0;

  // Grade Compensation State
  float _avgCurrent = 0.0f;       // Smoothed instantaneous current
  static const int SPEED_STEPS = 129;
  float _baselineTable[SPEED_STEPS] = {0}; // Speed-indexed baseline table
  
  // Helpers
  void _drive(uint16_t speed, bool direction);
  void _generateScurve(uint8_t intensity);
  void _updateCvCache();

  // Debug & Safety
  void streamTelemetry();
  int16_t _lastPwmValue = 0;
  unsigned long _stallStartTime = 0;
  const float _stallThreshold = 2.0f; // Amps (Tune for Paragon 4)
};

#endif
