#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include <cmath>

MotorController::MotorController() : 
    _currentDirection(true), 
    _currentSpeed(0.0f), 
    _lastMomentumUpdate(0),
    _lastCvUpdate(0),
    _avgCurrent(0.0f),
    _currentOffset(0.0f),
    _pwmResolution(10),
    _maxPwm(1023),
    _pwmFreq(4000), 
    _cvBaselineAlpha(5),
    _cvStictionKick(50),
    _cvDeltaCap(180),
    _cvPwmDither(0),
    _cvBaselineReset(0),
    _cvCurveIntensity(0)
{
    // Prototypical S-Curve Default (CV 67-94)
    uint8_t sCurve[] = {1, 3, 7, 12, 18, 26, 35, 46, 58, 72, 87, 103, 119, 135, 
                        151, 166, 181, 194, 206, 217, 226, 234, 241, 246, 250, 253, 254, 255};
    
    for (int i = 0; i < 28; i++) {
        _speedTable[i] = sCurve[i];
    }

    for (int i = 0; i < SPEED_STEPS; i++) {
        _baselineTable[i] = 0.0f;
    }
}

void MotorController::setup() {
  Log.println("MotorController: Initializing (10-bit / 16kHz / Slow Decay / 4A OCP)...");

  pinMode(Pinout::MOTOR_IN1, OUTPUT);
  pinMode(Pinout::MOTOR_IN2, OUTPUT);
  pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
  pinMode(Pinout::MOTOR_CURRENT, INPUT);
  pinMode(Pinout::VMOTOR_PG, INPUT_PULLUP); 

  // 1. Hardware Wake-up & Configuration
  digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW); // LOW = 4A Current Limit (Safe for Inrush)
  digitalWrite(Pinout::MOTOR_IN1, HIGH);     // Idle in Brake (High) to keep charge pump active
  digitalWrite(Pinout::MOTOR_IN2, HIGH);
  delay(10); // Stability delay

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  _pwmResolution = 10; 
  _maxPwm = 1023;
  _pwmFreq = 16000; 

  ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
  ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

  _loadBaselineTable();

  // Initialize to Brake (Both HIGH)
  ledcWrite(_pwmChannel1, _maxPwm);
  ledcWrite(_pwmChannel2, _maxPwm);

  // --- Startup Self-Test ---
  Log.printf("Motor: Starting Self-Test (PG State: %s)...\n", digitalRead(Pinout::VMOTOR_PG) == HIGH ? "OK" : "LOW");
  
  _drive(500, true);  // ~50% power FWD
  delay(500);
  _drive(0, true);
  delay(200);
  _drive(500, false); // ~50% power REV
  delay(500);
  _drive(0, true);
  Log.println("Motor: Self-Test Complete.");

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
  }

  _updateCvCache();

  // 1. Oversampled Current Sensing
  // Read 8 times to smooth out the skewed armature commutation noise
  long sum = 0;
  for(int i=0; i<8; i++) sum += analogRead(Pinout::MOTOR_CURRENT);
  float rawAdc = sum / 8.0f;
  
  // Auto-Calibration: If motor is stopped, track the noise floor
  if (targetSpeed == 0 && _currentSpeed < 0.1f) {
      // Slowly follow the idle ADC value to establish a "zero" reference
      _currentOffset = (_currentOffset * 0.99f) + (rawAdc * 0.01f);
  }

  // Subtract offset and clamp to 0
  float netAdc = rawAdc - _currentOffset;
  if (netAdc < 0.5f) netAdc = 0.0f; // Relaxed cutoff to catch tiny signals

  // Convert ADC to Amps (DRV8213 IPROPI Scalar)
  // With 12-bit ADC @ 3.3V (3.3/4095) and R_sense=2400:
  // I_out = ADC * (3.3/4095) * (2000/2400) = ADC * 0.0006715
  float currentAmps = netAdc * 0.0006715f;

  // EMA Filter: Slowed down slightly (0.02) for better stability
  _avgCurrent = (_avgCurrent * 0.98f) + (currentAmps * 0.02f);

  // 2. Momentum Logic (dt tracking)
  unsigned long now = millis();

  // --- SAFETY: Hardware Fault Monitoring ---
  if (digitalRead(Pinout::MOTOR_FAULT) == LOW) {
      Log.println("SAFETY: MOTOR FAULT DETECTED! (Check for Short/Overheat)");
  }

  // --- SAFETY: Stall Watchdog ---
  if (_avgCurrent > _stallThreshold) {
      if (_stallStartTime == 0) {
          _stallStartTime = now;
      } else if (now - _stallStartTime > 2000) {
          // Sustained Stall > 2s
          _drive(0, true);
          _currentSpeed = 0;
          Log.printf("SAFETY: MOTOR STALL DETECTED! (%.2f A)\n", _avgCurrent);
          return;
      }
  } else {
      _stallStartTime = 0; // Reset if current drops
  }

  unsigned long dt = now - _lastMomentumUpdate;
  if (dt >= 5) { // 200Hz update
      _lastMomentumUpdate = now;
      float accelDelay = ((_cvAccel == 0) ? 1 : _cvAccel) * 3.5f;
      float decelDelay = ((_cvDecel == 0) ? 1 : _cvDecel) * 3.5f;
      
      float step = (float)dt / (targetSpeed > _currentSpeed ? accelDelay : decelDelay);
      if (step > 2.0f) step = 2.0f; 

      if (targetSpeed > _currentSpeed) {
        _currentSpeed = min((float)targetSpeed, _currentSpeed + step);
      } else if (targetSpeed < _currentSpeed) {
        _currentSpeed = max((float)targetSpeed, _currentSpeed - step);
      }
  }

  if (_currentSpeed < 1.0f && reqDirection != _currentDirection) {
    _currentDirection = reqDirection;
  }

  // 3. Output Logic
  int32_t pwmOutput = 0;
  int speedIndex = constrain((int)_currentSpeed, 0, 127);

  if (_currentSpeed > 0.1f) {
      // 10-bit Scaling (0-1023)
      // Increased floor to 100 (~40% duty) to overcome Paragon armature stiction
      uint32_t pwmStart = max((uint32_t)_cvVstart, (uint32_t)100) * 4;
      uint32_t pwmMid = _cvVmid * 4;
      uint32_t pwmHigh = _cvVhigh * 4;
      
      float basePwm = 0;
      
      if (_cvConfig & 0x10) { // Bit 4: Use Speed Table
          float tableIndex = (_currentSpeed / 255.0f) * 27.0f;
          int idxA = (int)tableIndex;
          int idxB = min(idxA + 1, 27);
          float fraction = tableIndex - (float)idxA;

          uint32_t valA = map(_speedTable[idxA], 0, 255, pwmStart, _maxPwm);
          uint32_t valB = map(_speedTable[idxB], 0, 255, pwmStart, _maxPwm);
          
          basePwm = (float)valA + (fraction * (float)(valB - valA));
      } else {
          // Standard 3-point curve with Hard Pedestal
          if (_currentSpeed <= 128.0f) {
              basePwm = map((long)(_currentSpeed * 10), 0, 1280, pwmStart, pwmMid);
          } else {
              basePwm = map((long)(_currentSpeed * 10), 1280, 2550, pwmMid, pwmHigh);
          }
      }

      // Ensure basePwm never drops below our minimum turn pedestal while target > 0
      if (basePwm < pwmStart) basePwm = pwmStart;

      // DELTA-BASED COMPENSATION
      float deltaI = _avgCurrent - _baselineTable[speedIndex];
      if (deltaI < 0) deltaI = 0;

      // Apply Load Gain (IR Compensation)
      float comp = deltaI * ((float)_cvLoadGain / 2.0f); // Recalibrated for 10-bit
      
      // Cap boost (Delta Cap)
      float maxComp = ((float)_cvDeltaCap / 255.0f) * 300.0f;
      if (comp > maxComp) comp = maxComp;

      pwmOutput = (int32_t)(basePwm + comp);

      // Zone A Startup Floor & Baseline Learning
      if (_currentSpeed < 20.0f) {
          if (_currentSpeed < 5.0f && _currentSpeed > 0.1f) {
              pwmOutput += _cvStictionKick; 
          }
          
          // Absolute minimum to prevent stall-hum (approx 15% duty)
          if (pwmOutput < 150) pwmOutput = 150; 
          
          // Learn baseline (Adaptive Alpha)
          float alpha = ((float)_cvBaselineAlpha / 1000.0f);
          if (_avgCurrent > 0.01f) { 
              _baselineTable[speedIndex] = (_baselineTable[speedIndex] * (1.0f - alpha)) + (_avgCurrent * alpha);
          }
      }
  }

  // STRICT CLAMPING: Prevent wrap-around to 0 (Full Speed)
  pwmOutput = constrain(pwmOutput, 0, (int32_t)_maxPwm);
  _drive((uint16_t)pwmOutput, _currentDirection);

  // Telemetry Stream
  streamTelemetry();

  // Load Factor reporting for Audio
  static unsigned long lastUpdate = 0;
  if (now - lastUpdate > 100) {
      lastUpdate = now;
      SystemContext::getInstance().getState().loadFactor = constrain((_avgCurrent - _baselineTable[speedIndex]) / 0.1f, 0.0f, 1.0f);
  }
}

/**
 * _drive implements SLOW DECAY (Inverted PWM).
 * Uses a wake-up pulse to prevent auto-sleep lock.
 */
void MotorController::_drive(uint16_t speed, bool direction) {
  _lastPwmValue = (int16_t)speed;

  if (speed == 0) {
      // Brake: Both High
      ledcWrite(_pwmChannel1, _maxPwm);
      ledcWrite(_pwmChannel2, _maxPwm);
      return;
  }

  // WAKE-UP PULSE: Ensure charge pump is alive
  static uint16_t lastSpeed = 0;
  if (lastSpeed == 0 && speed > 0) {
      ledcWrite(_pwmChannel1, _maxPwm);
      ledcWrite(_pwmChannel2, _maxPwm);
      delayMicroseconds(300);
  }
  lastSpeed = speed;

  // --- SINE DITHER ---
  float ditherAmplitude = (float)_cvPwmDither / 8.0f; 
  static float phase = 0.0f;
  static unsigned long lastMicros = 0;
  unsigned long now = micros();
  float dt = (float)(now - lastMicros) / 1000000.0f;
  lastMicros = now;
  if (dt > 0.1f) dt = 0.0f; 
  phase += 6.28318f * 60.0f * dt;
  if (phase > 6.28318f) phase -= 6.28318f;
  float sineOffset = sin(phase) * ditherAmplitude;
  
  int32_t ditheredSpeed = (int32_t)((float)speed + sineOffset);
  ditheredSpeed = constrain(ditheredSpeed, 0, (int32_t)_maxPwm);

  // Slow Decay Inversion: Drive = LOW, Brake = HIGH
  uint32_t invertedPwm = _maxPwm - (uint32_t)ditheredSpeed;

  if (direction) {
    // FWD: PWM on Ch 2 (IN2), Ch 1 (IN1) is hard HIGH
    ledcWrite(_pwmChannel1, _maxPwm);
    ledcWrite(_pwmChannel2, invertedPwm); 
  } else {
    // REV: Ch 2 (IN2) is hard HIGH, PWM on Ch 1 (IN1)
    ledcWrite(_pwmChannel1, invertedPwm);
    ledcWrite(_pwmChannel2, _maxPwm);
  }
}

void MotorController::_updateCvCache() {
    if (millis() - _lastCvUpdate > 500) { 
        _lastCvUpdate = millis();
        NmraDcc &dcc = DccController::getInstance().getDcc();
        
        // 1. Standard DCC Motion CVs
        _cvAccel = dcc.getCV(CV::ACCEL);
        _cvDecel = dcc.getCV(CV::DECEL);
        _cvVstart = dcc.getCV(CV::V_START);
        _cvVmid = dcc.getCV(CV::V_MID);
        _cvVhigh = dcc.getCV(CV::V_HIGH);
        _cvConfig = dcc.getCV(CV::CONFIG);
        _cvLoadGain = dcc.getCV(CV::LOAD_GAIN);
        
        // Read Speed Table (CV 67-94) if enabled
        if (_cvConfig & 0x10) { // Bit 4
            for (int i = 0; i < 28; i++) {
                _speedTable[i] = dcc.getCV(67 + i);
            }
        }

        _cvBaselineAlpha = dcc.getCV(CV::BASELINE_ALPHA);
        _cvStictionKick = dcc.getCV(CV::STICTION_KICK);
        _cvDeltaCap = dcc.getCV(CV::DELTA_CAP);
        _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
        _cvBaselineReset = dcc.getCV(CV::BASELINE_RESET);

        // Parametric S-Curve Generator (CV 66)
        uint8_t newIntensity = dcc.getCV(CV::CURVE_INTENSITY);
        if (newIntensity != _cvCurveIntensity) {
             _cvCurveIntensity = newIntensity;
             if (_cvCurveIntensity > 0) {
                 _generateScurve(_cvCurveIntensity);
                 // Write generated table back to CVs so throttle can read it
                 for (int i=0; i<28; i++) {
                     dcc.setCV(67 + i, _speedTable[i]);
                 }
             }
        }

        // Handle Baseline Commands (CV 65)
        if (_cvBaselineReset == 1) {
            Log.println("Motor: Resetting Baseline Table...");
            for (int i = 0; i < SPEED_STEPS; i++) {
                _baselineTable[i] = 0.0f;
            }
            dcc.setCV(CV::BASELINE_RESET, 0); // Reset the flag
            _cvBaselineReset = 0;
        } else if (_cvBaselineReset == 2) {
            Log.println("Motor: Saving Calibration Snapshot...");
            _saveBaselineTable();
            dcc.setCV(CV::BASELINE_RESET, 0);
            _cvBaselineReset = 0;
        }
        
        // Handle Frequency (Fix overflow and scaling)
        _cvPwmFreq = dcc.getCV(CV::PWM_FREQ);
        _cvPwmFreqH = dcc.getCV(CV::PWM_FREQ_H);
        uint32_t targetFreq = (_cvPwmFreqH << 8) | _cvPwmFreq;
        
        // HARDWARE SAFETY CLAMP (ESP32-S3 10-bit limit is ~19.5kHz)
        if (targetFreq < 50 || targetFreq > 19000) {
            targetFreq = 16000; 
        }
        
        if (targetFreq != _pwmFreq) {
            _pwmFreq = targetFreq;
            Log.printf("Motor: Updating PWM Freq to %u Hz\n", _pwmFreq);
            ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
            ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
            ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
            ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);
            _drive(0, true); 
        }
    }
}

/**
 * Parametric S-Curve Generator
 * intensity: 0 (Linear) to 255 (Max S-Curve)
 * Uses a normalized sigmoid function to generate 28-point speed table.
 */
void MotorController::_generateScurve(uint8_t intensity) {
    // Map CV 0-255 to a k-factor of 0.1 to 12.0
    // Higher k = steeper S-curve (more "crawl" at bottom)
    float k = 0.1f + ((float)intensity / 255.0f) * 11.9f;

    for (int i = 0; i < 28; i++) {
        // Normalized x-axis (0.0 at step 0, 1.0 at step 27)
        float x = (float)i / 27.0f;
        
        // Sigmoid formula centered at 0.5
        // f(x) = 1 / (1 + e^(-k(x-0.5)))
        float sigmoid = 1.0f / (1.0f + exp(-k * (x - 0.5f)));
        
        // Calculate offsets to normalize the curve to pass through exactly (0,0) and (1,1)
        float s0 = 1.0f / (1.0f + exp(-k * (0.0f - 0.5f)));
        float s1 = 1.0f / (1.0f + exp(-k * (1.0f - 0.5f)));
        
        float normalized = (sigmoid - s0) / (s1 - s0);

        // Scale to 8-bit for DCC standard table storage
        _speedTable[i] = (uint8_t)(normalized * 255.0f);
    }
    Log.printf("Motor: Generated S-Curve with K=%.2f\n", k);
}

void MotorController::streamTelemetry() {
    static unsigned long lastStream = 0;
    // 10Hz telemetry stream (every 100ms) is plenty for the dashboard
    if (millis() - lastStream > 100) {
        lastStream = millis();
        
        SystemState &state = SystemContext::getInstance().getState();
        int rawAdc = analogRead(Pinout::MOTOR_CURRENT);
        
        // Output format: [NIMRS_DATA],Target,CurrentSpeed,PWM,Amps,Load,Fault,RawADC,Offset,Gain
        Log.printf("[NIMRS_DATA],%d,%.2f,%d,%.2f,%.2f,%d,%d,%d,%d\n",
            state.speed,
            _currentSpeed,
            _lastPwmValue, 
            _avgCurrent,
            state.loadFactor,
            digitalRead(Pinout::MOTOR_FAULT) == LOW ? 1 : 0,
            rawAdc,
            (int)_currentOffset,
            digitalRead(Pinout::MOTOR_GAIN_SEL)
        );
    }
}

void MotorController::_saveBaselineTable() {
    Preferences prefs;
    prefs.begin("motor", false);
    prefs.putBytes("baseline", _baselineTable, sizeof(_baselineTable));
    prefs.end();
    Log.printf("Motor: Baseline (%d bytes) saved to NVS\n", sizeof(_baselineTable));
}

void MotorController::_loadBaselineTable() {
    Preferences prefs;
    prefs.begin("motor", true);
    size_t len = prefs.getBytes("baseline", _baselineTable, sizeof(_baselineTable));
    prefs.end();

    if (len == sizeof(_baselineTable)) {
        Log.println("Motor: Calibration Snapshot loaded from NVS");
    } else {
        Log.println("Motor: No valid Calibration Snapshot found, starting fresh");
        for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.0f;
    }
}