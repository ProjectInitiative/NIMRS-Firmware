// TEST_FLAGS: -DSKIP_MOCK_DCC_CONTROLLER
// TEST_SOURCES: src/DccController.cpp tests/mocks/mocks.cpp
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Mock everything before including the class under test
#define private public
#include "../src/DccController.h"
#undef private

#include "SystemContext.h"
#include "NmraDcc.h"
#include "EEPROM.h"
#include "Arduino.h" // for _mockMillis

// Simple test framework
#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

// Helper to reset state
void resetState() {
    SystemContext::getInstance().getState() = SystemState();
    DccController::getInstance().getDcc().resetMock();
    EEPROM.reset();
    _mockMillis = 1000;
}

TEST_CASE(test_getInstance) {
    resetState();
    DccController& dcc = DccController::getInstance();
    // Address of getInstance() should be consistent
    DccController& dcc2 = DccController::getInstance();
    assert(&dcc == &dcc2);
}

TEST_CASE(test_setup) {
    resetState();
    DccController& dcc = DccController::getInstance();
    dcc.setup();

    // Verify NmraDcc calls
    NmraDcc& mockDcc = dcc.getDcc();
    assert(mockDcc.pinCalledCount == 1);
    assert(mockDcc.pinArg == 1); // Pinout::TRACK_LEFT_3V3 is 1
    assert(mockDcc.initCalledCount == 1);

    // Verify EEPROM init (mock sets size)
    assert(EEPROM._size == 512);
}

TEST_CASE(test_loop) {
    resetState();
    DccController& dcc = DccController::getInstance();
    dcc.loop();

    NmraDcc& mockDcc = dcc.getDcc();
    assert(mockDcc.processCalledCount == 1);
}

TEST_CASE(test_updateSpeed) {
    resetState();
    DccController& dcc = DccController::getInstance();

    dcc.updateSpeed(50, true);

    SystemState& state = SystemContext::getInstance().getState();
    assert(state.speed == 50);
    assert(state.direction == true);

    dcc.updateSpeed(0, false);
    assert(state.speed == 0);
    assert(state.direction == false);
}

TEST_CASE(test_updateFunction) {
    resetState();
    DccController& dcc = DccController::getInstance();

    dcc.updateFunction(0, true);
    SystemState& state = SystemContext::getInstance().getState();
    assert(state.functions[0] == true);

    dcc.updateFunction(0, false);
    assert(state.functions[0] == false);

    dcc.updateFunction(5, true);
    assert(state.functions[5] == true);
}

TEST_CASE(test_notifyDccSpeed) {
    resetState();

    // Simulate DCC speed packet
    notifyDccSpeed(1234, 0, 60, DCC_DIR_FWD, 0);

    SystemState& state = SystemContext::getInstance().getState();
    assert(state.speed == 60);
    assert(state.direction == true); // FWD is true
    assert(state.speedSource == SOURCE_DCC);
    assert(state.dccAddress == 1234);

    // Simulate stop
    notifyDccSpeed(1234, 0, 0, DCC_DIR_FWD, 0);
    assert(state.speed == 0);
}

TEST_CASE(test_notifyDccFunc) {
    resetState();
    SystemState& state = SystemContext::getInstance().getState();

    // Group 0-4
    // F0 is bit 4 (0x10) -> FN_BIT_00
    // F1 is bit 0 (0x01) -> FN_BIT_01
    // ...
    // Set F0 and F1 ON
    notifyDccFunc(3, 0, FN_0_4, FN_BIT_00 | FN_BIT_01);
    assert(state.functions[0] == true); // F0
    assert(state.functions[1] == true); // F1
    assert(state.functions[2] == false);

    // Set F0 OFF, F2 ON (bit 1 -> 0x02)
    notifyDccFunc(3, 0, FN_0_4, FN_BIT_02);
    assert(state.functions[0] == false);
    assert(state.functions[1] == false);
    assert(state.functions[2] == true);

    // Group 5-8 (FN_5_8)
    // baseIndex = 5
    // bit 0 -> F5, bit 1 -> F6, ...
    notifyDccFunc(3, 0, FN_5_8, 0x01 | 0x04); // F5 (bit 0) and F7 (bit 2)
    assert(state.functions[5] == true);
    assert(state.functions[6] == false);
    assert(state.functions[7] == true);
}

TEST_CASE(test_isPacketValid) {
    resetState();
    DccController& dcc = DccController::getInstance();

    // Initially invalid (lastPacketTime = 0)
    assert(dcc.isPacketValid() == false);

    // Send a packet
    notifyDccSpeed(3, 0, 10, DCC_DIR_FWD, 0);

    // Should be valid now
    assert(dcc.isPacketValid() == true);

    // Advance time by 1000ms
    _mockMillis += 1000;
    assert(dcc.isPacketValid() == true);

    // Advance time by another 1001ms (total 2001ms diff)
    _mockMillis += 1001;
    assert(dcc.isPacketValid() == false);
}

TEST_CASE(test_notifyCVWrite) {
    resetState();
    notifyCVWrite(1, 10);

    // Verify EEPROM write
    // EEPROM mock just writes to map
    assert(EEPROM.read(1) == 10);

    // Verify it returns the value
    assert(notifyCVWrite(2, 20) == 20);
    assert(EEPROM.read(2) == 20);
}

int main() {
    RUN_TEST(test_getInstance);
    RUN_TEST(test_setup);
    RUN_TEST(test_loop);
    RUN_TEST(test_updateSpeed);
    RUN_TEST(test_updateFunction);
    RUN_TEST(test_notifyDccSpeed);
    RUN_TEST(test_notifyDccFunc);
    RUN_TEST(test_isPacketValid);
    RUN_TEST(test_notifyCVWrite);

    std::cout << "All tests passed!" << std::endl;
    return 0;
}
