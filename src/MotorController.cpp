#include "MotorController.h"

MotorController::MotorController() {}

void MotorController::setup() {
    Serial.println("MotorController: Initializing...");

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
    Serial.println("MotorController: Ready.");
}

void MotorController::loop() {
    SystemState& state = SystemContext::getInstance().getState();
    
    // In a future version, we would implement momentum here
    // For now, we drive directly
    _drive(state.speed, state.direction);
}

void MotorController::setGain(bool high) {
    digitalWrite(_gainPin, high ? HIGH : LOW);
}

void MotorController::_drive(uint8_t speed, bool direction) {
    // DRV8213 Logic (PWM Mode):
    // FWD: IN1 = PWM, IN2 = LOW
    // REV: IN1 = LOW, IN2 = PWM
    // COAST: IN1 = LOW, IN2 = LOW
    
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
