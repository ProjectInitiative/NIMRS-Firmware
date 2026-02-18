// MotorController.cpp - Simple Open Loop with 20kHz PWM and High Starting Torque

#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cmath>

// Simple table: Starts high (400) to overcome static friction
static const uint16_t SIMPLE_PWM_TABLE[29] = {
    0,    // Speed 0: stop
    400, 420, 440, 460, 480, 500, 520, 540, 560, 580,  // Steps 1-10: Start HIGH
    600, 640, 680, 720, 760, 800, 840, 880, 920, 960,  // Steps 11-20
    980, 990, 1000, 1010, 1015, 1020, 1022, 1023       // Steps 21-28
};

MotorController::MotorController() : 
    _currentDirection(true), _currentSpeed(0.0f), _lastMomentumUpdate(0),
    _avgCurrent(0.0f), _currentOffset(5.0f), _torqueIntegrator(0.0f),
    _maxPwm(1023), _pwmFreq(20000)  // 20kHz for smooth operation
{
    for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.05f;
}

void MotorController::setup() {
    Log.println("NIMRS: Simple Mode - 20kHz PWM / High Start");
    
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);
    
    pinMode(Pinout::MOTOR_IN1, OUTPUT);
    pinMode(Pinout::MOTOR_IN2, OUTPUT);
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
    pinMode(Pinout::MOTOR_CURRENT, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db);
    
    ledcSetup(_pwmChannel1, 20000, 10);
    ledcSetup(_pwmChannel2, 20000, 10);
    ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
    ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);
    
    float offsetAccum = 0.0f;
    for (int i = 0; i < 100; i++) {
        offsetAccum += (float)analogRead(Pinout::MOTOR_CURRENT);
        delay(2);
    }
    _currentOffset = offsetAccum / 100.0f;
    Log.printf("NIMRS: ADC offset %.2f\n", _currentOffset);
    
    _drive(0, true);
}

void MotorController::loop() {
    uint8_t targetSpeed;
    bool reqDirection;
    {
        SystemContext &ctx = SystemContext::getInstance();
        ScopedLock lock(ctx);
        targetSpeed = ctx.getState().speed;
        reqDirection = ctx.getState().direction;
    }
    _updateCvCache();
    
    if (reqDirection != _currentDirection) {
        _currentDirection = reqDirection;
        _avgCurrent = 0.0f;
    }
    
    // Momentum
    unsigned long now = millis();
    unsigned long dt = now - _lastMomentumUpdate;
    if (dt >= 5) {
        _lastMomentumUpdate = now;
        float accelDelay = std::max(1, (int)_cvAccel) * 10.0f; 
        float step = (float)dt / accelDelay;
        if (targetSpeed > _currentSpeed) 
            _currentSpeed = std::min((float)targetSpeed, _currentSpeed + step);
        else if (targetSpeed < _currentSpeed) 
            _currentSpeed = std::max((float)targetSpeed, _currentSpeed - step);
    }
    
    // Current Sensing
    float rawPeak = (float)_getPeakADC();
    float netPeak = std::max(0.0f, rawPeak - _currentOffset);
    float currentAmps = netPeak * 0.00054f;
    _avgCurrent = (_avgCurrent * 0.90f) + (currentAmps * 0.10f);
    
    // Open Loop Drive
    uint8_t speedIdx = (uint8_t)std::min(28.0f, std::max(0.0f, _currentSpeed));
    uint16_t pwm = SIMPLE_PWM_TABLE[speedIdx];
    
    _drive(pwm, _currentDirection);
    
    SystemContext::getInstance().getState().loadFactor = 0.5f;
    streamTelemetry();
}

uint32_t MotorController::_getPeakADC() {
    uint32_t maxVal = 0;
    for(int i = 0; i < 20; i++) {
        uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
        if (s > maxVal) maxVal = s;
    }
    return maxVal;
}

void MotorController::_drive(uint16_t speed, bool direction) {
    _lastPwmValue = (int16_t)speed;
    
    if (speed == 0) {
        // BRAKE: Both HIGH (Slow Decay)
        // This keeps the low-side FETs on, allowing current to recirculate
        ledcWrite(_pwmChannel1, _maxPwm); 
        ledcWrite(_pwmChannel2, _maxPwm); 
        return;
    }
    
    // Standard Slow Decay Drive:
    // One pin HIGH, other pin PWM (inverted logic)
    // 1023 (MAX) - speed = inverted duty cycle
    uint32_t duty = _maxPwm - speed;
    
    if (direction) {
        ledcWrite(_pwmChannel1, _maxPwm); // IN1 High
        ledcWrite(_pwmChannel2, duty);    // IN2 PWM (Active Low)
    } else {
        ledcWrite(_pwmChannel1, duty);    // IN1 PWM (Active Low)
        ledcWrite(_pwmChannel2, _maxPwm); // IN2 High
    }
}

void MotorController::streamTelemetry() {
    static unsigned long lastS = 0;
    if (millis() - lastS > 150) {
        lastS = millis();
        SystemState &state = SystemContext::getInstance().getState();
        Log.printf("[NIMRS_DATA],%d,%.1f,%d,%.3f,%.3f,%d\n",
            state.speed, _currentSpeed, _lastPwmValue, 
            _avgCurrent, 0.0f, (int)_getPeakADC());
    }
}

// Stubs
void MotorController::_updateCvCache() {
    if (millis() - _lastCvUpdate > 500) {
        _lastCvUpdate = millis();
        NmraDcc &dcc = DccController::getInstance().getDcc();
        _cvAccel = dcc.getCV(CV::ACCEL);
    }
}
void MotorController::_generateScurve(uint8_t i) {}
void MotorController::_saveBaselineTable() {}
void MotorController::_loadBaselineTable() {}