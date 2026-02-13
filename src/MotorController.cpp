#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"

MotorController::MotorController() {}

void MotorController::setup() {
  Log.println("MotorController: Initializing (LEDC Slow Decay)...");

  pinMode(Pinout::MOTOR_IN1, OUTPUT);
  pinMode(Pinout::MOTOR_IN2, OUTPUT);
  pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);

  // Initialize ADC for Current Sensing
  // Pin 5 is ADC1_CH4 on S3? Need to check mapping if analogRead fails.
  // Standard Arduino analogRead should work.
  pinMode(Pinout::MOTOR_CURRENT, INPUT);

  digitalWrite(Pinout::MOTOR_IN1, LOW);
  digitalWrite(Pinout::MOTOR_IN2, LOW);
  digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW);

  ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);

  ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
  ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

  _drive(0, true);
  Log.println("MotorController: Ready.");
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

  // Optimize CV reads
  _updateCvCache();

  // 1. Current Sensing & Filtering
  int rawCurrent = analogRead(Pinout::MOTOR_CURRENT);
  // Simple Low Pass Filter (Alpha = 0.1)
  _avgCurrent = (_avgCurrent * 0.9f) + (rawCurrent * 0.1f);

  // 2. Safety Logic
  uint8_t effectiveTarget = targetSpeed;
  if (reqDirection != _currentDirection) {
    effectiveTarget = 0;
  }

  // 3. Momentum
  unsigned long now = millis();
  
  // Use Defaults if CVs are 0
  uint8_t accel = (_cvAccel == 0) ? 1 : _cvAccel;
  uint8_t decel = (_cvDecel == 0) ? 1 : _cvDecel;

  float accelDelay = accel * 3.5f;
  float decelDelay = decel * 3.5f;

  unsigned long dt = now - _lastMomentumUpdate;
  if (dt >= 2) {
      _lastMomentumUpdate = now;
      float accelStep = (float)dt / accelDelay;
      float decelStep = (float)dt / decelDelay;
      if (accelStep > 5.0f) accelStep = 5.0f;
      if (decelStep > 5.0f) decelStep = 5.0f;

      if ((float)effectiveTarget > _currentSpeed) {
        _currentSpeed += accelStep;
        if (_currentSpeed > effectiveTarget) _currentSpeed = effectiveTarget;
      } else if ((float)effectiveTarget < _currentSpeed) {
        _currentSpeed -= decelStep;
        if (_currentSpeed < effectiveTarget) _currentSpeed = effectiveTarget;
      }
  }

  // Direction Flip
  if (_currentSpeed < 1.0f && reqDirection != _currentDirection) {
    _currentDirection = reqDirection;
  }

  // 4. Output Logic (10-bit)
  // Scale internal 0-255 speed to 10-bit PWM (0-1023)
  int32_t pwmOutput = (int32_t)_currentSpeed * 4; 

  if (pwmOutput > 0) {
      // A. Kick Start
      if (!_kickActive && _currentSpeed < 5.0f && effectiveTarget > 0) {
          _kickActive = true;
          _kickStartTime = millis();
      }

      if (_kickActive) {
          if (millis() - _kickStartTime < KICK_DURATION) {
              pwmOutput = KICK_STRENGTH * 4;
          } else {
              _kickActive = false;
          }
      }

      // B. Load Compensation (PI Loop)
      // Only apply if we are running normally (not kicking)
      if (!_kickActive && (_cvLoadK > 0 || _cvLoadI > 0)) {
          // Error = Current (Load) - Baseline? 
          // Actually simpler: Just boost proportional to Current.
          // More current = More load = More Voltage needed.
          
          float pTerm = _avgCurrent * (_cvLoadK / 10.0f); // Scale K down
          
          // Integral Term: Accumulate load over time (Anti-Stall)
          // Limit integral windup
          _loadIntegral += (_avgCurrent * (_cvLoadI / 100.0f));
          if (_loadIntegral > 200.0f) _loadIntegral = 200.0f;
          if (_loadIntegral < 0.0f) _loadIntegral = 0.0f;
          
          // Reset Integral if current drops (stiction broken)
          // or if we stop.
          if (_avgCurrent < 50) _loadIntegral = 0; // Tune threshold?

          pwmOutput += (int32_t)(pTerm + _loadIntegral);
      }

      // C. Vstart Mapping
      // Apply Vstart AFTER compensation so boost is added on top?
      // Or before? Standard is: Output = Map(Speed) + PID.
      
      if (!_kickActive && _cvVstart > 0 && pwmOutput < 1023) {
          uint16_t vStart10 = _cvVstart * 4;
          // Ensure we don't double-count pwmOutput.
          // Standard map: range [0..1023] -> [Vstart..1023]
          // We map the *Base Speed* then add Boost.
          int32_t basePwm = (int32_t)_currentSpeed * 4;
          basePwm = map(basePwm, 4, 1023, vStart10, 1023);
          
          // Re-add Boost
          pwmOutput = basePwm + (pwmOutput - ((int32_t)_currentSpeed*4));
      }
      
      // Clamp
      if (pwmOutput > 1023) pwmOutput = 1023;
      if (pwmOutput < 0) pwmOutput = 0;

  } else {
      _kickActive = false;
      _loadIntegral = 0;
  }

  _drive((uint16_t)pwmOutput, _currentDirection);
  
  // Debug Log (Periodic)
  static unsigned long lastLog = 0;
  if (millis() - lastLog > 500 && pwmOutput > 0) {
      lastLog = millis();
      Log.printf("Motor: Spd %d | PWM %d | I: %.1f | Boost: %d\n", 
        (int)_currentSpeed, pwmOutput, _avgCurrent, (int)(pwmOutput - ((int)_currentSpeed*4)));
  }
}

void MotorController::setGain(bool high) {
  digitalWrite(Pinout::MOTOR_GAIN_SEL, high ? HIGH : LOW);
}

// SLOW DECAY Implementation (10-bit)
void MotorController::_drive(uint16_t speed, bool direction) {
  if (speed == 0) {
    ledcWrite(_pwmChannel1, 1023);
    ledcWrite(_pwmChannel2, 1023);
  } else {
    uint16_t invSpeed = 1023 - speed;
    if (direction) {
      ledcWrite(_pwmChannel1, 1023);
      ledcWrite(_pwmChannel2, invSpeed);
    } else {
      ledcWrite(_pwmChannel1, invSpeed);
      ledcWrite(_pwmChannel2, 1023);
    }
  }
}

void MotorController::_updateCvCache() {
    if (millis() - _lastCvUpdate > 100) {
        _lastCvUpdate = millis();
        NmraDcc &dcc = DccController::getInstance().getDcc();
        _cvAccel = dcc.getCV(CV::ACCEL);
        _cvDecel = dcc.getCV(CV::DECEL);
        _cvVstart = dcc.getCV(CV::V_START);
        _cvLoadK = dcc.getCV(CV::LOAD_K);
        _cvLoadI = dcc.getCV(CV::LOAD_I);
    }
}
