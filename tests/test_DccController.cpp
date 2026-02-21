#include <Arduino.h>
#include "../src/DccController.h"
#include "SystemContext.h"
#include "Logger.h"
#include "EEPROM.h"
#include "nimrs-pinout.h"
#include <cassert>
#include <iostream>
#include <vector>

// Define global mocks
unsigned long _mock_millis = 0;
MockSerial Serial;
ESPClass ESP;
EEPROMClass EEPROM;

// Logger Mock Implementation
// Only implement methods that are NOT inline in Logger.h or inherited from Print
size_t Logger::printf(const char *format, ...) { return 0; }
void Logger::debug(const char *format, ...) {}
size_t Logger::write(uint8_t c) { return 1; }
size_t Logger::write(const uint8_t *buffer, size_t size) { return size; }
String Logger::getLogsJSON(const String &filter) { return "[]"; }
String Logger::getLogsHTML() { return ""; }
void Logger::_addToBuffer(const String &line) {}

// Test Macros
#define TEST_CASE(name) void name()
#define RUN_TEST(name) \
    std::cout << "Running " << #name << "... "; \
    name(); \
    std::cout << "PASSED" << std::endl;

// --- Test Cases ---

TEST_CASE(test_setup) {
    // Reset mocks
    NmraDcc::reset();
    EEPROM.data.assign(4096, 0xFF);

    // Set CV version to trigger reset or not
    // We can just run setup and verify calls.

    DccController::getInstance().setup();

    // Verify pin was attached
    // Pinout::TRACK_LEFT_3V3 is defined in nimrs-pinout.h included by DccController.h
    assert(NmraDcc::lastPin == Pinout::TRACK_LEFT_3V3);
    assert(NmraDcc::lastPullup == 1);
    assert(NmraDcc::initCalled == true);
}

TEST_CASE(test_notifyDccSpeed) {
    SystemContext &ctx = SystemContext::getInstance();
    SystemState &state = ctx.getState();

    // Reset state
    state.speed = 0;
    state.speedSource = SOURCE_WEB; // Start in WEB mode
    state.dccAddress = 0;

    // 1. Send Speed Command (Forward, Speed 50)
    // Speed 50 > 1, so targetSpeed = 50.
    // Source is WEB, so we need significant change or same source to take over.
    // Since lastDccSpeed is 0, 50 is significant (>2).

    notifyDccSpeed(3, 0, 50, DCC_DIR_FWD, 0);

    assert(state.speed == 50);
    assert(state.direction == true); // FWD
    assert(state.speedSource == SOURCE_DCC);
    assert(state.dccAddress == 3);

    // 2. Send Same Speed
    notifyDccSpeed(3, 0, 50, DCC_DIR_FWD, 0);
    assert(state.speed == 50);

    // 3. Switch to WEB mode externally
    state.speedSource = SOURCE_WEB;
    state.speed = 0;

    // 4. Send Same DCC Speed (Refresh)
    // Should NOT take over because delta is 0 and we are in WEB mode
    notifyDccSpeed(3, 0, 50, DCC_DIR_FWD, 0);
    assert(state.speedSource == SOURCE_WEB);
    assert(state.speed == 0);

    // 5. Send Different DCC Speed
    notifyDccSpeed(3, 0, 60, DCC_DIR_FWD, 0);
    assert(state.speedSource == SOURCE_DCC);
    assert(state.speed == 60);
}

TEST_CASE(test_notifyDccFunc) {
    SystemContext &ctx = SystemContext::getInstance();
    SystemState &state = ctx.getState();

    // Reset functions
    for(int i=0; i<29; i++) state.functions[i] = false;

    // F0-F4 (Group 1)
    // Bit 4 (0x10) is F0 (Light)
    // Bit 0 (0x01) is F1
    notifyDccFunc(3, 0, FN_0_4, FN_BIT_00 | FN_BIT_01);

    assert(state.functions[0] == true); // F0
    assert(state.functions[1] == true); // F1
    assert(state.functions[2] == false);

    // F5-F8 (Group 2)
    // Bit 0 is F5
    notifyDccFunc(3, 0, FN_5_8, 0x01);
    assert(state.functions[5] == true);
    assert(state.functions[6] == false);
}

TEST_CASE(test_isPacketValid) {
    SystemContext &ctx = SystemContext::getInstance();

    // Set current time
    _mock_millis = 5000;

    // Case 1: No packet received yet
    ctx.getState().lastDccPacketTime = 0;
    assert(DccController::getInstance().isPacketValid() == false);

    // Case 2: Packet received recently
    ctx.getState().lastDccPacketTime = 4000; // 1 second ago
    assert(DccController::getInstance().isPacketValid() == true);

    // Case 3: Packet too old
    ctx.getState().lastDccPacketTime = 2000; // 3 seconds ago (>2000ms)
    assert(DccController::getInstance().isPacketValid() == false);
}

TEST_CASE(test_cv_write) {
    // Reset EEPROM
    EEPROM.data.assign(4096, 0xFF);

    // Write CV 1 = 10
    uint8_t ret = notifyCVWrite(1, 10);

    assert(ret == 10);
    assert(EEPROM.read(1) == 10);
}

int main() {
    RUN_TEST(test_setup);
    RUN_TEST(test_notifyDccSpeed);
    RUN_TEST(test_notifyDccFunc);
    RUN_TEST(test_isPacketValid);
    RUN_TEST(test_cv_write);

    std::cout << "All DccController tests passed!" << std::endl;
    return 0;
}
