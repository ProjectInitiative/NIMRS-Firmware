#include "LightingController.h"

LightingController::LightingController() {}

void LightingController::setup() {
    Serial.println("LightingController: Initializing...");

    pinMode(_frontPin, OUTPUT);
    pinMode(_rearPin, OUTPUT);
    pinMode(_aux1Pin, OUTPUT);
    pinMode(_aux2Pin, OUTPUT);
    
    pinMode(_in1Pin, INPUT_PULLUP);
    pinMode(_in2Pin, INPUT_PULLUP);

    digitalWrite(_frontPin, LOW);
    digitalWrite(_rearPin, LOW);
    digitalWrite(_aux1Pin, LOW);
    digitalWrite(_aux2Pin, LOW);

    Serial.println("LightingController: Ready.");
}

void LightingController::loop() {
    SystemState& state = SystemContext::getInstance().getState();

    // Standard NMRA Mapping:
    // F0: Headlight. Often directional (F0F vs F0R).
    if (state.functions[0]) {
        if (state.direction) {
            digitalWrite(_frontPin, HIGH);
            digitalWrite(_rearPin, LOW);
        } else {
            digitalWrite(_frontPin, LOW);
            digitalWrite(_rearPin, HIGH);
        }
    } else {
        digitalWrite(_frontPin, LOW);
        digitalWrite(_rearPin, LOW);
    }

    // F1: AUX1
    digitalWrite(_aux1Pin, state.functions[1] ? HIGH : LOW);

    // F2: AUX2
    digitalWrite(_aux2Pin, state.functions[2] ? HIGH : LOW);
}
