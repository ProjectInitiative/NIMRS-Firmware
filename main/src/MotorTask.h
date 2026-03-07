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

  void start();
  void setTargetSpeed(uint8_t speedStep, bool forward);
  void reloadCvs();

  struct Status {
    float appliedVoltage;
    float current;
    float estimatedRpm;
    float rippleFreq;
    bool stalled;
    bool hardwareFault;
    bool isMoving;
    float duty;
    uint32_t rawAdc;
  };
  Status getStatus() const;

  enum class ResistanceState { IDLE, MEASURING, DONE, ERROR };
  void measureResistance();
  void resetModel();
  ResistanceState getResistanceState() const;
  float getMeasuredResistance() const;
  float getLearnedResistance() const;

  void startTest();
  String getTestJSON() const;

private:
  MotorTask();

  static void _taskEntry(void *param);
  void _loop();

  TaskHandle_t _taskHandle;

  BemfEstimator _estimator;
  RippleDetector _rippleDetector;
  EmaFilter _currentFilter;
  EmaFilter _peakFilter;

  uint8_t _targetSpeedStep;
  bool _targetDirection;
  float _currentDuty;

  float _piErrorSum;
  float _prevCurrent;
  float _filteredDiDt;

  // --- Adaptive Stall Detector State ---
  enum class AdaptiveMotorState {
    STOPPED,
    STARTUP,
    BASELINING,
    RUNNING
  } _adaptiveState;

  uint32_t _stateStartTime;
  float _baselineCurrent;
  uint16_t _baselineSampleCount;
  float _baselineSum;
  // --------------------------------------

  float _kp;
  float _ki;
  float _trackVoltage;
  float _maxRpm;
  float _vStart;
  uint8_t _cvPwmDither;
  uint8_t _cvStictionKick;

  bool _vKickActive;
  unsigned long _vKickStartTime;

  Status _status;

  ResistanceState _resistanceState;
  unsigned long _resistanceStartTime;
  float _measuredResistance;

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
