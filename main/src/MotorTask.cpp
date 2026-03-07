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
      _piErrorSum(0.0f), _prevCurrent(0.0f), _filteredDiDt(0.0f),
      _adaptiveState(AdaptiveMotorState::STOPPED), _stateStartTime(0),
      _baselineCurrent(0.0f), _baselineSampleCount(0), _baselineSum(0.0f),
      _kp(0.002f), _ki(0.0005f), _trackVoltage(14.0f), _maxRpm(3000.0f),
      _vStart(0.0f), _cvPwmDither(0), _cvStictionKick(0), _vKickActive(false),
      _vKickStartTime(0), _resistanceState(ResistanceState::IDLE),
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

    // --- ADAPTIVE DI/DT STALL DETECTOR ---
    float rawDiDt = avgCurrent - _prevCurrent;
    _filteredDiDt = (0.05f * rawDiDt) + (0.95f * _filteredDiDt);
    _prevCurrent = avgCurrent;

    bool lowSpeedStall = false;
    if (_targetSpeedStep == 0) {
      _adaptiveState = AdaptiveMotorState::STOPPED;
    } else {
      uint32_t nowMs = millis();
      switch (_adaptiveState) {
      case AdaptiveMotorState::STOPPED:
        _adaptiveState = AdaptiveMotorState::STARTUP;
        _stateStartTime = nowMs;
        break;
      case AdaptiveMotorState::STARTUP:
        if (nowMs - _stateStartTime > 500) {
          _adaptiveState = AdaptiveMotorState::BASELINING;
          _baselineSampleCount = 0;
          _baselineSum = 0.0f;
        }
        break;
      case AdaptiveMotorState::BASELINING:
        _baselineSum += avgCurrent;
        _baselineSampleCount++;
        if (_baselineSampleCount >= 50) {
          _baselineCurrent = _baselineSum / 50.0f;
          _adaptiveState = AdaptiveMotorState::RUNNING;
        }
        break;
      case AdaptiveMotorState::RUNNING: {
        float currentStep = std::min(128.0f, (float)_targetSpeedStep);
        float slope = (1.5f - 4.0f) / 128.0f;
        float dynamicMultiplier = 4.0f + (slope * currentStep);
        if ((avgCurrent > (_baselineCurrent * dynamicMultiplier)) &&
            (_filteredDiDt > 0.5f)) {
          lowSpeedStall = true;
        }
        break;
      }
      }
    }

    // --- MANUAL RESISTANCE MEASUREMENT ---
    if (_resistanceState == ResistanceState::MEASURING) {
      unsigned long elapsed = millis() - _resistanceStartTime;
      if (elapsed < 1000) {
        _currentDuty = 3.0f / _trackVoltage;
      } else {
        _currentDuty = 0.0f;
        if (avgCurrent > 0.01f) {
          _measuredResistance = 3.0f / avgCurrent;
          _estimator.setMotorParams(_measuredResistance, -1);
          uint16_t cvVal = (uint16_t)(_measuredResistance / 0.2f);
          if (cvVal > 255)
            cvVal = 255;
          DccController::getInstance().getDcc().setCV(CV::MOTOR_R_ARM,
                                                      (uint8_t)cvVal);
          _resistanceState = ResistanceState::DONE;
        } else {
          _resistanceState = ResistanceState::ERROR;
        }
      }
      MotorHal::getInstance().setDuty(_currentDuty);
      _status.current = avgCurrent;
      _status.duty = _currentDuty;
      _status.appliedVoltage = 3.0f;
      continue;
    } else if (_resistanceState == ResistanceState::DONE ||
               _resistanceState == ResistanceState::ERROR) {
      _currentDuty = 0.0f;
      MotorHal::getInstance().setDuty(0.0f);
      if (millis() - _resistanceStartTime > 5000)
        _resistanceState = ResistanceState::IDLE;
      continue;
    }

    // --- THREE-ZONE MOTOR CONTROL ---
    float vAppliedNow = _trackVoltage * fabs(_currentDuty);
    _estimator.updateLowSpeedData(vAppliedNow, avgCurrent);
    _estimator.updateRippleFreq(rippleFreq);
    _estimator.calculateEstimate();
    float actualRpm = _estimator.getEstimatedRpm();
    bool rippleConfirm = (rippleFreq > 10.0f);

    float vTarget = 0.0f;

    if (_targetSpeedStep == 0) {
      _currentDuty = 0.0f;

      // PERSIST LEARNED Ke ON STOP
      uint16_t learnedCvVal =
          (uint16_t)(_estimator.getBemfConstant() * 1000.0f);
      NmraDcc &dcc = DccController::getInstance().getDcc();
      uint16_t currentCvVal = dcc.getCV(CV::MOTOR_KE);
      if (learnedCvVal != currentCvVal && learnedCvVal > 0 &&
          learnedCvVal < 255) {
        dcc.setCV(CV::MOTOR_KE, (uint8_t)learnedCvVal);
      }

      _piErrorSum = 0.0f;
      lastVControl = 0.0f;
      _vKickActive = false;
    } else {
      // 1. Kick Start
      float kickBonus = 0.0f;
      if (!_vKickActive && vAppliedNow < 0.1f) {
        _vKickActive = true;
        _vKickStartTime = millis();
      }
      if (_vKickActive) {
        unsigned long elapsed = millis() - _vKickStartTime;
        if (elapsed < 100) {
          kickBonus = (_cvStictionKick / 255.0f) * 4.0f;
        } else {
          _vKickActive = false;
        }
      }

      float targetRpm = (_targetSpeedStep / 255.0f) * _maxRpm;

      // HANDOFF LOGIC
      if (!rippleConfirm && _targetSpeedStep < 20) {
        // ZONE 2: LOW SPEED (TORQUE CONTROL)
        float targetCurrent = (_targetSpeedStep / 255.0f) * 0.5f;
        vTarget = (targetCurrent * _estimator.getMeasuredResistance()) +
                  _vStart + kickBonus;

        // BUMPLESS PRE-LOAD: Match current voltage output so PI doesn't jerk on
        // transition
        if (_ki > 0.0f) {
          _piErrorSum = vTarget / _ki;
        }
      } else {
        // ZONE 3: HIGH SPEED (VELOCITY CONTROL)
        float error = targetRpm - actualRpm;
        if (fabs(error) > 5.0f) {
          _piErrorSum = constrain(_piErrorSum + error, -1000.0f, 1000.0f);
        }
        float vPi = (_kp * error) + (_ki * _piErrorSum);
        vTarget = vPi + _vStart + kickBonus;
      }

      float maxIncrease = 0.4f;
      float maxDecrease = 0.02f;
      float vControl = vTarget;
      if (vTarget > lastVControl + maxIncrease)
        vControl = lastVControl + maxIncrease;
      if (vTarget < lastVControl - maxDecrease)
        vControl = lastVControl - maxDecrease;

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
    _status.stalled = lowSpeedStall || _estimator.isStalled();
    _status.hardwareFault = MotorHal::getInstance().readFault();
    _status.isMoving = rippleConfirm;
    _status.duty = _currentDuty;
    _status.rawAdc = rawMaxAdc;

    // --- TELEMETRY LOGGING (10Hz) ---
    static unsigned long lastLogTime = 0;
    if (millis() - lastLogTime >= 100) {
      lastLogTime = millis();

      uint8_t currentZone = 1; // Default Static
      if (_targetSpeedStep == 0)
        currentZone = 0;
      else if (!rippleConfirm && _targetSpeedStep < 20)
        currentZone = 2; // Torque
      else
        currentZone = 3; // Velocity (PI)

      Log.printf("[NIMRS_DATA] "
                 "{\"tgt\":%d,\"cur\":%.3f,\"rpm\":%.1f,\"rip_ok\":%d,\"zone\":"
                 "%d,\"v\":%.2f,\"ke\":%.4f,\"stall\":%d}\n",
                 _targetSpeedStep, avgCurrent, actualRpm, rippleConfirm ? 1 : 0,
                 currentZone, _status.appliedVoltage,
                 _estimator.getBemfConstant(), _status.stalled ? 1 : 0);
    }
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
  if (cvRa == 0)
    cvRa = 175;
  _estimator.setMotorParams(
      cvRa * 0.2f, dcc.getCV(CV::MOTOR_POLES) ? dcc.getCV(CV::MOTOR_POLES) : 5);
  uint16_t cvKe = dcc.getCV(CV::MOTOR_KE);
  _estimator.setBemfConstant(cvKe > 0 ? cvKe * 0.001f : 0.015f);
  uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE);
  _trackVoltage = cvTv > 50 ? cvTv * 0.1f : 14.0f;
  uint16_t cvVs = dcc.getCV(CV::V_START);
  _vStart = (cvVs / 255.0f) * _trackVoltage;
  _cvStictionKick = dcc.getCV(CV::STICTION_KICK);
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
void MotorTask::resetModel() {
  _estimator.reset();
  DccController::getInstance().getDcc().setCV(CV::MOTOR_R_ARM, 175);
  DccController::getInstance().getDcc().setCV(CV::MOTOR_KE, 15);
}
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
