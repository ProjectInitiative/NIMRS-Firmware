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
    _pwmResolution(10),
    _maxPwm(1023),
    _pwmFreq(25000),
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
  Log.println("MotorController: Initializing (Slow Decay / Delta-IR Strategy)...");

  pinMode(Pinout::MOTOR_IN1, OUTPUT);
  pinMode(Pinout::MOTOR_IN2, OUTPUT);
  pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
  pinMode(Pinout::MOTOR_CURRENT, INPUT);
  pinMode(Pinout::VMOTOR_PG, INPUT_PULLUP); 

  analogReadResolution(12); // Ensure 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db); // Back to 3.3V range for stability during initial tuning

  // Initialize DRV8213 to a "Brake" state (Both High)
  ledcWrite(_pwmChannel1, _maxPwm);
  ledcWrite(_pwmChannel2, _maxPwm);
  digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW); // Start with Low Gain for cleaner signal

  // Using 13-bit for Paragon-level precision
  _pwmResolution = 13; 
  _maxPwm = (1 << _pwmResolution) - 1; // 8191

  ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);

  ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

  // Initialize to Brake (Both High)
  ledcWrite(_pwmChannel1, _maxPwm);
  ledcWrite(_pwmChannel2, _maxPwm);

  // --- Startup Self-Test ---
  // Spin the motor briefly to verify H-Bridge and Slow Decay logic
  Log.println("Motor: Starting Self-Test...");
  _drive(1000, true);  // ~12% power FWD
  delay(500);
  _drive(0, true);
  delay(200);
  _drive(1000, false); // ~12% power REV
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
  // Read 4 times to smooth out the skewed armature commutation noise
  long sum = 0;
  for(int i=0; i<4; i++) sum += analogRead(Pinout::MOTOR_CURRENT);
  float rawAdc = sum / 4.0f;
  
  // Convert ADC to Amps (DRV8213 IPROPI Scalar)
  // V = (I_out / 2000) * R_sense
  // I_out = (V * 2000) / R_sense
  // With 12-bit ADC @ 3.3V (3.3/4095) and R_sense=2000:
  // I_out = ADC * (3.3/4095) * (2000/2000) = ADC * 0.000805
  float currentAmps = rawAdc * 0.0008058f;

  // EMA Filter: Slowed down slightly (0.02) for better stability
  _avgCurrent = (_avgCurrent * 0.98f) + (currentAmps * 0.02f);

  // Auto-Zero Baseline when stopped
  if (targetSpeed == 0 && _currentSpeed < 0.1f) {
      _baselineTable[0] = (_baselineTable[0] * 0.95f) + (_avgCurrent * 0.05f);
  }

  // 2. Momentum Logic (dt tracking)
  unsigned long now = millis();

  // --- SAFETY: Hardware Fault Monitoring ---
  if (digitalRead(Pinout::MOTOR_FAULT) == LOW) {
      // Latched Fault State: Kill Drive immediately
      _drive(0, true);
      _currentSpeed = 0;
      Log.println("SAFETY: MOTOR FAULT DETECTED! (Pin Low)");
      delay(100); // Debounce/Limit log spam
      return; 
  }

  // --- SAFETY: Stall Watchdog ---
  if (_avgCurrent > _stallThreshold) {
      if (_stallStartTime == 0) {
          _stallStartTime = now;
      } else if (now - _stallStartTime > 2000) {
          // Sustained Stall > 2s
          _drive(0, true);
          _currentSpeed = 0;
          Log.printf("SAFETY: MOTOR STALL DETECTED! (%.2f A > %.2f A)\n", _avgCurrent, _stallThreshold);
          delay(1000); // Cool down
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
      // Scale CVs (Vstart/Mid/High are 0-255, we are 13-bit 0-8191)
      // 255 * 32 = 8160 (close enough to 8191)
      uint32_t pwmStart = max((uint32_t)_cvVstart, (uint32_t)100) * 32;
      uint32_t pwmMid = _cvVmid * 32;
      uint32_t pwmHigh = _cvVhigh * 32;
      
      float basePwm = 0;
      
      if (_cvConfig & 0x10) { // Bit 4: Use Speed Table
          // Map 0-255 speed to 28 table entries
          float tableIndex = (_currentSpeed / 255.0f) * 27.0f;
          int idxA = (int)tableIndex;
          int idxB = min(idxA + 1, 27);
          float fraction = tableIndex - (float)idxA;

          // Map table values (0-255) to the range [pwmStart, _maxPwm]
          // This ensures the table sits on top of the voltage pedestal
          // Note: map() works with integers, so we cast carefully
          uint32_t valA = map(_speedTable[idxA], 0, 255, pwmStart, _maxPwm);
          uint32_t valB = map(_speedTable[idxB], 0, 255, pwmStart, _maxPwm);
          
          basePwm = (float)valA + (fraction * (float)(valB - valA));
      } else {
          // Standard 3-point curve
          // Input range 0-255. Midpoint is ~128.
          if (_currentSpeed <= 128.0f) {
              basePwm = map((long)(_currentSpeed * 10), 0, 1280, pwmStart, pwmMid);
          } else {
              basePwm = map((long)(_currentSpeed * 10), 1280, 2550, pwmMid, pwmHigh);
          }
      }

      // DELTA-BASED COMPENSATION
      // Calculate how much extra current we are drawing compared to "empty track"
      float deltaI = _avgCurrent - _baselineTable[speedIndex];
      if (deltaI < 0) deltaI = 0;

      // Apply Load Gain (IR Compensation)
      float comp = deltaI * ((float)_cvLoadGain / 8.0f); // Tuned for 13-bit
      
      // Cap boost (Delta Cap) - Scale 0-255 CV to PWM range (approx 1500 max)
      float maxComp = ((float)_cvDeltaCap / 255.0f) * 1500.0f;
      if (comp > maxComp) comp = maxComp;

      pwmOutput = (int32_t)(basePwm + comp);

      // Zone A Startup Floor & Baseline Learning
      if (_currentSpeed < 20.0f) {
          // Stiction Kick: Add pulse for very low speeds
          if (_currentSpeed < 5.0f && _currentSpeed > 0.1f) {
              pwmOutput += (_cvStictionKick * 4); // Boost start
          }
          
          // Absolute minimum to prevent stall-hum (approx 10% duty)
          if (pwmOutput < 800) pwmOutput = 800; 
          
          // Learn baseline (Adaptive Alpha)
          float alpha = ((float)_cvBaselineAlpha / 1000.0f); // Map 5 -> 0.005
          if (_avgCurrent > 10) { 
              _baselineTable[speedIndex] = (_baselineTable[speedIndex] * (1.0f - alpha)) + (_avgCurrent * alpha);
          }
      }
  }

  // STRICT CLAMPING: Prevent wrap-around to 0 (Full Speed)
  pwmOutput = constrain(pwmOutput, 0, (int32_t)_maxPwm);
  _drive((uint16_t)pwmOutput, _currentDirection);

  // Telemetry Stream (Replaces old Heartbeat)
  streamTelemetry();

  // Load Factor reporting for Audio
  static unsigned long lastUpdate = 0;
  if (now - lastUpdate > 100) {
      lastUpdate = now;
      // Increased sensitivity: 0.1A delta now represents 100% load for audio chuffing
      SystemContext::getInstance().getState().loadFactor = constrain((_avgCurrent - _baselineTable[speedIndex]) / 0.1f, 0.0f, 1.0f);
  }
}

/**
 * _drive implements SLOW DECAY + SINE DITHER.
 * This version uses a sine function to oscillate the magnetic field
 * for the smoothest possible 'stiction' break.
 */
void MotorController::_drive(uint16_t speed, bool direction) {
  _lastPwmValue = (int16_t)speed;

  if (speed == 0) {
      // True Brake: both pins HIGH
      ledcWrite(_pwmChannel1, _maxPwm);
      ledcWrite(_pwmChannel2, _maxPwm);
      return;
  }

  // --- SINE DITHER CALCULATION ---
  // CV64 (0-255) defines the amplitude. 
  // We'll target a ~60Hz dither frequency.
  float ditherAmplitude = (float)_cvPwmDither / 4.0f; // Scale to ~64 units
  
  // Use a phase accumulator to avoid floating point precision loss over time
  static float phase = 0.0f;
  static unsigned long lastMicros = 0;
  
  unsigned long now = micros();
  // Handle wrap-around carefully (though subtraction usually handles it for uint32)
  float dt = (float)(now - lastMicros) / 1000000.0f;
  lastMicros = now;
  
  // Prevent huge jumps if thread stalls
  if (dt > 0.1f) dt = 0.0f; 

  float frequency = 60.0f; // 60Hz dither
  phase += 6.28318f * frequency * dt;
  if (phase > 6.28318f) phase -= 6.28318f;
  
  // Calculate sine offset: sin(phase)
  float sineOffset = sin(phase) * ditherAmplitude;
  
  // Apply dither to the target speed
  int32_t ditheredSpeed = (int32_t)((float)speed + sineOffset);
  
  // Constrain and invert for Slow Decay
  // CRITICAL: Ensure we never exceed _maxPwm or drop below 0
  if (ditheredSpeed > (int32_t)_maxPwm) ditheredSpeed = _maxPwm;
  if (ditheredSpeed < 0) ditheredSpeed = 0;

  uint16_t finalSpeed = (uint16_t)ditheredSpeed;
  uint16_t invertedPwm = _maxPwm - finalSpeed;

  if (direction) {
    ledcWrite(_pwmChannel2, _maxPwm); // Hold IN2 High
    ledcWrite(_pwmChannel1, invertedPwm);
  } else {
    ledcWrite(_pwmChannel1, _maxPwm); // Hold IN1 High
    ledcWrite(_pwmChannel2, invertedPwm);
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

        // Handle Baseline Reset (CV 65)
        if (_cvBaselineReset == 1) {
            Log.println("Motor: Resetting Baseline Table...");
            for (int i = 0; i < SPEED_STEPS; i++) {
                _baselineTable[i] = 0.0f;
            }
            dcc.setCV(CV::BASELINE_RESET, 0); // Reset the flag
            _cvBaselineReset = 0;
        }
        
        // 2. Frequency Handling Logic
        // Combine High and Low CVs for a 16-bit frequency value
        _cvPwmFreq = dcc.getCV(CV::PWM_FREQ);
        _cvPwmFreqH = dcc.getCV(CV::PWM_FREQ_H);
        uint32_t targetFreq = (_cvPwmFreqH << 8) | _cvPwmFreq;
        
        // Default to 25kHz for the Paragon motor if CV is empty/invalid
        if (targetFreq < 50) targetFreq = 25000; 
        
        // Only re-init hardware if the frequency has changed to avoid PWM glitches
        if (targetFreq != _pwmFreq) {
            _pwmFreq = targetFreq;
            Log.printf("Motor: Updating PWM Freq to %u Hz\n", _pwmFreq);
            
            // Re-configure the LEDC Timer
            ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
            ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
            
            // Re-attach pins to ensure the timer link is refreshed
            ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
            ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);
            
            // Re-apply current drive state to the new timer configuration
            _drive((uint16_t)constrain(_currentSpeed * 64, 0, _maxPwm), _currentDirection);
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
        int speedIndex = constrain((int)_currentSpeed, 0, 127);
        int rawAdc = analogRead(Pinout::MOTOR_CURRENT);
        
        // Output format: [NIMRS_DATA],Target,CurrentSpeed,PWM,Amps,Load,Fault,RawADC
        Log.printf("[NIMRS_DATA],%d,%.2f,%d,%.2f,%.2f,%d,%d\n",
            state.speed,
            _currentSpeed,
            _lastPwmValue, 
            _avgCurrent,
            state.loadFactor,
            digitalRead(Pinout::MOTOR_FAULT) == LOW ? 1 : 0,
            rawAdc
        );
    }
}