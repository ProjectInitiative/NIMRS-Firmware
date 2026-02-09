#include "MotorController.h"
#include "Logger.h"

MotorController::MotorController() {}

void MotorController::setup() {
    Log.println("MotorController: Initializing...");

    // Setup Pins
    pinMode(_in1Pin, OUTPUT);
    pinMode(_in2Pin, OUTPUT);
    pinMode(_gainPin, OUTPUT);
    
    // Initial State: Stop
    digitalWrite(_in1Pin, LOW);
    digitalWrite(_in2Pin, LOW);
    digitalWrite(_gainPin, LOW); // Default Low Gain

    // Setup LEDC (PWM) for ESP32
    // Channel 1 -> IN1
    ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
    ledcAttachPin(_in1Pin, _pwmChannel1);

    // Channel 2 -> IN2
    ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
    ledcAttachPin(_in2Pin, _pwmChannel2);

    _drive(0, true);
    Log.println("MotorController: Ready.");
}

void MotorController::loop() {
    SystemState& state = SystemContext::getInstance().getState();
    uint8_t targetSpeed = state.speed;
    bool targetDir = state.direction;

    unsigned long now = millis();
    
    uint8_t cv3 = DccController::getInstance().getDcc().getCV(3);
    uint8_t cv4 = DccController::getInstance().getDcc().getCV(4);
    
    if (cv3 == 0) cv3 = 1;
    if (cv4 == 0) cv4 = 1;

    float accelDelay = cv3 * 3.5f;
    float decelDelay = cv4 * 3.5f;

    unsigned long dt = now - _lastMomentumUpdate;
    if (dt < 2) return; 
    
    _lastMomentumUpdate = now;

    float accelStep = (float)dt / accelDelay;
    float decelStep = (float)dt / decelDelay;
    
    if (accelStep > 5.0f) accelStep = 5.0f;
    if (decelStep > 5.0f) decelStep = 5.0f;

    if ((float)targetSpeed > _currentSpeed) {
        _currentSpeed += accelStep;
        if (_currentSpeed > targetSpeed) _currentSpeed = targetSpeed;
    } 
    else if ((float)targetSpeed < _currentSpeed) {
        _currentSpeed -= decelStep;
        if (_currentSpeed < targetSpeed) _currentSpeed = targetSpeed;
    }

    _drive((uint8_t)_currentSpeed, targetDir);
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
