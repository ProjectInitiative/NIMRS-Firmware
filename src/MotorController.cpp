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

    // Output Logic (10-bit)

    int32_t pwmOutput = 0;

  

    // --- MODE SELECTION ---

    if (_cvPidP > 0) {

        // 1. Observe

        float targetOmega = (float)effectiveTarget / 128.0f;

        float vApplied = _modelPwm / 1023.0f;

        float iMeasured = _avgCurrent / 4095.0f; 

        float R = _cvMotorR / 25.0f; 

        float Ke = (_cvMotorKe > 0) ? (_cvMotorKe / 100.0f) : 1.0f;

  

        float backEmf = vApplied - (iMeasured * R);

        _modelOmega = (backEmf < 0) ? 0 : (backEmf / Ke);

        

        // 2. Control (PI)

        float error = targetOmega - _modelOmega;

        float pOut = error * (_cvPidP * 4.0f);

        _pidErrorSum += (error * (_cvPidI / 100.0f));

        if (_pidErrorSum > 1023.0f) _pidErrorSum = 1023.0f;

        if (_pidErrorSum < -500.0f) _pidErrorSum = -500.0f;

        

        float ff = targetOmega * Ke * 1023.0f;

        _modelPwm = ff + pOut + _pidErrorSum;

        if (_modelPwm > 1023.0f) _modelPwm = 1023.0f;

        if (_modelPwm < 0.0f) _modelPwm = 0.0f;

        pwmOutput = (int32_t)_modelPwm;

        

        if (effectiveTarget == 0) {

            _pidErrorSum = 0; _modelPwm = 0; pwmOutput = 0;

        }

        

        static unsigned long lastModelLog = 0;

        if (millis() - lastModelLog > 200 && effectiveTarget > 0) {

            lastModelLog = millis();

            Log.printf("Model: Tgt %.2f | Est %.2f | PWM %d | I %.3f\n", targetOmega, _modelOmega, pwmOutput, iMeasured);

        }

    } else {

        // --- OPEN LOOP (Fallback) ---

        pwmOutput = (int32_t)_currentSpeed * 4; 

        if (pwmOutput > 0) {

            if (!_kickActive && _currentSpeed < 5.0f && effectiveTarget > 0) {

                _kickActive = true; _kickStartTime = millis();

            }

                      if (_kickActive) {

                          if (millis() - _kickStartTime < KICK_DURATION) {

                              pwmOutput = KICK_STRENGTH * 4;

                          } else {

                              _kickActive = false;

                          }

                      }

            

                      // B. Load Compensation (Removed in favor of Model-Based)

                      

                      // C. Vstart Mapping

                      if (!_kickActive && _cvVstart > 0 && pwmOutput < 1023) {

                          uint16_t vStart10 = _cvVstart * 4;

                int32_t basePwm = (int32_t)_currentSpeed * 4;

                basePwm = map(basePwm, 4, 1023, vStart10, 1023);

                pwmOutput = basePwm + (pwmOutput - ((int32_t)_currentSpeed * 4));

            }

            if (pwmOutput > 1023) pwmOutput = 1023;

            if (pwmOutput < 0) pwmOutput = 0;

        } else {

            _kickActive = false; _loadIntegral = 0;

        }

    }

  

    _drive((uint16_t)pwmOutput, _currentDirection);

    

    static unsigned long lastLog = 0;

    if (_cvPidP == 0 && millis() - lastLog > 500 && pwmOutput > 0) {

        lastLog = millis();

        Log.printf("Motor: Spd %d | PWM %d | I: %.1f | Boost: %d\n", (int)_currentSpeed, pwmOutput, _avgCurrent, (int)(pwmOutput - ((int)_currentSpeed*4)));

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
        _cvChuffRate = dcc.getCV(CV::CHUFF_RATE);
        _cvChuffDrag = dcc.getCV(CV::CHUFF_DRAG);
        _cvMotorR = dcc.getCV(CV::MOTOR_R);
    }
}
