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
    // Average current (Low speed)
    float sumCurrent = 0.0f;
    for (size_t i = 0; i < samples; i++) {
      sumCurrent += adcBuffer[i];
    }
    float instantAvg = (samples > 0) ? (sumCurrent / samples) : 0.0f;

    // Convert raw ADC to Amps (Need calibration factor)
    // Using existing factor from MotorController: 0.00054f
    float currentAmps = instantAvg * 0.00054f;

    // Update Filters
    float avgCurrent = _currentFilter.update(currentAmps);

    // Resistance Measurement Override
    if (_resistanceState == ResistanceState::MEASURING) {
      if (millis() - _resistanceStartTime < 1000) {
        // Apply fixed PWM (approx 20%)
        _currentDuty = 0.2f; // Fixed 20%
        MotorHal::getInstance().setDuty(_currentDuty);
      } else {
        // Measurement complete
        _currentDuty = 0.0f;
        MotorHal::getInstance().setDuty(0.0f);

        float vApplied = _trackVoltage * 0.2f;

        if (avgCurrent > 0.05f) { // Ensure some current is flowing
          float r = vApplied / avgCurrent;
          _measuredResistance = r;

          // Note: CV Write is delegated to MotorController (Core 0) to ensure
          // thread safety

          Log.printf("Motor: Measured R=%.2f Ohm\n", r);
          _resistanceState = ResistanceState::DONE;
        } else {
          Log.printf("Motor: Measured current I=%.4f A. Resistance Measurement "
                     "Failed (Low Current)\n",
                     avgCurrent);
          _resistanceState = ResistanceState::ERROR;
        }
      }

      // Update Telemetry even in test mode
      _status.appliedVoltage = _trackVoltage * 0.2f;
      _status.current = avgCurrent;
      _status.estimatedRpm = 0;
      _status.rippleFreq = 0;
      _status.stalled = false;
      _status.duty = _currentDuty;

      continue; // Skip normal control
    } else if (_resistanceState == ResistanceState::DONE ||
               _resistanceState == ResistanceState::ERROR) {
      _currentDuty = 0.0f;
      MotorHal::getInstance().setDuty(0.0f);

      if (millis() - _resistanceStartTime > 6000) {
        _resistanceState = ResistanceState::IDLE;
      }

      // Update Telemetry
      _status.current = avgCurrent;
      _status.duty = 0.0f;

      continue;
    }

    // Ripple Detector
    // Convert buffer to Amps for RippleDetector
    for (size_t i = 0; i < samples; i++) {
      adcBuffer[i] *= 0.00054f;
    }
    _rippleDetector.processBuffer(adcBuffer, samples, sampleRate);
    float rippleFreq = _rippleDetector.getFrequency();

    // 4. Update Estimator
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

        // Data Logging (Decimate to fit 3s into 100 points: every 2nd frame)
        static uint8_t decimate = 0;
        if (++decimate >= 2) {
          decimate = 0;
          if (_testDataIdx < MAX_TEST_POINTS) {
            _testData[_testDataIdx++] = {(uint32_t)t, _targetSpeedStep,
                                         _currentDuty, avgCurrent, actualRpm};
          }
        }
      }
    }

    // 5. Control Loop
    if (_targetSpeedStep == 0) {
      _currentDuty = 0.0f;
      _piErrorSum = 0.0f;
    } else {
      // Target RPM
      float targetRpm = (_targetSpeedStep / 255.0f) * _maxRpm;

      float error = targetRpm - actualRpm;

      // PI
      _piErrorSum += error;
      // Anti-windup
      if (_piErrorSum > 2000.0f)
        _piErrorSum = 2000.0f;
      if (_piErrorSum < -2000.0f)
        _piErrorSum = -2000.0f;

      float output = (_kp * error) + (_ki * _piErrorSum);

      float vControl = output;

      // Constrain Voltage
      if (vControl > _trackVoltage)
        vControl = _trackVoltage;
      if (vControl < 0.0f)
        vControl = 0.0f;

      // Calculate Duty
      float duty = vControl / _trackVoltage;

      // --- DITHER LOGIC ---
      if (_targetSpeedStep > 0 && _targetSpeedStep < 15 && _cvPwmDither > 0) {
        // 40ms period = 25Hz
        unsigned long phase = (xTaskGetTickCount() * portTICK_PERIOD_MS) % 40;

        // Base amplitude: CV 0-255 maps to 0-400 (out of 1023) -> 0-0.39 duty
        float baseAmplitude = (_cvPwmDither / 255.0f) * 0.39f;

        // Fade factor
        float fadeFactor = 1.0f - (_targetSpeedStep / 15.0f);
        float dither = baseAmplitude * fadeFactor;

        if (phase < 20) {
          duty += dither;
        } else {
          duty -= dither;
        }
      }
      // ---------------------

      if (!_targetDirection)
        duty = -duty;
      _currentDuty = duty;
    }

    MotorHal::getInstance().setDuty(_currentDuty);

    // 6. Telemetry Update
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

  // Read CVs
  uint16_t cvRa = dcc.getCV(CV::MOTOR_R_ARM); // 10mOhm units
  float rArm = cvRa * 0.01f;                  // Convert to Ohm

  uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE); // 100mV units
  _trackVoltage = cvTv * 0.1f;                  // Volts
  if (_trackVoltage < 5.0f)
    _trackVoltage = 12.0f; // Safety default

  uint16_t poles = dcc.getCV(CV::MOTOR_POLES);
  if (poles == 0)
    poles = 5;

  _estimator.setMotorParams(rArm, poles);

  // PI Gains
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
    _currentFilter.reset(); // Reset filter
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
             "{\"t\":%u,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             _testData[i].t, _testData[i].target, pwm, _testData[i].current,
             _testData[i].rpm);
    out += buf;
    if (i < _testDataIdx - 1)
      out += ",";
  }
  out += "]";
  return out;
}
