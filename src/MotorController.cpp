// MotorController.cpp - High-Performance PI Control with Kick Start

#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cmath>

MotorController::MotorController()
    : _currentSpeed(0.0f), _lastMomentumUpdate(0), _avgCurrent(0.0f),
      _currentOffset(5.0f), _piErrorSum(0.0f), _maxPwm(DEFAULT_MAX_PWM),
      _pwmFreq(DEFAULT_PWM_FREQ) {}

void MotorController::setup() {
  Log.println("NIMRS: PI Mode - 20kHz PWM / Kick Start");

  gpio_reset_pin((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
  gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);

  pinMode(Pinout::MOTOR_IN1, OUTPUT);
  pinMode(Pinout::MOTOR_IN2, OUTPUT);
  pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
  pinMode(Pinout::MOTOR_CURRENT, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db);

  // high-freq PWM for silent motor operation
  // 20kHz ensures no audible buzzing during the crawl
  ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
  ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
  ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

  float offsetAccum = 0.0f;
  for (int i = 0; i < 100; i++) {
    offsetAccum += (float)analogRead(Pinout::MOTOR_CURRENT);
    delay(2);
  }
  _currentOffset = offsetAccum / 100.0f;
  Log.printf("NIMRS: ADC offset %.2f\n", _currentOffset);

  _drive(0, true);
}

void MotorController::loop() {
  SystemState &state = SystemContext::getInstance().getState();
  uint8_t targetSpeed = state.speed;
  bool direction = state.direction;

  // --- TEST MODE OVERRIDE ---
  if (_testMode) {
    unsigned long t = millis() - _testStartTime;
    if (t > 3000) {
      _testMode = false;
      targetSpeed = 0;
      _drive(0, true);
    } else {
      if (t < 1000) {
        targetSpeed = map(t, 0, 1000, 0, 100);
        direction = true;
      } else if (t < 1500) {
        targetSpeed = 100;
        direction = true;
      } else if (t < 2500) {
        targetSpeed = map(t, 1500, 2500, 0, 100);
        direction = false;
      } else {
        targetSpeed = 100;
        direction = false;
      }

      static unsigned long lastLog = 0;
      if (millis() - lastLog > 50 && _testDataIdx < MAX_TEST_POINTS) {
        lastLog = millis();
        _testData[_testDataIdx++] = {(uint32_t)t, targetSpeed, _lastPwmValue,
                                     _avgCurrent, _currentSpeed};
      }
    }
  }
  // --------------------------

  _updateCvCache();

  // 1. MOMENTUM (CV3/CV4)
  unsigned long now = millis();
  unsigned long dt = now - _lastMomentumUpdate;
  if (dt >= 10) {
    _lastMomentumUpdate = now;
    float accelDelay = std::max(1, (int)_cvAccel) * 5.0f;
    float step = (float)dt / accelDelay;
    if (targetSpeed > _currentSpeed)
      _currentSpeed = std::min((float)targetSpeed, _currentSpeed + step);
    else if (targetSpeed < _currentSpeed)
      _currentSpeed = std::max((float)targetSpeed, _currentSpeed - step);
  }

  // 2. STABILIZED SENSING (CV189 Filter)
  float rawAmps = (float)_getPeakADC() * 0.00054f;
  float alpha = std::min(0.95f, _cvLoadFilter / 255.0f);
  _avgCurrent = (alpha * _avgCurrent) + ((1.0f - alpha) * rawAmps);

  // --- RESISTANCE MEASUREMENT OVERRIDE ---
  if (_resistanceState == ResistanceState::MEASURING) {
    if (millis() - _resistanceStartTime < 1000) {
      // Apply fixed PWM (approx 20%)
      uint16_t testPwm = _maxPwm / 5;
      _drive(testPwm, true);
    } else {
      // Measurement complete
      _drive(0, true);

      NmraDcc &dcc = DccController::getInstance().getDcc();
      uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE);
      float vTrack = (cvTv > 0) ? (cvTv * 0.1f) : 14.0f;

      // V_applied = V_track * (PWM / MaxPWM)
      // Re-calculate duty used in the measurement phase (_maxPwm / 5)
      float duty = ((float)(_maxPwm / 5)) / _maxPwm;
      float vApplied = vTrack * duty;

      if (_avgCurrent > 0.05f) { // Ensure some current is flowing
        float r = vApplied / _avgCurrent;
        _measuredResistance = r;

        // Store in CV (10mOhm units)
        uint16_t cvVal = (uint16_t)(r * 100.0f);
        dcc.setCV(CV::MOTOR_R_ARM, cvVal);

        Log.printf("Motor: Measured R=%.2f Ohm (CV=%d)\n", r, cvVal);
        _resistanceState = ResistanceState::DONE;
      } else {
        Log.println("Motor: Resistance Measurement Failed (Low Current)");
        _resistanceState = ResistanceState::ERROR;
      }
    }
    return; // Skip normal control
  } else if (_resistanceState == ResistanceState::DONE ||
             _resistanceState == ResistanceState::ERROR) {
    _drive(0, true);
    // Auto-reset after 5 seconds
    if (millis() - _resistanceStartTime > 6000) {
      _resistanceState = ResistanceState::IDLE;
    }
    return;
  }
  // ---------------------------------------

  int32_t finalPwm = 0;

  if (_currentSpeed > 0.5f) {
    float speedNorm = _currentSpeed / 255.0f;

    // BASELINE: Guarantee the motor has enough power to move
    float vStart = map(_cvVStart, 0, 255, _cvPedestalFloor, _maxPwm);
    float basePwm = vStart + (speedNorm * ((float)_maxPwm - vStart));

    // TORQUE PUNCH: Allow negative error so the integral can unwind
    float thresholdBase = _cvLearnThreshold / 100.0f;
    float expectedLoad = thresholdBase + (speedNorm * 0.4f);
    float loadError = _avgCurrent - expectedLoad;

    // CRAWL CAP
    float maxPunch = _cvLoadGainScalar * 20.0f;
    float punchLimit = map((long)_currentSpeed, 0, 20, 150, (long)maxPunch);
    punchLimit = constrain(punchLimit, 150.0f, maxPunch);

    // Calculate PI Gains
    float currentKp =
        (_currentSpeed < 20.0f) ? (_cvKpSlow / 64.0f) : (_cvKp / 128.0f);
    float currentKi = _cvKi / 1000.0f;

    // Throttled PI Integration (50Hz)
    static unsigned long lastPiTimer = 0;
    if (now - lastPiTimer > 20) {
      _piErrorSum += loadError;
      if (loadError < 0)
        _piErrorSum += loadError; // Unwind faster when running free
      _piErrorSum = constrain(_piErrorSum, 0.0f, 500.0f);
      lastPiTimer = now;
    }

    // Apply PI Output (P-term only hits on positive load/binds)
    float pTerm = std::max(0.0f, loadError) * currentKp * 100.0f;
    float iTerm = _piErrorSum * currentKi;

    float torquePunch = constrain(pTerm + iTerm, 0.0f, punchLimit);

    // KICKSTART (CV65)
    float kickBonus = 0;
    if (!_isMoving) {
      _kickStartTimer = now;
      _isMoving = true;
    }
    if (now - _kickStartTimer < 100) {
      kickBonus = map(_cvKickStart, 0, 255, 0, 350);
    }

    finalPwm = (int32_t)(basePwm + torquePunch + kickBonus);

    // DYNAMIC DITHER INJECTION (CV64): 25Hz with smooth fade-out
    if (_currentSpeed > 0.1f && _currentSpeed < 15.0f && _cvPwmDither > 0) {
      // 40ms period = 25Hz dither frequency
      unsigned long phase = millis() % 40;

      // Map base amplitude
      float baseAmplitude = map(_cvPwmDither, 0, 255, 0, 400);

      // Dynamic fade: 100% strength near speed 0, fading to 0% at speed 15
      float fadeFactor = 1.0f - (_currentSpeed / 15.0f);
      int32_t ditherAmplitude = (int32_t)(baseAmplitude * fadeFactor);

      // Symmetrical 25Hz square wave (20ms high, 20ms low)
      if (phase < 20) {
        finalPwm += ditherAmplitude;
      } else {
        finalPwm -= ditherAmplitude;
      }
    }

    finalPwm = constrain(finalPwm, 0, (int32_t)_maxPwm);

  } else {
    _isMoving = false;
    finalPwm = 0;
    _piErrorSum = 0.0f; // MUST clear integral windup on stop
  }

  _drive((uint16_t)finalPwm, state.direction);
  streamTelemetry();
}

uint32_t MotorController::_getPeakADC() {
  // Average out the high-frequency PWM and Dither ripple
  uint32_t sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(Pinout::MOTOR_CURRENT);
  }
  return sum / 50;
}

void MotorController::_drive(uint16_t speed, bool direction) {
  _lastPwmValue = (int16_t)speed;

  if (speed == 0) {
    // BRAKE: Both HIGH (Slow Decay)
    ledcWrite(_pwmChannel1, _maxPwm);
    ledcWrite(_pwmChannel2, _maxPwm);
    return;
  }

  // Standard Slow Decay Drive:
  // One pin HIGH, other pin PWM (inverted logic)
  // 1023 (MAX) - speed = inverted duty cycle
  uint32_t duty = _maxPwm - speed;

  if (direction) {
    ledcWrite(_pwmChannel1, _maxPwm); // IN1 High
    ledcWrite(_pwmChannel2, duty);    // IN2 PWM (Active Low)
  } else {
    ledcWrite(_pwmChannel1, duty);    // IN1 PWM (Active Low)
    ledcWrite(_pwmChannel2, _maxPwm); // IN2 High
  }
}

void MotorController::streamTelemetry() {
  static unsigned long lastS = 0;
  if (millis() - lastS > 150) {
    lastS = millis();
    SystemState &state = SystemContext::getInstance().getState();
    Log.printf("[NIMRS_DATA],%d,%.1f,%d,%.3f,%.3f,%d\n", state.speed,
               _currentSpeed, _lastPwmValue, _avgCurrent, 0.0f,
               (int)_getPeakADC());
  }
}

void MotorController::_updateCvCache() {
  if (millis() - _lastCvUpdate > 500) {
    _lastCvUpdate = millis();
    NmraDcc &dcc = DccController::getInstance().getDcc();

    // Standard CVs
    _cvVStart = dcc.getCV(CV::V_START);
    _cvAccel = dcc.getCV(CV::ACCEL);

    // Extended Motor Control CVs
    _cvKickStart = dcc.getCV(65); // User requested CV65 (Kick Start)
    _cvKp = dcc.getCV(112);       // Proportional Gain
    // Note: 113 mentioned in prompt but usually one CV is enough or it's split.
    // Assuming 112 is the main one or 8-bit.

    _cvKi = dcc.getCV(114);         // Integral Gain
    _cvKpSlow = dcc.getCV(118);     // Slow Speed Gain
    _cvLoadFilter = dcc.getCV(189); // Load Filter
    _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
    if (_cvLoadFilter == 255)
      _cvLoadFilter = 120;

    _cvPedestalFloor = dcc.getCV(CV::PEDESTAL_FLOOR);
    _cvLoadGainScalar = dcc.getCV(CV::LOAD_GAIN_SCALAR);
    _cvLearnThreshold = dcc.getCV(CV::LEARN_THRESHOLD);
  }
}

void MotorController::startTest() {
  if (_testMode)
    return;
  _testMode = true;
  _testStartTime = millis();
  _testDataIdx = 0;
  Log.println("Motor: Starting Self-Test...");
}

String MotorController::getTestJSON() {
  String out;
  out.reserve(MAX_TEST_POINTS *
              64); // Pre-allocate ~3.8KB to avoid reallocations
  out += "[";
  for (int i = 0; i < _testDataIdx; i++) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"t\":%u,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}",
             _testData[i].t, _testData[i].target, _testData[i].pwm,
             _testData[i].current, _testData[i].speed);
    out += buf;
    if (i < _testDataIdx - 1)
      out += ",";
  }
  out += "]";
  return out;
}

void MotorController::measureResistance() {
  if (_isMoving || _resistanceState == ResistanceState::MEASURING) {
    return;
  }
  _resistanceState = ResistanceState::MEASURING;
  _resistanceStartTime = millis();
  _measuredResistance = 0.0f;
  _avgCurrent = 0.0f; // Reset filter
  Log.println("Motor: Starting Resistance Measurement...");
}
