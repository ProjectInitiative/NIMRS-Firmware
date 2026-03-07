#include "MotorTask.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "Logger.h"
#include "MotorHal.h"
#include <Arduino.h>
#include <cmath>

MotorTask &MotorTask::getInstance() {
  static uint8_t motor_task_instance_buf[sizeof(MotorTask)];
  static MotorTask *instance = new (motor_task_instance_buf) MotorTask();
  return *instance;
}

MotorTask::MotorTask()
    : _taskHandle(NULL), _currentFilter(0.1f), // Alpha for I_avg
      _targetSpeedStep(0), _targetDirection(true), _currentDuty(0.0f),
      _piErrorSum(0.0f), _kp(0.1f), _ki(0.01f), _trackVoltage(14.0f),
      _maxRpm(6000.0f), // Can be tunable
      _cvPwmDither(0), _resistanceState(ResistanceState::IDLE),
      _resistanceStartTime(0), _measuredResistance(0.0f), _testMode(false),
      _testStartTime(0), _testDataIdx(0) {}

void MotorTask::start() {
  MotorHal::getInstance().init();
  reloadCvs();
  xTaskCreatePinnedToCore(_taskEntry, "MotorTask", 4096, this, 10, &_taskHandle,
                          1);
}

void MotorTask::_taskEntry(void *param) {
  MotorTask *self = (MotorTask *)param;
  self->_loop();
}

void MotorTask::_loop() {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz
  static float adcBuffer[1024];

  while (true) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 1. Read Sensors
    size_t samples = MotorHal::getInstance().getAdcSamples(adcBuffer, 1024);
    float sampleRate = MotorHal::getInstance().getAdcSampleRate();

    float avgCurrent = 0.0f;
    float rippleFreq = 0.0f;
    uint32_t rawAdcValue = 0;

    if (samples > 0) {
      float sumCurrent = 0.0f;
      for (size_t i = 0; i < samples; i++)
        sumCurrent += adcBuffer[i];
      float instantAvg = sumCurrent / samples;
      rawAdcValue = (uint32_t)instantAvg;

      static float adcOffset = 0.0f;
      if (fabs(_currentDuty) < 0.01f) {
        adcOffset = (adcOffset * 0.9f) + (instantAvg * 0.1f);
      }
      float calibratedAvg = std::max(0.0f, instantAvg - adcOffset);
      float currentAmps = calibratedAvg * 0.0016f; // Scaling for 11dB (3.3V)
      avgCurrent = _currentFilter.update(currentAmps);

      for (size_t i = 0; i < samples; i++)
        adcBuffer[i] *= 0.0016f;
      _rippleDetector.processBuffer(adcBuffer, samples, sampleRate);
      rippleFreq = _rippleDetector.getFrequency();
    } else {
      avgCurrent = _currentFilter.getValue();
    }

    // --- SENSING & MEASUREMENT OVERRIDES ---
    if (_resistanceState == ResistanceState::MEASURING) {
      unsigned long elapsed = millis() - _resistanceStartTime;
      if (elapsed < 1000) {
        _currentDuty = 0.3f;
      } else {
        _currentDuty = 0.0f;
        float vApplied = _trackVoltage * 0.3f;
        if (avgCurrent > 0.01f) {
          _measuredResistance = vApplied / avgCurrent;
          Log.printf("Motor: Measured R = %.2f Ohm (I=%.3fA)\n",
                     _measuredResistance, avgCurrent);
          _resistanceState = ResistanceState::DONE;
        } else {
          _resistanceState = ResistanceState::ERROR;
        }
      }
      MotorHal::getInstance().setDuty(_currentDuty);
      _status.current = avgCurrent;
      _status.duty = _currentDuty;
      _status.rawAdc = rawAdcValue;
      continue;
    } else if (_resistanceState == ResistanceState::DONE ||
               _resistanceState == ResistanceState::ERROR) {
      _currentDuty = 0.0f;
      MotorHal::getInstance().setDuty(0.0f);
      if (millis() - _resistanceStartTime > 6000)
        _resistanceState = ResistanceState::IDLE;
      continue;
    }

    // --- NORMAL OPERATION ---
    float vApplied = _trackVoltage * fabs(_currentDuty);
    _estimator.updateLowSpeedData(vApplied, avgCurrent);
    _estimator.updateRippleFreq(rippleFreq);
    _estimator.calculateEstimate();
    float actualRpm = _estimator.getEstimatedRpm();

    // PI Control
    if (_targetSpeedStep == 0) {
      _currentDuty = 0.0f;
      _piErrorSum = 0.0f;
    } else {
      float targetRpm = (_targetSpeedStep / 255.0f) * _maxRpm;
      float error = targetRpm - actualRpm;
      _piErrorSum = constrain(_piErrorSum + error, -2000.0f, 2000.0f);
      float vControl =
          constrain((_kp * error) + (_ki * _piErrorSum), 0.0f, _trackVoltage);
      float duty = vControl / _trackVoltage;

      if (_targetSpeedStep > 0 && _targetSpeedStep < 15 && _cvPwmDither > 0) {
        unsigned long phase = (xTaskGetTickCount() * portTICK_PERIOD_MS) % 40;
        float baseAmplitude = (_cvPwmDither / 255.0f) * 0.39f;
        float fadeFactor = 1.0f - (_targetSpeedStep / 15.0f);
        float dither = baseAmplitude * fadeFactor;
        if (phase < 20)
          duty += dither;
        else
          duty -= dither;
      }
      if (!_targetDirection)
        duty = -duty;
      _currentDuty = duty;
    }

    MotorHal::getInstance().setDuty(_currentDuty);

    _status.appliedVoltage = vApplied;
    _status.current = avgCurrent;
    _status.estimatedRpm = actualRpm;
    _status.rippleFreq = rippleFreq;
    _status.stalled = _estimator.isStalled();
    _status.duty = _currentDuty;
    _status.rawAdc = rawAdcValue;
  }
}

void MotorTask::setTargetSpeed(uint8_t speedStep, bool forward) {
  if (_testMode || _resistanceState != ResistanceState::IDLE)
    return;
  _targetSpeedStep = speedStep;
  _targetDirection = forward;
}

void MotorTask::reloadCvs() {
  NmraDcc &dcc = DccController::getInstance().getDcc();
  uint16_t cvRa = dcc.getCV(CV::MOTOR_R_ARM);
  _estimator.setMotorParams(cvRa * 0.01f, dcc.getCV(CV::MOTOR_POLES)
                                              ? dcc.getCV(CV::MOTOR_POLES)
                                              : 5);
  uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE);
  _trackVoltage = cvTv > 50 ? cvTv * 0.1f : 14.0f;
  _kp = dcc.getCV(CV::MOTOR_KP) * 0.01f;
  _ki = dcc.getCV(CV::MOTOR_KI) * 0.001f;
  _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
  _maxRpm = 6000.0f;
  MotorHal::getInstance().setHardwareGain(
      (uint8_t)dcc.getCV(CV::HARDWARE_GAIN));
}

MotorTask::Status MotorTask::getStatus() const { return _status; }
void MotorTask::measureResistance() {
  if (_resistanceState == ResistanceState::IDLE) {
    _resistanceState = ResistanceState::MEASURING;
    _resistanceStartTime = millis();
    _measuredResistance = 0.0f;
    _currentFilter.reset();
  }
}
MotorTask::ResistanceState MotorTask::getResistanceState() const {
  return _resistanceState;
}
float MotorTask::getMeasuredResistance() const { return _measuredResistance; }
void MotorTask::startTest() {
  if (_testMode)
    return;
  _testMode = true;
  _testStartTime = millis();
  _testDataIdx = 0;
}
String MotorTask::getTestJSON() const {
  String out = "[";
  for (int i = 0; i < _testDataIdx; i++) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"t\":%lu,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             (unsigned long)_testData[i].t, _testData[i].target,
             (uint16_t)(fabs(_testData[i].duty) * 1023.0f),
             _testData[i].current, _testData[i].rpm);
    out += buf;
    if (i < _testDataIdx - 1)
      out += ",";
  }
  out += "]";
  return out;
}
