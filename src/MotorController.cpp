#include "MotorController.h"
#include "Logger.h"

MotorController::MotorController() {}

void MotorController::setup() {
    Log.println("MotorController: Initializing...");

    pinMode(_in1Pin, OUTPUT);
    pinMode(_in2Pin, OUTPUT);
    pinMode(_gainPin, OUTPUT);
    
    digitalWrite(_in1Pin, LOW);
    digitalWrite(_in2Pin, LOW);
    digitalWrite(_gainPin, LOW);

    ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
    ledcAttachPin(_in1Pin, _pwmChannel1);

    ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
    ledcAttachPin(_in2Pin, _pwmChannel2);

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
    
    // Read CVs (3=Accel, 4=Decel)
    uint8_t cv3 = DccController::getInstance().getDcc().getCV(3);
    uint8_t cv4 = DccController::getInstance().getDcc().getCV(4);
    uint8_t cv2 = DccController::getInstance().getDcc().getCV(2); // Vstart
    
    if (cv3 == 0) cv3 = 1;
    if (cv4 == 0) cv4 = 1;

    // Calculate Step Delay
    float accelDelay = cv3 * 3.5f;
    float decelDelay = cv4 * 3.5f;

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

    // CV2 (Vstart) Application
    // If we are supposed to be moving (currentSpeed > 0), ensure we output at least Vstart
    uint8_t pwmOutput = (uint8_t)_currentSpeed;
    
    if (pwmOutput > 0 && pwmOutput < 255) {
        // Map the remaining range? Or just add offset?
        // Standard behavior: 1 = Vstart, 255 = Vhigh.
        // Simple map:
        if (cv2 > 0) {
            pwmOutput = map(pwmOutput, 1, 255, cv2, 255);
        }
    }

    _drive(pwmOutput, _currentDirection);
}

void MotorController::setGain(bool high) {
    digitalWrite(_gainPin, high ? HIGH : LOW);
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