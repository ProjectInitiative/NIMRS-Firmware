#include <cassert>
#include <iostream>
#include <math.h>
#include <map>
#include <functional>

// Allow access to private members for testing
#define private public
#include "../src/MotorController.h"
#include "../src/SystemContext.h"
#include "../src/CvRegistry.h"
#include "../src/nimrs-pinout.h"
#include "mocks/Arduino.h"
#include "mocks/NmraDcc.h"

// Forward declaration of globals from mocks_motor.cpp
extern unsigned long _mockMillis;
extern std::map<int, int> _mockCVs;
extern std::map<int, int> _mockLedcValues;
extern std::function<int(uint8_t)> _mockAnalogRead;

void reset_mocks() {
    _mockMillis = 0;
    _mockCVs.clear();
    _mockLedcValues.clear();
    _mockAnalogRead = nullptr;

    // Reset SystemContext
    SystemState& state = SystemContext::getInstance().getState();
    state.speed = 0;
    state.direction = true;
    state.dccAddress = 3;
}

void reset_motor_controller(MotorController& mc) {
    mc._currentSpeed = 0.0f;
    mc._lastMomentumUpdate = 0;
    mc._piErrorSum = 0.0f;
    mc._kickStartTimer = 0;
    mc._isMoving = false;
    mc._avgCurrent = 0.0f;
    mc._lastCvUpdate = 0;
    mc._testMode = false;
    mc._lastPwmValue = 0;
}

void test_initialization() {
    std::cout << "Running test_initialization..." << std::endl;
    reset_mocks();
    MotorController& mc = MotorController::getInstance();
    reset_motor_controller(mc);

    // Mock analogRead for current offset calibration
    _mockAnalogRead = [](uint8_t pin) { return 100; };

    mc.setup();

    // Verify current offset is calibrated
    // 100 readings of 100 should average to 100.0
    assert(fabs(mc._currentOffset - 100.0f) < 0.1f);

    std::cout << "test_initialization passed." << std::endl;
}

void test_momentum() {
    std::cout << "Running test_momentum..." << std::endl;
    reset_mocks();
    MotorController& mc = MotorController::getInstance();
    reset_motor_controller(mc);

    // Set CV3 (Accel) to 1 -> 5ms delay per step (step = 1/5 = 0.2 per ms)
    // Code: accelDelay = std::max(1, (int)_cvAccel) * 5.0f;
    // float step = (float)dt / accelDelay;
    // If CV3=1, accelDelay = 5.0f.

    // Directly set internal state to bypass CV update timer
    mc._cvAccel = 1;
    mc._cvVStart = 0;

    SystemState& state = SystemContext::getInstance().getState();
    state.speed = 100; // Target speed

    // Initial update
    mc.loop();
    assert(mc._currentSpeed == 0.0f);

    // Advance time by 10ms.
    // dt = 10
    // step = 10 / 5 = 2.0
    _mockMillis += 10;
    mc.loop();

    // Current speed should increase
    std::cout << "Current Speed after 10ms: " << mc._currentSpeed << std::endl;
    assert(fabs(mc._currentSpeed - 2.0f) < 0.1f);

    // Advance 100ms
    _mockMillis += 100;
    mc.loop();

    // Should be around 2 + (100/5) = 22
    std::cout << "Current Speed after +100ms: " << mc._currentSpeed << std::endl;
    assert(mc._currentSpeed > 20.0f);

    std::cout << "test_momentum passed." << std::endl;
}

void test_kick_start() {
    std::cout << "Running test_kick_start..." << std::endl;
    reset_mocks();
    MotorController& mc = MotorController::getInstance();
    reset_motor_controller(mc);

    // CV65 Kick Start = 255 (Max) -> mapped to 350 PWM bonus
    // Directly set internal state
    mc._cvKickStart = 255;
    mc._cvVStart = 0;
    mc._cvPedestalFloor = 0;

    // Also set mockCVs because _updateCvCache() runs at millis()=1000
    _mockCVs[65] = 255;
    _mockCVs[CV::V_START] = 0;
    _mockCVs[CV::PEDESTAL_FLOOR] = 0;

    SystemState& state = SystemContext::getInstance().getState();
    state.speed = 10; // Low speed

    // Start moving
    _mockMillis = 1000;
    mc.loop();

    // Check if _kickStartTimer was set
    assert(mc._kickStartTimer == 1000);
    assert(mc._isMoving == true);

    // Check PWM output.
    // Kick bonus = 350
    std::cout << "Last PWM Value (KickStart): " << mc._lastPwmValue << std::endl;
    assert(mc._lastPwmValue > 300);

    std::cout << "test_kick_start passed." << std::endl;
}

void test_pid_response() {
    std::cout << "Running test_pid_response..." << std::endl;
    reset_mocks();
    MotorController& mc = MotorController::getInstance();
    reset_motor_controller(mc);

    // Configure PI
    mc._cvKp = 50;
    mc._cvKi = 10;
    mc._cvVStart = 20;

    // Set mocks to prevent overwriting with 0 when _updateCvCache runs
    _mockCVs[112] = 50;
    _mockCVs[114] = 10;
    _mockCVs[CV::V_START] = 20;
    _mockCVs[CV::PEDESTAL_FLOOR] = 160;
    _mockCVs[CV::LOAD_GAIN_SCALAR] = 20;
    _mockCVs[CV::LEARN_THRESHOLD] = 60;

    SystemState& state = SystemContext::getInstance().getState();
    state.speed = 50;

    // Force speed to target instantly to skip momentum
    mc._currentSpeed = 50.0f;
    mc._isMoving = true;

    // Simulate low load
    // _getPeakADC averages 50 reads.
    _mockAnalogRead = [](uint8_t p) { return 100; };

    // Run loop multiple times to accumulate integral
    for(int i=0; i<10; i++) {
        _mockMillis += 25; // > 20ms for PI update
        mc.loop();
    }

    float lowLoadPwm = mc._lastPwmValue;
    std::cout << "Low Load PWM: " << lowLoadPwm << std::endl;

    // Increase load
    _mockAnalogRead = [](uint8_t p) { return 400; }; // Higher load

     for(int i=0; i<20; i++) {
        _mockMillis += 25;
        mc.loop();
    }

    float highLoadPwm = mc._lastPwmValue;
    std::cout << "High Load PWM: " << highLoadPwm << std::endl;

    // PWM should increase to compensate load
    assert(highLoadPwm > lowLoadPwm);

    std::cout << "test_pid_response passed." << std::endl;
}

void test_test_mode() {
    std::cout << "Running test_test_mode..." << std::endl;
    reset_mocks();
    MotorController& mc = MotorController::getInstance();
    reset_motor_controller(mc);

    mc.startTest();
    assert(mc._testMode == true);

    _mockMillis += 100;
    mc.loop();

    // Check if test data is being collected
    assert(mc._testDataIdx > 0);

    String json = mc.getTestJSON();
    std::cout << "Test JSON: " << json.c_str() << std::endl;

    assert(json.startsWith("["));
    assert(json.endsWith("]"));

    std::cout << "test_test_mode passed." << std::endl;
}

int main() {
    test_initialization();
    test_momentum();
    test_kick_start();
    test_pid_response();
    test_test_mode();

    std::cout << "All MotorController tests passed." << std::endl;
    return 0;
}
