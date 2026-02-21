#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <Preferences.h>

class MotorController {
public:
  static MotorController& getInstance() {
      static MotorController instance;
      return instance;
  }
  
  void setup();
  void loop();
  void streamTelemetry();
  void startTest();
  String getTestJSON();

  MotorController(const MotorController&) = delete;
  MotorController& operator=(const MotorController&) = delete;

private:
  MotorController();

  // PWM Configuration Constants
  static constexpr uint32_t DEFAULT_PWM_FREQ = 20000;
  static constexpr uint8_t DEFAULT_PWM_RES = 10;
  static constexpr uint32_t DEFAULT_MAX_PWM = (1 << DEFAULT_PWM_RES) - 1;

  // PWM Config
  const uint8_t _pwmChannel1 = 2;
  const uint8_t _pwmChannel2 = 3;
  uint8_t _pwmResolution = DEFAULT_PWM_RES;
  uint32_t _maxPwm = DEFAULT_MAX_PWM;
  uint32_t _pwmFreq = DEFAULT_PWM_FREQ;

  // CV Mappings (Defaults from Paragon 4 Spec)
  uint8_t _cvVStart = 20;      // CV2: Starting PWM offset
  uint8_t _cvKickStart = 40;   // CV65: Instant burst on start
  uint8_t _cvKp = 20;          // CV112/113: Proportional Gain
  uint8_t _cvKi = 10;          // CV114/115: Integral Gain
  uint8_t _cvKpSlow = 128;     // CV118: Slow Speed Gain multiplier
  uint8_t _cvLoadFilter = 150; // CV189: Smoothing factor (0-255)
  uint8_t _cvAccel = 4;        // CV3
  uint8_t _cvPwmDither = 0;    // CV64
  uint8_t _cvPedestalFloor = 160;   // CV57: VStart Floor
  uint8_t _cvLoadGainScalar = 20;   // CV146: Punch Scalar
  uint8_t _cvLearnThreshold = 60;   // CV147: Load Threshold Base (x0.01)

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

  // Test Mode
  bool _testMode = false;
  unsigned long _testStartTime = 0;
  static const int MAX_TEST_POINTS = 60; // 3 seconds @ 50ms
  struct TestPoint {
      uint32_t t;
      uint8_t target;
      uint16_t pwm;
      float current;
      float speed;
  } _testData[MAX_TEST_POINTS];
  int _testDataIdx = 0;
};
#endif
