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
  uint8_t _pwmResolution = 12;
  uint32_t _maxPwm = 4095;
  uint32_t _pwmFreq = 16000;

  // State
  float _currentSpeed = 0.0f;
  bool _currentDirection = true;
  unsigned long _lastMomentumUpdate = 0;
  uint16_t _lastPwmValue = 0;
  uint16_t _lastSmoothingPwm = 0;

  // Current Sensing (GPIO 4)
  uint8_t _activeGainMode; // 0=LO, 1=HI-Z (MED), 2=HI
  float _adcHistory[16];
  uint8_t _adcIdx = 0;
  float _currentOffset = 0.0f;
  float _avgCurrent = 0.0f;
  float _fastCurrent = 0.0f;

  // Load Compensation
  float _torqueIntegrator = 0.0f;
  float _slewedPwm = 0.0f;
  static const int SPEED_STEPS = 256; // Increased to match 0-255 range
  float _baselineTable[SPEED_STEPS];
  unsigned long _lastBaselineUpdate = 0;

  // Protection
  unsigned long _stallStartTime = 0;
  float _stallThreshold = 0.7f;
  bool _stallKickActive = false;
  unsigned long _stallKickTimer = 0;

  // CV Cache
  uint8_t _cvAccel, _cvDecel, _cvVstart, _cvVmid, _cvVhigh, _cvConfig;
  uint8_t _cvPedestalFloor, _cvLoadGain, _cvLoadGainScalar, _cvDeltaCap;
  uint8_t _cvStictionKick, _cvBaselineAlpha, _cvBaselineReset, _cvPwmDither;
  uint8_t _cvLearnThreshold, _cvCurveIntensity, _cvHardwareGain, _cvDriveMode;
  unsigned long _lastCvUpdate = 0;
  uint8_t _speedTable[28];

  void _drive(uint16_t speed, bool direction);
  void _updateCvCache();
  void _generateScurve(uint8_t intensity);
  void _saveBaselineTable();
  void _loadBaselineTable();
  uint32_t _getPeakADC();
};
#endif