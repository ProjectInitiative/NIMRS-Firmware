#include "MotorTask.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "Logger.h"
#include "MotorHal.h"
#include <Arduino.h>
#include <cmath>

MotorTask &MotorTask::getInstance() {
  static MotorTask instance;
  return instance;
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
  // 1. Initialize Hardware
  MotorHal::getInstance().init();

  // 2. Load CVs
  reloadCvs();

  // 3. Create Task
  xTaskCreatePinnedToCore(_taskEntry, "MotorTask", 4096, this,
                          10, // Priority
                          &_taskHandle,
                          1 // Core 1
  );
}

void MotorTask::_taskEntry(void *param) {
  MotorTask *self = (MotorTask *)param;
  self->_loop();
}

void MotorTask::_loop() {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz

  // Buffer for ADC (20kHz * 20ms = 400 samples minimum)
  static float adcBuffer[1024];

  while (true) {
    // 1. Wait for next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    // 2. Read Sensors
    size_t samples = MotorHal::getInstance().getAdcSamples(adcBuffer, 1024);
    float sampleRate = MotorHal::getInstance().getAdcSampleRate();

    // 3. Process Ripple & Current
    float sumCurrent = 0.0f;
    for (size_t i = 0; i < samples; i++) {
      sumCurrent += adcBuffer[i];
    }
    float instantAvg = (samples > 0) ? (sumCurrent / samples) : 0.0f;

    // --- AUTO-ZERO CALIBRATION ---
    static float adcOffset = 0.0f;
    static int offsetSamples = 0;
    if (fabs(_currentDuty) < 0.01f && offsetSamples < 50) {
      adcOffset =
          (adcOffset * offsetSamples + instantAvg) / (offsetSamples + 1);
      offsetSamples++;
    }
    float calibratedAvg = std::max(0.0f, instantAvg - adcOffset);

    // Convert raw ADC to Amps
    float currentAmps = calibratedAvg * 0.00025f;

    // Update Filters
    float avgCurrent = _currentFilter.update(currentAmps);

    // --- SENSING & MEASUREMENT OVERRIDES ---
    if (_resistanceState == ResistanceState::MEASURING) {
      unsigned long elapsed = millis() - _resistanceStartTime;
      if (elapsed < 1000) {
        _currentDuty = 0.3f; // Apply 30% PWM
        MotorHal::getInstance().setDuty(_currentDuty);
        if (elapsed < 100) {
          _status.duty = _currentDuty;
          continue;
        }
      } else {
        _currentDuty = 0.0f;
        MotorHal::getInstance().setDuty(0.0f);
        float vApplied = _trackVoltage * 0.3f;
        if (avgCurrent > 0.01f) {
          _measuredResistance = vApplied / avgCurrent;
          Log.printf("Motor: Measured Resistance = %.2f Ohm (I=%.3fA)\n",
                     _measuredResistance, avgCurrent);
          _resistanceState = ResistanceState::DONE;
        } else {
          Log.printf("Motor: Low signal (I=%.4fA). Measurement Failed.\n",
                     avgCurrent);
          _resistanceState = ResistanceState::ERROR;
        }
      }
      _status.appliedVoltage = _trackVoltage * fabs(_currentDuty);
      _status.current = avgCurrent;
      _status.estimatedRpm = 0;
      _status.rippleFreq = 0;
      _status.stalled = false;
      _status.duty = _currentDuty;
      continue;
    } else if (_resistanceState == ResistanceState::DONE ||
               _resistanceState == ResistanceState::ERROR) {
      _currentDuty = 0.0f;
      MotorHal::getInstance().setDuty(0.0f);
      if (millis() - _resistanceStartTime > 6000)
        _resistanceState = ResistanceState::IDLE;
      _status.current = avgCurrent;
      _status.duty = 0.0f;
      continue;
    }

    // --- NORMAL OPERATION ---

    // Ripple Detector
    for (size_t i = 0; i < samples; i++) {
      adcBuffer[i] *= 0.00025f;
    }
    _rippleDetector.processBuffer(adcBuffer, samples, sampleRate);
    float rippleFreq = _rippleDetector.getFrequency();

    // Update Estimator
    float vApplied = _trackVoltage * fabs(_currentDuty);
    _estimator.updateLowSpeedData(vApplied, avgCurrent);
    _estimator.updateRippleFreq(rippleFreq);
    _estimator.calculateEstimate();

    float actualRpm = _estimator.getEstimatedRpm();

    // Test Mode Override
    if (_testMode) {
      unsigned long t = millis() - _testStartTime;
      if (t > 3000) {
        _testMode = false;
        _targetSpeedStep = 0;
      } else {
        if (t < 1000) {
          _targetSpeedStep = map(t, 0, 1000, 0, 100);
          _targetDirection = true;
        } else if (t < 1500) {
          _targetSpeedStep = 100;
          _targetDirection = true;
        } else if (t < 2500) {
          _targetSpeedStep = map(t, 1500, 2500, 0, 100);
          _targetDirection = false;
        } else {
          _targetSpeedStep = 100;
          _targetDirection = false;
        }

        static uint8_t decimate = 0;
        if (++decimate >= 2) {
          decimate = 0;
          if (_testDataIdx < MAX_TEST_POINTS) {
            _testData[_testDataIdx].t = (uint32_t)t;
            _testData[_testDataIdx].target = _targetSpeedStep;
            _testData[_testDataIdx].duty = _currentDuty;
            _testData[_testDataIdx].current = avgCurrent;
            _testData[_testDataIdx].rpm = actualRpm;
            _testDataIdx++;
          }
        }
      }
    }

    // Control Loop
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

      // DITHER
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

    // Telemetry Update
    _status.appliedVoltage = vApplied;
    _status.current = avgCurrent;
    _status.estimatedRpm = actualRpm;
    _status.rippleFreq = rippleFreq;
    _status.stalled = _estimator.isStalled();
    _status.duty = _currentDuty;
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
  float rArm = cvRa * 0.01f;

  uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE);
  _trackVoltage = cvTv * 0.1f;
  if (_trackVoltage < 5.0f)
    _trackVoltage = 14.0f;

  uint16_t poles = dcc.getCV(CV::MOTOR_POLES);
  if (poles == 0)
    poles = 5;

  _estimator.setMotorParams(rArm, poles);

  uint16_t kp = dcc.getCV(CV::MOTOR_KP);
  uint16_t ki = dcc.getCV(CV::MOTOR_KI);
  _kp = kp * 0.01f;
  _ki = ki * 0.001f;

  _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
}

MotorTask::Status MotorTask::getStatus() const { return _status; }

void MotorTask::measureResistance() {
  if (_resistanceState == ResistanceState::IDLE) {
    _resistanceState = ResistanceState::MEASURING;
    _resistanceStartTime = millis();
    _measuredResistance = 0.0f;
    _currentFilter.reset();
    Log.println("MotorTask: Starting Resistance Measurement...");
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
  Log.println("MotorTask: Starting Self-Test...");
}

String MotorTask::getTestJSON() const {
  String out = "[";
  for (int i = 0; i < _testDataIdx; i++) {
    char buf[128];
    uint16_t pwm = (uint16_t)(fabs(_testData[i].duty) * 1023.0f);
    snprintf(buf, sizeof(buf),
             "{\"t\":%lu,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             (unsigned long)_testData[i].t, _testData[i].target, pwm,
             _testData[i].current, _testData[i].rpm);
    out += buf;
    if (i < _testDataIdx - 1)
      out += ",";
  }
  out += "]";
  return out;
}
