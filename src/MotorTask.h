#ifndef MOTOR_TASK_H
#define MOTOR_TASK_H

#include "BemfEstimator.h"
#include "DspFilters.h"
#include "MotorHal.h"
#include "RippleDetector.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class MotorTask {
public:
  static MotorTask &getInstance();

  // Start the FreeRTOS task on Core 1
  void start();

  // Set Target Speed (0-255) and Direction
  void setTargetSpeed(uint8_t speedStep, bool forward);

  // Update CVs from Registry
  void reloadCvs();

  // Telemetry
  struct Status {
    float appliedVoltage;
    float current;
    float estimatedRpm;
    float rippleFreq;
    bool stalled;
    float duty;
  };
  Status getStatus() const;

  // Resistance Measurement
  enum class ResistanceState { IDLE, MEASURING, DONE, ERROR };
  void measureResistance();
  ResistanceState getResistanceState() const;
  float getMeasuredResistance() const;

  // Test Mode
  void startTest();
  String getTestJSON() const;

private:
  MotorTask();

  static void _taskEntry(void *param);
  void _loop();

  TaskHandle_t _taskHandle;

  // Components
  BemfEstimator _estimator;
  RippleDetector _rippleDetector;
  EmaFilter _currentFilter; // For low speed average (I_avg)

  // Control State
  uint8_t _targetSpeedStep;
  bool _targetDirection;
  float _currentDuty; // Current PWM output

  // PI State
  float _piErrorSum;

  // CV Cache
  float _kp;
  float _ki;
  float _trackVoltage;
  float _maxRpm; // Calculated from CVs? Or hardcoded limit?
  uint8_t _cvPwmDither;

  // Telemetry
  Status _status;

  // Resistance Measurement State
  ResistanceState _resistanceState;
  unsigned long _resistanceStartTime;
  float _measuredResistance;

  // Test Mode State
  bool _testMode;
  unsigned long _testStartTime;
  static const int MAX_TEST_POINTS = 100;
  struct TestPoint {
    uint32_t t;
    uint8_t target;
    float duty;
    float current;
    float rpm;
  } _testData[MAX_TEST_POINTS];
  int _testDataIdx;
};

#endif
