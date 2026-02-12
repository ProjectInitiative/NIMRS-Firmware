#include "MotorController.h"
#include "Logger.h"
#include "CvRegistry.h"

MotorController::MotorController() {}

void MotorController::setup() {
    Log.println("MotorController: Initializing...");

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
        SystemContext& ctx = SystemContext::getInstance();
        ScopedLock lock(ctx);
        SystemState& state = ctx.getState();
        targetSpeed = state.speed;
        reqDirection = state.direction;
    }

    // Safety Logic:
    // If requested direction differs from current physical direction,
    // we MUST stop first.
    uint8_t effectiveTarget = targetSpeed;
    
    if (reqDirection != _currentDirection) {
        // We are moving wrong way (or stopped but facing wrong way).
        // Force target to 0 to decelerate.
        effectiveTarget = 0;
    }

    // Momentum Calculations
    unsigned long now = millis();
    
    // Read CVs using descriptive names
    uint8_t accelRate = DccController::getInstance().getDcc().getCV(CV::ACCEL);
    uint8_t decelRate = DccController::getInstance().getDcc().getCV(CV::DECEL);
    uint8_t vStart    = DccController::getInstance().getDcc().getCV(CV::V_START);
    
    if (accelRate == 0) accelRate = 1;
    if (decelRate == 0) decelRate = 1;

    // Calculate Step Delay
    float accelDelay = accelRate * 3.5f;
    float decelDelay = decelRate * 3.5f;

    unsigned long dt = now - _lastMomentumUpdate;
    if (dt < 2) return; 
    
    _lastMomentumUpdate = now;

    float accelStep = (float)dt / accelDelay;
    float decelStep = (float)dt / decelDelay;
    
    if (accelStep > 5.0f) accelStep = 5.0f;
    if (decelStep > 5.0f) decelStep = 5.0f;

    // Apply Momentum
    if ((float)effectiveTarget > _currentSpeed) {
        _currentSpeed += accelStep;
        if (_currentSpeed > effectiveTarget) _currentSpeed = effectiveTarget;
    } 
    else if ((float)effectiveTarget < _currentSpeed) {
        _currentSpeed -= decelStep;
        if (_currentSpeed < effectiveTarget) _currentSpeed = effectiveTarget;
    }

    // Direction Flipping Logic
    // Only flip direction if we are effectively stopped
    if (_currentSpeed < 1.0f && reqDirection != _currentDirection) {
        _currentDirection = reqDirection;
        // The momentum logic will naturally ramp up from 0 in the next loops
    }

    // Vstart Application
    // If we are supposed to be moving (currentSpeed > 0), ensure we output at least Vstart
    uint8_t pwmOutput = (uint8_t)_currentSpeed;
    
    if (pwmOutput > 0 && pwmOutput < 255) {
        // Map the remaining range? Or just add offset?
        // Standard behavior: 1 = Vstart, 255 = Vhigh.
        // Simple map:
        if (vStart > 0) {
            pwmOutput = map(pwmOutput, 1, 255, vStart, 255);
        }
    }

    _drive(pwmOutput, _currentDirection);
}

void MotorController::setGain(bool high) {
    digitalWrite(Pinout::MOTOR_GAIN_SEL, high ? HIGH : LOW);
}

void MotorController::_drive(uint8_t speed, bool direction) {
    if (speed == 0) {
        ledcWrite(_pwmChannel1, 0);
        ledcWrite(_pwmChannel2, 0);
    } else {
        if (direction) { // Forward
            ledcWrite(_pwmChannel1, speed);
            ledcWrite(_pwmChannel2, 0);
        } else { // Reverse
            ledcWrite(_pwmChannel1, 0);
            ledcWrite(_pwmChannel2, speed);
        }
    }
}