#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"

MotorController::MotorController() {}

void MotorController::setup() {
  Log.println("MotorController: Initializing (LEDC Slow Decay)...");

  pinMode(Pinout::MOTOR_IN1, OUTPUT);
  pinMode(Pinout::MOTOR_IN2, OUTPUT);
  pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);

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

  // Safety Logic
  uint8_t effectiveTarget = targetSpeed;
  if (reqDirection != _currentDirection) {
    effectiveTarget = 0;
  }

  // Momentum
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

  // Output Logic
  // Scale internal 0-255 speed to 10-bit PWM (0-1023)
  uint16_t pwmOutput = (uint16_t)_currentSpeed * 4; // Crude scale up

  if (pwmOutput > 0) {
      // Kick Start
      if (!_kickActive && _currentSpeed < 5.0f && effectiveTarget > 0) {
          _kickActive = true;
          _kickStartTime = millis();
      }

      if (_kickActive) {
          if (millis() - _kickStartTime < KICK_DURATION) {
              pwmOutput = KICK_STRENGTH * 4; // Scale 8-bit const to 10-bit
          } else {
              _kickActive = false;
          }
      }

      // Vstart Mapping (Normal Running)
      if (!_kickActive && _cvVstart > 0 && pwmOutput < 1023) {
          // Map input (4-1023) to output (Vstart*4 - 1023)
          uint16_t vStart10 = _cvVstart * 4;
          pwmOutput = map(pwmOutput, 4, 1023, vStart10, 1023);
      }
  } else {
      _kickActive = false;
  }

  _drive(pwmOutput, _currentDirection);
}

void MotorController::setGain(bool high) {
  digitalWrite(Pinout::MOTOR_GAIN_SEL, high ? HIGH : LOW);
}

// SLOW DECAY Implementation (10-bit)
// To Drive: One side HIGH (1023), Other side PWM (inverted).
// To Brake: Both HIGH.
void MotorController::_drive(uint16_t speed, bool direction) {
  if (speed == 0) {
    // Hard Brake (Slow Decay at 0 speed)
    ledcWrite(_pwmChannel1, 1023);
    ledcWrite(_pwmChannel2, 1023);
  } else {
    // Invert speed for Low-Side PWM
    // Speed 1023 -> Duty 0 (Full Drive)
    // Speed 1    -> Duty 1022 (Tiny Drive)
    uint16_t invSpeed = 1023 - speed;

    if (direction) { // Forward
      ledcWrite(_pwmChannel1, 1023); // High
      ledcWrite(_pwmChannel2, invSpeed); // Pulsing Low
    } else { // Reverse
      ledcWrite(_pwmChannel1, invSpeed); // Pulsing Low
      ledcWrite(_pwmChannel2, 1023); // High
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
    }
}