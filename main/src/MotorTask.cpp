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
    : _taskHandle(NULL), _currentFilter(0.1f), _peakFilter(0.2f),
      _targetSpeedStep(0), _targetDirection(true), _currentDuty(0.0f),
      _piErrorSum(0.0f), _kp(0.1f), _ki(0.01f), _trackVoltage(14.0f),
      _maxRpm(3000.0f), _vStart(0.0f), _cvPwmDither(0),
      _resistanceState(ResistanceState::IDLE), _resistanceStartTime(0),
      _measuredResistance(0.0f), _testMode(false), _testStartTime(0),
      _testDataIdx(0) {}

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
  float lastVControl = 0.0f;

  while (true) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    float scalar = MotorHal::getInstance().getCurrentScalar();
    size_t samples = MotorHal::getInstance().getAdcSamples(adcBuffer, 1024);
    float sampleRate = MotorHal::getInstance().getAdcSampleRate();

    float avgCurrent = 0.0f;
    float rippleFreq = 0.0f;
    uint32_t rawMaxAdc = 0;

    if (samples > 0) {
      float sumCurrent = 0.0f;
      float maxSample = 0.0f;
      for (size_t i = 0; i < samples; i++) {
        sumCurrent += adcBuffer[i];
        if (adcBuffer[i] > maxSample)
          maxSample = adcBuffer[i];
      }
      float instantAvg = sumCurrent / samples;
      rawMaxAdc = (uint32_t)maxSample;

      static float adcOffset = 0.0f;
      if (fabs(_currentDuty) < 0.01f) {
        adcOffset = (adcOffset * 0.9f) + (instantAvg * 0.1f);
      }

      float calibratedAvg = std::max(0.0f, instantAvg - adcOffset);
      avgCurrent = _currentFilter.update(calibratedAvg * scalar);
      _peakFilter.update(std::max(0.0f, maxSample - adcOffset) * scalar);

      for (size_t i = 0; i < samples; i++)
        adcBuffer[i] *= scalar;
      _rippleDetector.processBuffer(adcBuffer, samples, sampleRate);
      rippleFreq = _rippleDetector.getFrequency();
    } else {
      avgCurrent = _currentFilter.getValue();
    }

    if (_resistanceState == ResistanceState::MEASURING) {
      unsigned long elapsed = millis() - _resistanceStartTime;
      if (elapsed < 1000) {
        _currentDuty = 0.3f;
      } else {
        _currentDuty = 0.0f;
        float vApplied = _trackVoltage * 0.3f;
        if (avgCurrent > 0.01f) {
          _measuredResistance = vApplied / avgCurrent;
          _resistanceState = ResistanceState::DONE;
        } else {
          _resistanceState = ResistanceState::ERROR;
        }
      }
      MotorHal::getInstance().setDuty(_currentDuty);
      _status.appliedVoltage = _trackVoltage * fabs(_currentDuty);
      _status.current = avgCurrent;
      _status.duty = _currentDuty;
      _status.rawAdc = rawMaxAdc;
      continue;
    } else if (_resistanceState == ResistanceState::DONE ||
               _resistanceState == ResistanceState::ERROR) {
      _currentDuty = 0.0f;
      MotorHal::getInstance().setDuty(0.0f);
      if (millis() - _resistanceStartTime > 6000)
        _resistanceState = ResistanceState::IDLE;
      continue;
    }

    float vAppliedNow = _trackVoltage * fabs(_currentDuty);
    _estimator.updateLowSpeedData(vAppliedNow, avgCurrent);
    _estimator.updateRippleFreq(rippleFreq);
    _estimator.calculateEstimate();
    float actualRpm = _estimator.getEstimatedRpm();

    if (_targetSpeedStep == 0) {
      _currentDuty = 0.0f;
      _piErrorSum = 0.0f;
      lastVControl = 0.0f;
    } else {
      float targetRpm = (_targetSpeedStep / 255.0f) * _maxRpm;
      float error = targetRpm - actualRpm;

      // Throttled and damped integral
      _piErrorSum = (_piErrorSum * 0.98f) + error;
      _piErrorSum = constrain(_piErrorSum, -1000.0f, 1000.0f);

      float vPi = (_kp * error) + (_ki * _piErrorSum);
      float vTarget = vPi + _vStart;

      // SLEW RATE LIMIT: Max 2V change per 20ms to prevent slamming
      float maxChange = 2.0f;
      float vControl = constrain(vTarget, lastVControl - maxChange,
                                 lastVControl + maxChange);
      vControl = constrain(vControl, 0.0f, _trackVoltage);
      lastVControl = vControl;

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

    _status.appliedVoltage = _trackVoltage * fabs(_currentDuty);
    _status.current = avgCurrent;
    _status.estimatedRpm = actualRpm;
    _status.rippleFreq = rippleFreq;
    _status.stalled = _estimator.isStalled();
    _status.hardwareFault = MotorHal::getInstance().readFault();
    _status.duty = _currentDuty;
    _status.rawAdc = rawMaxAdc;
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
  _estimator.setMotorParams(
      cvRa * 0.1f, dcc.getCV(CV::MOTOR_POLES) ? dcc.getCV(CV::MOTOR_POLES) : 5);
  uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE);
  _trackVoltage = cvTv > 50 ? cvTv * 0.1f : 14.0f;
  uint16_t cvVs = dcc.getCV(CV::V_START);
  _vStart = (cvVs / 255.0f) * _trackVoltage;
  uint16_t cvKe = dcc.getCV(CV::MOTOR_KE);
  _estimator.setBemfConstant(cvKe > 0 ? cvKe * 0.001f : 0.015f);

  _kp = dcc.getCV(CV::MOTOR_KP) * 0.001f;
  _ki = dcc.getCV(CV::MOTOR_KI) * 0.0001f;
  _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
  _maxRpm = 3000.0f;
  MotorHal::getInstance().setHardwareGain(
      (uint8_t)dcc.getCV(CV::HARDWARE_GAIN));
  if (dcc.getCV(CV::BASELINE_RESET) == 1) {
    resetModel();
    dcc.setCV(CV::BASELINE_RESET, 0);
  }
}

MotorTask::Status MotorTask::getStatus() const { return _status; }
void MotorTask::measureResistance() {
  if (_resistanceState == ResistanceState::IDLE) {
    _resistanceState = ResistanceState::MEASURING;
    _resistanceStartTime = millis();
    _measuredResistance = 0.0f;
    _currentFilter.reset();
    _peakFilter.reset();
  }
}
void MotorTask::resetModel() { _estimator.reset(); }
MotorTask::ResistanceState MotorTask::getResistanceState() const {
  return _resistanceState;
}
float MotorTask::getMeasuredResistance() const { return _measuredResistance; }
float MotorTask::getLearnedResistance() const {
  return _estimator.getMeasuredResistance();
}
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
