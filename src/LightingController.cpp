#include "LightingController.h"
#include "Logger.h"

LightingController::LightingController() {}

void LightingController::setup() {
    Log.println("LightingController: Initializing...");

    // Setup Outputs (Transistor Driven)
    pinMode(_frontPin, OUTPUT); // GPIO 12 (FRONT) - Driver Q2B for 21-Pin Pin 8
    pinMode(_rearPin, OUTPUT);  // GPIO 10 (REAR)  - Driver Q2A for 21-Pin Pin 7
    pinMode(_aux1Pin, OUTPUT);  // GPIO 8 (AUX1)   - Driver Q1A for 21-Pin Pin 15
    pinMode(_aux2Pin, OUTPUT);  // GPIO 9 (AUX2)   - Driver Q1B for 21-Pin Pin 14
    
    // Logic inputs
    pinMode(_in1Pin, INPUT_PULLUP); // GPIO 7 (INPUT1/AUX7) - 21-Pin Pin 1
    pinMode(_in2Pin, INPUT_PULLUP); // GPIO 6 (INPUT2/AUX8) - 21-Pin Pin 2

    // Initial State: All OFF
    digitalWrite(_frontPin, LOW);
    digitalWrite(_rearPin, LOW);
    digitalWrite(_aux1Pin, LOW);
    digitalWrite(_aux2Pin, LOW);

    Log.println("LightingController: Ready.");
}

void LightingController::loop() {
    SystemState& state = SystemContext::getInstance().getState();

    // NMRA F0: Headlight. Directional logic.
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

    // F1: AUX1 (Pin 15)
    digitalWrite(_aux1Pin, state.functions[1] ? HIGH : LOW);

    // F2: AUX2 (Pin 14)
    digitalWrite(_aux2Pin, state.functions[2] ? HIGH : LOW);
}