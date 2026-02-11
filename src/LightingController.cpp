#include "LightingController.h"
#include "DccController.h"
#include "Logger.h"

LightingController::LightingController() {}

void LightingController::setup() {
    Log.println("OutputController: Initializing...");

    pinMode(_frontPin, OUTPUT);
    pinMode(_rearPin, OUTPUT);
    pinMode(_aux1Pin, OUTPUT);
    pinMode(_aux2Pin, OUTPUT);
    pinMode(_aux3Pin, OUTPUT);
    pinMode(_aux4Pin, OUTPUT);
    pinMode(_aux5Pin, OUTPUT);
    pinMode(_aux6Pin, OUTPUT);
    
    // Setup Inputs
    pinMode(_in1Pin, INPUT);
    pinMode(_in2Pin, INPUT);

    // All OFF initially
    digitalWrite(_frontPin, LOW);
    digitalWrite(_rearPin, LOW);
    digitalWrite(_aux1Pin, LOW);
    digitalWrite(_aux2Pin, LOW);
    digitalWrite(_aux3Pin, LOW);
    digitalWrite(_aux4Pin, LOW);
    digitalWrite(_aux5Pin, LOW);
    digitalWrite(_aux6Pin, LOW);

    Log.println("OutputController: Ready.");
}

void LightingController::loop() {
    bool functions[29];
    bool direction;

    // Thread-safe state capture
    {
        SystemContext& ctx = SystemContext::getInstance();
        ScopedLock lock(ctx);
        memcpy(functions, ctx.getState().functions, 29);
        direction = ctx.getState().direction;
    }

    NmraDcc& dcc = DccController::getInstance().getDcc();

    // Read mapping CVs
    uint8_t f0f_map = dcc.getCV(33); // Front Light
    uint8_t f0r_map = dcc.getCV(34); // Rear Light
    uint8_t a1_map  = dcc.getCV(35); // AUX 1
    uint8_t a2_map  = dcc.getCV(36); // AUX 2
    uint8_t a3_map  = dcc.getCV(37); // AUX 3
    uint8_t a4_map  = dcc.getCV(38); // AUX 4
    uint8_t a5_map  = dcc.getCV(39); // AUX 5
    uint8_t a6_map  = dcc.getCV(40); // AUX 6

    // F0 Directional Logic:
    // If a pin is mapped to F0, it only turns on if F0 is active AND direction matches.
    // If it's mapped to any other F, it turns on if that F is active.

    auto driveOutput = [&](uint8_t pin, uint8_t map, bool isFront, bool isRear) {
        bool active = false;
        if (map < 29) {
            if (map == 0) { // F0 logic
                if (functions[0]) {
                    if (isFront) active = direction;
                    else if (isRear) active = !direction;
                    else active = true; // Generic pin mapped to F0
                }
            } else {
                active = functions[map];
            }
        }
        digitalWrite(pin, active ? HIGH : LOW);
    };

    driveOutput(_frontPin, f0f_map, true, false);
    driveOutput(_rearPin, f0r_map, false, true);
    driveOutput(_aux1Pin, a1_map, false, false);
    driveOutput(_aux2Pin, a2_map, false, false);
    driveOutput(_aux3Pin, a3_map, false, false);
    driveOutput(_aux4Pin, a4_map, false, false);
    driveOutput(_aux5Pin, a5_map, false, false);
    driveOutput(_aux6Pin, a6_map, false, false);
}
