#include <cassert>
#include <iostream>
#include <map>
#include <vector>

// Mock definitions
#include "Arduino.h"
#include "DccController.h"
#include "SystemContext.h"
#include "CvRegistry.h"
#include "nimrs-pinout.h"

// Class under test
#include "../src/LightingController.h"

// Test framework macros
#define TEST_CASE(name) void name()
#define RUN_TEST(name) \
    std::cout << "Running " << #name << "... "; \
    setupTest(); \
    name(); \
    std::cout << "PASSED" << std::endl;

void setupTest() {
    // Reset mocks
    pinStates.clear();
    pinModes.clear();
    DccController::getInstance().getDcc().reset();

    // Reset System Context
    SystemState& state = SystemContext::getInstance().getState();
    state.direction = true; // Forward
    for(int i=0; i<29; i++) state.functions[i] = false;
}

TEST_CASE(test_setup_initializes_pins) {
    LightingController lc;
    lc.setup();

    // Check Pin Modes
    assert(pinModes[Pinout::LIGHT_FRONT] == OUTPUT);
    assert(pinModes[Pinout::LIGHT_REAR] == OUTPUT);
    assert(pinModes[Pinout::AUX1] == OUTPUT);

    // Check Initial State (OFF = LOW usually)
    assert(pinStates[Pinout::LIGHT_FRONT] == LOW);
    assert(pinStates[Pinout::LIGHT_REAR] == LOW);
}

TEST_CASE(test_headlight_directional) {
    LightingController lc;
    lc.setup();

    // Configure CVs (Default mapping)
    // CV::FRONT -> 0 (F0)
    // CV::REAR -> 0 (F0)
    DccController::getInstance().getDcc().setCV(CV::FRONT, 0);
    DccController::getInstance().getDcc().setCV(CV::REAR, 0);

    SystemState& state = SystemContext::getInstance().getState();

    // F0 OFF
    state.functions[0] = false;
    lc.loop();
    assert(pinStates[Pinout::LIGHT_FRONT] == LOW);
    assert(pinStates[Pinout::LIGHT_REAR] == LOW);

    // F0 ON, Forward
    state.functions[0] = true;
    state.direction = true;
    lc.loop();
    assert(pinStates[Pinout::LIGHT_FRONT] == HIGH);
    assert(pinStates[Pinout::LIGHT_REAR] == LOW);

    // F0 ON, Reverse
    state.direction = false;
    lc.loop();
    assert(pinStates[Pinout::LIGHT_FRONT] == LOW);
    assert(pinStates[Pinout::LIGHT_REAR] == HIGH);
}

TEST_CASE(test_aux_functions) {
    LightingController lc;
    lc.setup();

    // Map AUX1 to F1
    DccController::getInstance().getDcc().setCV(CV::AUX1, 1);

    SystemState& state = SystemContext::getInstance().getState();

    // F1 OFF
    state.functions[1] = false;
    lc.loop();
    assert(pinStates[Pinout::AUX1] == LOW);

    // F1 ON
    state.functions[1] = true;

    // Verify state is set
    assert(state.functions[1] == true);

    lc.loop();

    assert(pinStates[Pinout::AUX1] == HIGH);
}

TEST_CASE(test_complex_mapping) {
    LightingController lc;
    lc.setup();

    // Map AUX1 to F0 (Headlight function)
    DccController::getInstance().getDcc().setCV(CV::AUX1, 0);

    SystemState& state = SystemContext::getInstance().getState();
    state.functions[0] = true;
    state.direction = true;

    lc.loop();
    assert(pinStates[Pinout::AUX1] == HIGH);

    // Reverse direction
    state.direction = false;
    lc.loop();
    assert(pinStates[Pinout::AUX1] == HIGH); // Should still be ON because it's non-directional for AUX1
}

int main() {
    RUN_TEST(test_setup_initializes_pins);
    RUN_TEST(test_headlight_directional);
    RUN_TEST(test_aux_functions);
    RUN_TEST(test_complex_mapping);
    return 0;
}
