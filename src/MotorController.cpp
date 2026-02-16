#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "driver/gpio.h"
#include <algorithm> // For std::sort
#include <cmath>

MotorController::MotorController()
    : _currentDirection(true), _currentSpeed(0.0f), _lastMomentumUpdate(0), 
      _lastCvUpdate(0), _avgCurrent(0.0f), _currentOffset(0.0f),
      _pwmResolution(12), _maxPwm(4095), _pwmFreq(16000), _cvBaselineAlpha(5),
      _cvStictionKick(50), _cvDeltaCap(180), _cvPwmDither(0),
      _cvBaselineReset(0), _cvCurveIntensity(0), _torqueIntegrator(0.0f),
      _startupKickActive(false), _startupKickTime(0), _lastBaselineUpdate(0),
      _adcIdx(0), _lastPwmValue(0), _lastSmoothingPwm(0), _stallStartTime(0),
      _stallThreshold(0.5f), _cvAccel(3), _cvDecel(3), _cvVstart(40),
      _cvVmid(128), _cvVhigh(255), _cvConfig(0), _cvPedestalFloor(35),
      _cvLoadGain(128), _cvLoadGainScalar(128), _cvLearnThreshold(5),
      _cvHardwareGain(0), _cvDriveMode(0) {
  for (int i = 0; i < 16; i++)
    _adcHistory[i] = 0;

  uint8_t sCurve[] = {1,   3,   7,   12,  18,  26,  35,  46,  58,  72,
                      87,  103, 119, 135, 151, 166, 181, 194, 206, 217,
                      226, 234, 241, 246, 250, 253, 254, 255};

  for (int i = 0; i < 28; i++) {
    _speedTable[i] = sCurve[i];
  }

  for (int i = 0; i < SPEED_STEPS; i++) {
    _baselineTable[i] = 0.0f;
  }
}

void MotorController::setup() {
  // Unlock JTAG pins (GPIO 39-41) for IO use
  gpio_reset_pin((gpio_num_t)39);
  gpio_reset_pin((gpio_num_t)40);
  gpio_reset_pin((gpio_num_t)41);

  Log.println("\n╔════════════════════════════════════╗");
  Log.println("║  MOTOR CONTROLLER BOOT SEQUENCE    ║");
  Log.println("╚════════════════════════════════════╝\n");

  // Step 1: Force Stop (IN/IN Brake)
  pinMode(Pinout::MOTOR_IN1, OUTPUT);
  pinMode(Pinout::MOTOR_IN2, OUTPUT);
  digitalWrite(Pinout::MOTOR_IN1, LOW);
  digitalWrite(Pinout::MOTOR_IN2, LOW);
  delay(200);

  // Step 2: Initialize I/O
  // To wake the DRV8213, we must NOT drive Pin 6 LOW. 
  // We set it to INPUT (High-Z) to enable High-Accuracy Gain Mode.
  pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
  pinMode(Pinout::MOTOR_CURRENT, INPUT);
  pinMode(Pinout::VMOTOR_PG, INPUT_PULLUP);

  delay(20); // Give the driver time to wake up (~1ms required)

  // Step 3: Configure ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Step 4: Symmetric PWM Setup (Fast Decay)
  ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
  ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
  ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

  ledcWrite(_pwmChannel1, 0);
  ledcWrite(_pwmChannel2, 0);

  // Step 5: High-Precision Calibration
  Log.println("Step 5: Calibrating current sensor...");
  delay(500);

  float sumOffset = 0;
  for (int i = 0; i < 200; i++) {
    sumOffset += analogRead(Pinout::MOTOR_CURRENT);
    delay(2);
  }
  _currentOffset = sumOffset / 200.0f;
  Log.printf("  ✓ ADC offset = %.1f counts\n", _currentOffset);

  _loadBaselineTable();

  // Set Optimal Defaults for 5-pole motor
  NmraDcc &dcc = DccController::getInstance().getDcc();
  if (dcc.getCV(CV::V_START) == 0) {
    Log.println("Step 7: Writing default CVs...");
    dcc.setCV(CV::V_START, 60);
    dcc.setCV(CV::PEDESTAL_FLOOR, 80);
    dcc.setCV(CV::ACCEL, 25);
    dcc.setCV(CV::DECEL, 20);
    dcc.setCV(CV::LOAD_GAIN, 120);
    dcc.setCV(CV::LOAD_GAIN_SCALAR, 80);
    dcc.setCV(CV::STICTION_KICK, 80);
    dcc.setCV(CV::BASELINE_ALPHA, 5);
    Log.println("  ✓ CVs written");
  }

  Log.printf("Power Good: %s\n",
             digitalRead(Pinout::VMOTOR_PG) == HIGH ? "✓ OK" : "✗ FAIL");
  _drive(0, true);
}

void MotorController::loop() {
  uint8_t targetSpeed;
  bool reqDirection;

  {
    SystemContext &ctx = SystemContext::getInstance();
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();
    targetSpeed = state.speed;
    reqDirection = state.direction;

    // DCC Packet Watchdog
    if (state.lastDccPacketTime > 0 &&
        (millis() - state.lastDccPacketTime) > 2000) {
      _drive(0, true);
      _currentSpeed = 0;
      return;
    }
  }

  targetSpeed = constrain(targetSpeed, 0, 255);
  _updateCvCache();

  // --- ROBUST ADC FILTERING (Median of 5 + Moving Average) ---
  uint32_t samples[5];
  for (int i = 0; i < 5; i++)
    samples[i] = analogRead(Pinout::MOTOR_CURRENT);
  std::sort(samples, samples + 5);
  float medianSample = (float)samples[2];

  _adcHistory[_adcIdx] = medianSample;
  _adcIdx = (_adcIdx + 1) % 16;
  float sumAdc = 0;
  for (int i = 0; i < 16; i++)
    sumAdc += _adcHistory[i];
  float rawAdc = sumAdc / 16.0f;

  // Update offset only when truly stopped
  if (targetSpeed == 0 && _currentSpeed < 0.1f) {
    _currentOffset = (_currentOffset * 0.995f) + (rawAdc * 0.005f);
    _torqueIntegrator = 0.0f;
  }

  // --- NOISE FLOOR DEADBAND ---
  float netAdc = rawAdc - _currentOffset;
  if (abs(netAdc) < 10.0f)
    netAdc = 0.0f;

  float currentAmps = netAdc * 0.001638f;
  _avgCurrent = (_avgCurrent * 0.98f) + (currentAmps * 0.02f);

  unsigned long now = millis();

  // Stall Protection
  if (_avgCurrent > _stallThreshold) {
    if (_stallStartTime == 0)
      _stallStartTime = now;
    else if (now - _stallStartTime > 2000) {
      _drive(0, true);
      _currentSpeed = 0;
      Log.printf("SAFETY: MOTOR STALL DETECTED!\n");
      return;
    }
  } else
    _stallStartTime = 0;

  // Momentum
  unsigned long dt = now - _lastMomentumUpdate;
  if (dt >= 5) {
    _lastMomentumUpdate = now;
    float accelDelay = max(1, (int)_cvAccel) * 3.5f;
    float decelDelay = max(1, (int)_cvDecel) * 3.5f;
    float step =
        (float)dt / (targetSpeed > _currentSpeed ? accelDelay : decelDelay);
    if (step > 2.0f)
      step = 2.0f;
    if (targetSpeed > _currentSpeed)
      _currentSpeed = min((float)targetSpeed, _currentSpeed + step);
    else if (targetSpeed < _currentSpeed)
      _currentSpeed = max((float)targetSpeed, _currentSpeed - step);
  }

  // Direction Logic
  if (reqDirection != _currentDirection) {
    if (_currentSpeed < 0.5f) {
      _currentDirection = reqDirection;
      Log.printf("Motor: Direction changed to %s\n",
                 _currentDirection ? "FORWARD" : "REVERSE");
    } else {
      _currentSpeed = max(0.0f, _currentSpeed - 5.0f);
      return;
    }
  }

  int32_t pwmOutput = 0;
  int speedIndex = constrain((int)_currentSpeed, 0, 255);

  if (_currentSpeed > 0.1f) {
    float speedNorm = _currentSpeed / 255.0f;
    uint32_t pwmStart =
        max((uint32_t)_cvVstart, (uint32_t)_cvPedestalFloor) * 16;
    if (pwmStart < 205)
      pwmStart = 205;

    float basePwm = (float)pwmStart + (speedNorm * (float)(_maxPwm - pwmStart));

    if (_cvConfig & 0x10) {
      int tableIdx = constrain((int)(speedNorm * 27.0f), 0, 27);
      speedNorm = (float)_speedTable[tableIdx] / 255.0f;
      basePwm = (float)pwmStart + (speedNorm * (float)(_maxPwm - pwmStart));
    }

    // --- PI LOAD COMPENSATION ---
    float deltaI = _avgCurrent - _baselineTable[speedIndex];
    if (deltaI < 0)
      deltaI = 0;

    float gainScale = ((float)_cvLoadGain / 255.0f);
    float scalarScale = ((float)_cvLoadGainScalar / 255.0f);

    float Kp = 4.0f * gainScale;
    float Ki = 0.04f * scalarScale;

    _torqueIntegrator += deltaI * Ki;
    float maxComp = ((float)_cvDeltaCap / 255.0f) * 500.0f;
    _torqueIntegrator = constrain(_torqueIntegrator, 0.0f, maxComp);

    if (deltaI < 0.005f)
      _torqueIntegrator *= 0.95f;

    float comp = (deltaI * Kp) + _torqueIntegrator;
    pwmOutput = (int32_t)(basePwm + comp);

    // Startup Kick
    if (_currentSpeed > 0.1f && _currentSpeed < 4.0f) {
      if (!_startupKickActive) {
        _startupKickActive = true;
        _startupKickTime = now;
        pwmOutput += 200;
      }
      if (now - _startupKickTime > 150)
        _startupKickActive = false;
    } else
      _startupKickActive = false;

    // Baseline Learning
    if (_avgCurrent > 0.005f && _currentSpeed > (float)_cvLearnThreshold &&
        (now - _lastBaselineUpdate > 500)) {
      _lastBaselineUpdate = now;
      float alpha = ((float)_cvBaselineAlpha / 1000.0f);
      _baselineTable[speedIndex] =
          (_baselineTable[speedIndex] * (1.0f - alpha)) + (_avgCurrent * alpha);
    }
  }

  pwmOutput = constrain(pwmOutput, 0, (int32_t)_maxPwm);

  // Smoothing
  int32_t pwmDelta = pwmOutput - _lastSmoothingPwm;
  if (abs(pwmDelta) > 150)
    pwmOutput = _lastSmoothingPwm + (pwmDelta > 0 ? 150 : -150);
  _lastSmoothingPwm = (uint16_t)pwmOutput;

  _drive((uint16_t)pwmOutput, _currentDirection);

  static unsigned long lastStream = 0;
  if (now - lastStream > 1000) {
    lastStream = now;
    streamTelemetry();
  }
}

void MotorController::_drive(uint16_t speed, bool direction) {
  _lastPwmValue = (int16_t)speed;

  if (speed == 0) {
    ledcWrite(_pwmChannel1, 0);
    ledcWrite(_pwmChannel2, 0);
    return;
  }

  // Drive Mode: 0=Fast Decay (Coast), 1=Slow Decay (Brake)
  if (_cvDriveMode == 1) {
    uint32_t duty = _maxPwm - speed;
    if (direction) {
      // Forward: IN1=HIGH, IN2=PWM(Inv)
      ledcWrite(_pwmChannel1, _maxPwm);
      ledcWrite(_pwmChannel2, duty);
    } else {
      // Reverse: IN1=PWM(Inv), IN2=HIGH
      ledcWrite(_pwmChannel1, duty);
      ledcWrite(_pwmChannel2, _maxPwm);
    }
  } else {
    // Standard Fast Decay
    if (direction) {
      // Symmetric IN/IN: IN1 = PWM, IN2 = LOW
      ledcWrite(_pwmChannel2, 0);
      ledcWrite(_pwmChannel1, (uint32_t)speed);
    } else {
      // Symmetric IN/IN: IN1 = LOW, IN2 = PWM
      ledcWrite(_pwmChannel1, 0);
      ledcWrite(_pwmChannel2, (uint32_t)speed);
    }
  }
}

void MotorController::_updateCvCache() {
  if (millis() - _lastCvUpdate > 500) {
    _lastCvUpdate = millis();
    NmraDcc &dcc = DccController::getInstance().getDcc();

    _cvAccel = dcc.getCV(CV::ACCEL);
    _cvDecel = dcc.getCV(CV::DECEL);
    _cvVstart = dcc.getCV(CV::V_START);
    _cvVmid = dcc.getCV(CV::V_MID);
    _cvVhigh = dcc.getCV(CV::V_HIGH);
    _cvConfig = dcc.getCV(CV::CONFIG);
    _cvLoadGain = dcc.getCV(CV::LOAD_GAIN);

    if (_cvConfig & 0x10) {
      for (int i = 0; i < 28; i++) {
        _speedTable[i] = dcc.getCV(67 + i);
      }
    }

    _cvBaselineAlpha = dcc.getCV(CV::BASELINE_ALPHA);
    _cvStictionKick = dcc.getCV(CV::STICTION_KICK);
    _cvDeltaCap = dcc.getCV(CV::DELTA_CAP);
    _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
    _cvBaselineReset = dcc.getCV(CV::BASELINE_RESET);

    _cvDriveMode = dcc.getCV(CV::DRIVE_MODE);
    _cvPedestalFloor = dcc.getCV(CV::PEDESTAL_FLOOR);
    _cvLoadGainScalar = dcc.getCV(CV::LOAD_GAIN_SCALAR);
    _cvLearnThreshold = dcc.getCV(CV::LEARN_THRESHOLD);
    uint8_t newHardwareGain = dcc.getCV(CV::HARDWARE_GAIN);

    if (newHardwareGain != _cvHardwareGain) {
      _cvHardwareGain = newHardwareGain;
      // GAIN_SEL: 0=Low (Output Low), 1=High-Z (Input), 2=High (Output High)
      if (_cvHardwareGain == 1) {
        pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
        Log.println("Motor: Hardware Gain switched to HIGH-Z (Med)");
      } else {
        pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
        digitalWrite(Pinout::MOTOR_GAIN_SEL, _cvHardwareGain == 2 ? HIGH : LOW);
        Log.printf("Motor: Hardware Gain switched to %s\n",
                   _cvHardwareGain == 2 ? "HIGH" : "LOW");
      }
    }

    uint8_t newIntensity = dcc.getCV(CV::CURVE_INTENSITY);
    if (newIntensity != _cvCurveIntensity) {
      _cvCurveIntensity = newIntensity;
      if (_cvCurveIntensity > 0) {
        _generateScurve(_cvCurveIntensity);
        for (int i = 0; i < 28; i++) {
          dcc.setCV(67 + i, _speedTable[i]);
        }
      }
    }

    if (_cvBaselineReset == 1) {
      Log.println("Motor: Resetting Baseline Table...");
      for (int i = 0; i < SPEED_STEPS; i++) {
        _baselineTable[i] = 0.0f;
      }
      dcc.setCV(CV::BASELINE_RESET, 0);
      _cvBaselineReset = 0;
    } else if (_cvBaselineReset == 2) {
      Log.println("Motor: Saving Calibration Snapshot...");
      _saveBaselineTable();
      dcc.setCV(CV::BASELINE_RESET, 0);
      _cvBaselineReset = 0;
    }
  }
}

void MotorController::_generateScurve(uint8_t intensity) {
  float k = 0.1f + ((float)intensity / 255.0f) * 11.9f;
  for (int i = 0; i < 28; i++) {
    float x = (float)i / 27.0f;
    float sigmoid = 1.0f / (1.0f + exp(-k * (x - 0.5f)));
    float s0 = 1.0f / (1.0f + exp(-k * (0.0f - 0.5f)));
    float s1 = 1.0f / (1.0f + exp(-k * (1.0f - 0.5f)));
    float normalized = (sigmoid - s0) / (s1 - s0);
    _speedTable[i] = (uint8_t)(normalized * 255.0f);
  }
  Log.printf("Motor: Generated S-Curve with K=%.2f\n", k);
}

void MotorController::streamTelemetry() {
  SystemState &state = SystemContext::getInstance().getState();
  int rawAdc = analogRead(Pinout::MOTOR_CURRENT);
  float netAdc = (float)rawAdc - _currentOffset;

  Log.printf("[NIMRS_DATA],%d,%.2f,%d,%.3f,%.2f,%d,%d,%.1f,%d,%.1f\n",
             state.speed, _currentSpeed, _lastPwmValue, _avgCurrent,
             state.loadFactor, 0, rawAdc, _currentOffset,
             digitalRead(Pinout::MOTOR_GAIN_SEL), netAdc);
}

void MotorController::_saveBaselineTable() {
  Preferences prefs;
  prefs.begin("motor", false);
  prefs.putBytes("baseline", _baselineTable, sizeof(_baselineTable));
  prefs.end();
  Log.printf("Motor: Baseline saved to NVS\n");
}

void MotorController::_loadBaselineTable() {
  Preferences prefs;
  prefs.begin("motor", true);
  size_t len =
      prefs.getBytes("baseline", _baselineTable, sizeof(_baselineTable));
  prefs.end();

  if (len == sizeof(_baselineTable)) {
    Log.println("Motor: Calibration Snapshot loaded from NVS");
  } else {
    Log.println("Motor: No valid Calibration Snapshot found, starting fresh");
    for (int i = 0; i < SPEED_STEPS; i++)
      _baselineTable[i] = 0.0f;
  }
}