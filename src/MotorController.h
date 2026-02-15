#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <Preferences.h> // Required for NVS baseline storage

class MotorController {
public:
  MotorController();
  void setup();
  void loop();

  // Telemetry and Monitoring
  void streamTelemetry();

private:
  // PWM & Hardware State
  uint8_t _pwmResolution = 12;    // 12-bit for fine-grained 5-pole control
  uint32_t _maxPwm = 4095;        // (1 << 12) - 1
  uint32_t _pwmFreq = 16000;      // 16kHz to minimize motor hum
  const uint8_t _pwmChannel1 = 0; // Symmetric PWM Channels
  const uint8_t _pwmChannel2 = 1;

  // Motor State
  float _currentSpeed = 0.0f;
  bool _currentDirection = true;
  unsigned long _lastMomentumUpdate = 0;
  uint16_t _lastPwmValue = 0;
  uint16_t _lastSmoothingPwm = 0;

  // ADC Filtering & Current Sensing
  float _adcHistory[16]; // Moving average buffer
  uint8_t _adcIdx = 0;
  float _currentOffset = 0.0f; // Calibrated at boot
  float _avgCurrent = 0.0f;    // Smoothed Amps

  // Load Compensation (PI Regulator)
  float _torqueIntegrator = 0.0f;
  static const int SPEED_STEPS = 129;
  float _baselineTable[SPEED_STEPS]; // Current draw at each speed step
  unsigned long _lastBaselineUpdate = 0;

  // Safety & Protection
  unsigned long _stallStartTime = 0;
  float _stallThreshold = 0.6f; // Amps
  bool _startupKickActive = false;
  unsigned long _startupKickTime = 0;

  // CV Cache (Dynamic Performance Tuning)
  uint8_t _cvAccel, _cvDecel;
  uint8_t _cvVstart, _cvVmid, _cvVhigh;
  uint8_t _cvConfig, _cvPedestalFloor;
  uint8_t _cvLoadGain, _cvLoadGainScalar;
  uint8_t _cvDeltaCap, _cvStictionKick;
  uint8_t _cvBaselineAlpha, _cvBaselineReset;
  uint8_t _cvPwmDither, _cvLearnThreshold;
  uint8_t _cvCurveIntensity, _cvHardwareGain, _cvDriveMode;
  unsigned long _lastCvUpdate = 0;

  // Speed Tables
  uint8_t _speedTable[28]; // Standard 28-step DCC table

  // Internal Helpers
  void _drive(uint16_t speed, bool direction);
  void _updateCvCache();
  void _generateScurve(uint8_t intensity);
  void _saveBaselineTable();
  void _loadBaselineTable();
};

#endif