#include "Arduino.h"
#include "EEPROM.h"
#include "NmraDcc.h"
#include <iostream>
#include <cassert>
#include <stdarg.h>

// Mock EEPROM instance
EEPROMClass EEPROM;

// Include Logger header to implement methods
#include "../src/Logger.h"

// Implement Logger methods
size_t Logger::printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    return 0;
}
void Logger::debug(const char *format, ...) {}
size_t Logger::write(uint8_t c) { return 1; }
size_t Logger::write(const uint8_t *buffer, size_t size) { return size; }
String Logger::getLogsJSON(const String &filter) { return "[]"; }
String Logger::getLogsHTML() { return ""; }
void Logger::_addToBuffer(const String &line) {}

// Include the source file under test
// This is a common technique to test static/internal functions or avoid linking issues with mocks
#include "../src/DccController.cpp"

void test_notifyDccSpeed_basic() {
    SystemContext::getInstance().getState().speedSource = SOURCE_DCC;
    SystemContext::getInstance().getState().lastDccSpeed = 0;

    // Simulate DCC speed command: Addr 3, Speed 50, Fwd
    notifyDccSpeed(3, 0, 50, DCC_DIR_FWD, 0);

    SystemState &state = SystemContext::getInstance().getState();

    // Check if state updated
    if (state.speed != 50) {
        std::cout << "FAILED: Speed should be 50, got " << (int)state.speed << std::endl;
        assert(state.speed == 50);
    }
    if (state.direction != true) {
        std::cout << "FAILED: Direction should be true (FWD)" << std::endl;
        assert(state.direction == true);
    }
    if (state.speedSource != SOURCE_DCC) {
        std::cout << "FAILED: Source should be DCC" << std::endl;
        assert(state.speedSource == SOURCE_DCC);
    }

    std::cout << "test_notifyDccSpeed_basic passed" << std::endl;
}

void test_notifyDccSpeed_hysteresis() {
    SystemState &state = SystemContext::getInstance().getState();
    // Setup initial state: Controlled by WEB
    state.speedSource = SOURCE_WEB;
    state.speed = 100;

    // Last DCC was 50
    state.lastDccSpeed = 50;
    state.lastDccDirection = true; // FWD

    // Case 1: Small change (delta = 1) -> Should be IGNORED
    // DCC sends 51 (delta from 50 is 1)
    notifyDccSpeed(3, 0, 51, DCC_DIR_FWD, 0);

    if (state.speedSource != SOURCE_WEB) {
        std::cout << "FAILED: Should remain SOURCE_WEB for small change" << std::endl;
        assert(state.speedSource == SOURCE_WEB);
    }
    if (state.speed != 100) {
        std::cout << "FAILED: Speed should remain 100 (Web)" << std::endl;
        assert(state.speed == 100);
    }
    if (state.lastDccSpeed != 51) {
        std::cout << "FAILED: lastDccSpeed should be updated to 51" << std::endl;
        assert(state.lastDccSpeed == 51);
    }

    std::cout << "test_notifyDccSpeed_hysteresis passed" << std::endl;
}

void test_notifyDccSpeed_override() {
    SystemState &state = SystemContext::getInstance().getState();
    // Setup: Web control
    state.speedSource = SOURCE_WEB;
    state.speed = 100;
    state.lastDccSpeed = 50;
    state.lastDccDirection = true;

    // Case 2: Large change (delta > 2) -> Should TAKE CONTROL
    // DCC sends 60 (delta from 50 is 10)
    notifyDccSpeed(3, 0, 60, DCC_DIR_FWD, 0);

    if (state.speedSource != SOURCE_DCC) {
        std::cout << "FAILED: Should switch to SOURCE_DCC for large change" << std::endl;
        assert(state.speedSource == SOURCE_DCC);
    }
    if (state.speed != 60) {
        std::cout << "FAILED: Speed should be updated to 60" << std::endl;
        assert(state.speed == 60);
    }

    std::cout << "test_notifyDccSpeed_override passed" << std::endl;
}

void test_notifyDccSpeed_direction_change() {
    SystemState &state = SystemContext::getInstance().getState();
    // Setup: Web control
    state.speedSource = SOURCE_WEB;
    state.speed = 50;
    state.lastDccSpeed = 50;
    state.lastDccDirection = true; // FWD

    // Case 3: Direction change -> Should TAKE CONTROL
    // DCC sends 50 (same speed) but REV
    notifyDccSpeed(3, 0, 50, DCC_DIR_REV, 0);

    if (state.speedSource != SOURCE_DCC) {
        std::cout << "FAILED: Should switch to SOURCE_DCC for direction change" << std::endl;
        assert(state.speedSource == SOURCE_DCC);
    }
    if (state.direction != false) { // REV is false
        std::cout << "FAILED: Direction should be REV (false)" << std::endl;
        assert(state.direction == false);
    }

    std::cout << "test_notifyDccSpeed_direction_change passed" << std::endl;
}

void test_notifyDccSpeed_speed_steps() {
    SystemState &state = SystemContext::getInstance().getState();
    state.speedSource = SOURCE_DCC;

    // Speed 0 -> 0
    notifyDccSpeed(3, 0, 0, DCC_DIR_FWD, 0);
    assert(state.speed == 0);

    // Speed 1 -> 0 (E-Stop usually, mapped to 0 here based on code: if (Speed > 1) target=Speed else 0)
    notifyDccSpeed(3, 0, 1, DCC_DIR_FWD, 0);
    assert(state.speed == 0);

    // Speed 2 -> 2
    notifyDccSpeed(3, 0, 2, DCC_DIR_FWD, 0);
    assert(state.speed == 2);

    std::cout << "test_notifyDccSpeed_speed_steps passed" << std::endl;
}

int main() {
    std::cout << "Running DccController tests..." << std::endl;

    test_notifyDccSpeed_basic();
    test_notifyDccSpeed_hysteresis();
    test_notifyDccSpeed_override();
    test_notifyDccSpeed_direction_change();
    test_notifyDccSpeed_speed_steps();

    std::cout << "All DccController tests passed!" << std::endl;
    return 0;
}
