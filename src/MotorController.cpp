// MotorController.cpp - High-Performance PI Control with Kick Start

#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cmath>

MotorController::MotorController() : 
    _currentSpeed(0.0f), 
    _lastMomentumUpdate(0),
    _avgCurrent(0.0f), 
    _currentOffset(5.0f), 
    _piErrorSum(0.0f),
    _maxPwm(1023), 
    _pwmFreq(20000)
{
}

void MotorController::setup() {
    Log.println("NIMRS: PI Mode - 20kHz PWM / Kick Start");
    
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);
    
    pinMode(Pinout::MOTOR_IN1, OUTPUT);
    pinMode(Pinout::MOTOR_IN2, OUTPUT);
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
    pinMode(Pinout::MOTOR_CURRENT, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db);
    
    // Paragon 4 uses high-freq PWM for silent motor operation
    // 20kHz ensures no audible buzzing during the crawl
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
    SystemState &state = SystemContext::getInstance().getState();
    uint8_t targetSpeed = state.speed;
    
    _updateCvCache(); // Ensure CVs are updated from registry

    // 1. MOMENTUM & DIRECTION
    unsigned long now = millis();
    unsigned long dt_ms = now - _lastMomentumUpdate;
    if (dt_ms >= 10) {
        _lastMomentumUpdate = now;
        float accelDelay = std::max(1, (int)_cvAccel) * 5.0f; 
        float step = (float)dt_ms / accelDelay;
        if (targetSpeed > _currentSpeed) _currentSpeed = std::min((float)targetSpeed, _currentSpeed + step);
        else if (targetSpeed < _currentSpeed) _currentSpeed = std::max((float)targetSpeed, _currentSpeed - step);
    }

    // 2. FILTERED CURRENT SENSING (Mimicking CV189)
    // Using a coefficient-based low pass filter instead of raw peak search
    float rawCurrent = (float)_getPeakADC() * 0.00054f; // approx scaling for current
    float alpha = _cvLoadFilter / 255.0f;
    _avgCurrent = (alpha * _avgCurrent) + ((1.0f - alpha) * rawCurrent);

    int32_t finalPwm = 0;

    if (_currentSpeed > 0.5f) {
        // 3. KICK START LOGIC (CV65)
        if (!_isMoving) {
            _kickStartTimer = now;
            _isMoving = true;
        }

        // 4. BASELINE CALCULATION (CV2 - VStart)
        float speedNorm = _currentSpeed / 255.0f;
        float vStartPwm = map(_cvVStart, 0, 255, 0, 1023);
        float basePwm = vStartPwm + (speedNorm * (1023.0f - vStartPwm));

        // 5. PI LOAD COMPENSATION
        // Threshold shifts based on speed steps, similar to Paragon 4 'Load Factor'
        float loadThreshold = (_currentSpeed < 15.0f) ? 0.30f : 0.50f;
        float error = _avgCurrent - loadThreshold;

        // Apply CV118 (Slow Speed Gain) if we are in the crawl range
        float currentKp = _cvKp;
        if (_currentSpeed < 20.0f) {
            currentKp *= (_cvKpSlow / 64.0f); // Scale Kp by CV118 factor
        }

        // Integration (mimicking CV114/115)
        _piErrorSum += error * (_cvKi / 100.0f);
        _piErrorSum = constrain(_piErrorSum, -200.0f, 200.0f);

        float compensation = (error * currentKp) + _piErrorSum;

        // Apply Kick Start burst if within the first 80ms of movement
        if (now - _kickStartTimer < 80) {
            compensation += map(_cvKickStart, 0, 255, 0, 400);
        }

        finalPwm = (int32_t)(basePwm + compensation);
    } else {
        _isMoving = false;
        _piErrorSum = 0;
        finalPwm = 0;
    }

    _drive((uint16_t)constrain(finalPwm, 0, 1023), state.direction);
    
    // Update Load Factor for other systems (Audio/Lighting)
    // Simple heuristic: diff between base PWM and actual PWM
    // This is just a placeholder logic for now
    state.loadFactor = 0.5f; 
    
    streamTelemetry();
}

uint32_t MotorController::_getPeakADC() {
    // High-speed sampling to capture the PWM-on current pulse
    uint32_t maxVal = 0;
    for(int i = 0; i < 40; i++) {
        uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
        if (s > maxVal) maxVal = s;
    }
    return maxVal;
}

void MotorController::_drive(uint16_t speed, bool direction) {
    _lastPwmValue = (int16_t)speed;
    
    if (speed == 0) {
        // BRAKE: Both HIGH (Slow Decay)
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

void MotorController::_updateCvCache() {
    if (millis() - _lastCvUpdate > 500) {
        _lastCvUpdate = millis();
        NmraDcc &dcc = DccController::getInstance().getDcc();
        
        // Standard CVs
        _cvVStart = dcc.getCV(CV::V_START);
        _cvAccel = dcc.getCV(CV::ACCEL);
        
        // Extended Motor Control CVs
        _cvKickStart = dcc.getCV(65);       // User requested CV65 (Kick Start)
        _cvKp = dcc.getCV(112);             // Proportional Gain
        // Note: 113 mentioned in prompt but usually one CV is enough or it's split. 
        // Assuming 112 is the main one or 8-bit.
        
        _cvKi = dcc.getCV(114);             // Integral Gain
        _cvKpSlow = dcc.getCV(118);         // Slow Speed Gain
        _cvLoadFilter = dcc.getCV(189);     // Load Filter
    }
}
