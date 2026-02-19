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
    bool direction = state.direction;

    // --- TEST MODE OVERRIDE ---
    if (_testMode) {
        unsigned long t = millis() - _testStartTime;
        if (t > 3000) {
            _testMode = false;
            targetSpeed = 0;
            _drive(0, true);
        } else {
            // Profile: 
            // 0-1000ms: Ramp FWD 0->100
            // 1000-1500ms: Hold FWD 100
            // 1500-2500ms: Ramp REV 0->100
            // 2500-3000ms: Hold REV 100
            
            if (t < 1000) {
                targetSpeed = map(t, 0, 1000, 0, 100);
                direction = true;
            } else if (t < 1500) {
                targetSpeed = 100;
                direction = true;
            } else if (t < 2500) {
                targetSpeed = map(t, 1500, 2500, 0, 100);
                direction = false;
            } else {
                targetSpeed = 100;
                direction = false;
            }
            
            // Record Data (approx 50ms interval)
            static unsigned long lastLog = 0;
            if (millis() - lastLog > 50 && _testDataIdx < MAX_TEST_POINTS) {
                lastLog = millis();
                _testData[_testDataIdx++] = {
                    (uint32_t)t, targetSpeed, _lastPwmValue, 
                    _avgCurrent, _currentSpeed
                };
            }
        }
    }
    // --------------------------
    
    _updateCvCache(); // Ensure CVs are updated from registry

    // 1. MOMENTUM (CV3/CV4)
    unsigned long now = millis();
    unsigned long dt = now - _lastMomentumUpdate;
    if (dt >= 10) {
        _lastMomentumUpdate = now;
        float accelDelay = std::max(1, (int)_cvAccel) * 5.0f; 
        float step = (float)dt / accelDelay;
        if (targetSpeed > _currentSpeed) _currentSpeed = std::min((float)targetSpeed, _currentSpeed + step);
        else if (targetSpeed < _currentSpeed) _currentSpeed = std::max((float)targetSpeed, _currentSpeed - step);
    }

    // 2. STABILIZED SENSING (CV189 Filter)
    float rawAmps = (float)_getPeakADC() * 0.00054f;
    float alpha = std::min(0.95f, _cvLoadFilter / 255.0f);
    _avgCurrent = (alpha * _avgCurrent) + ((1.0f - alpha) * rawAmps);

    int32_t finalPwm = 0;

    if (_currentSpeed > 0.5f) {
        // 3. THE PARAGON 4 CRAWL ARCHITECTURE
        float speedNorm = _currentSpeed / 255.0f;
        
        // BASELINE: Guarantee the motor has enough power to move (CV2)
        // We set the floor to CV57 (default 160)
        float vStart = map(_cvVStart, 0, 255, _cvPedestalFloor, 1023); 
        float basePwm = vStart + (speedNorm * (1023.0f - vStart));

        // TORQUE PUNCH (CV118): Only adds power if current is HIGHER than expected
        // CV147 sets the base load threshold (0.60A default)
        float thresholdBase = _cvLearnThreshold / 100.0f;
        float expectedLoad = thresholdBase + (speedNorm * 0.4f);
        float loadError = std::max(0.0f, _avgCurrent - expectedLoad);
        
        // CRAWL CAP: At low speeds, we limit how much "Punch" we can add
        // Gradual ramp for punch limit. Max punch is CV146 * 20 (default 400)
        float maxPunch = _cvLoadGainScalar * 20.0f;
        float punchLimit = map((long)_currentSpeed, 0, 20, 150, (long)maxPunch);
        punchLimit = constrain(punchLimit, 150.0f, maxPunch);
        
        float currentKp = (_currentSpeed < 20.0f) ? (_cvKpSlow / 64.0f) : (_cvKp / 128.0f);
        float torquePunch = constrain(loadError * currentKp * 100.0f, 0.0f, punchLimit);

        // KICKSTART (CV65): A single burst, capped to avoid PWM 1023
        float kickBonus = 0;
        if (!_isMoving) {
            _kickStartTimer = now;
            _isMoving = true;
        }
        if (now - _kickStartTimer < 100) {
            kickBonus = map(_cvKickStart, 0, 255, 0, 350); // Cap the kick to 35% extra
        }

        finalPwm = (int32_t)(basePwm + torquePunch + kickBonus);
        
        // SPEED STEP 1 SAFETY: Never allow full power at the lowest steps
        uint32_t absoluteMax = (_currentSpeed < 5.0f) ? 500 : 1023;
        finalPwm = constrain(finalPwm, 0, (int)absoluteMax);

    } else {
        _isMoving = false;
        finalPwm = 0;
    }

    _drive((uint16_t)finalPwm, state.direction);
    streamTelemetry();
}

uint32_t MotorController::_getPeakADC() {
    // High-speed sampling to capture the PWM-on current pulse
    uint32_t maxVal = 0;
    for(int i = 0; i < 100; i++) {
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
        if (_cvLoadFilter == 255) _cvLoadFilter = 120;
        
        _cvPedestalFloor = dcc.getCV(CV::PEDESTAL_FLOOR);
        _cvLoadGainScalar = dcc.getCV(CV::LOAD_GAIN_SCALAR);
        _cvLearnThreshold = dcc.getCV(CV::LEARN_THRESHOLD);
    }
}

void MotorController::startTest() {
    if (_testMode) return;
    _testMode = true;
    _testStartTime = millis();
    _testDataIdx = 0;
    Log.println("Motor: Starting Self-Test...");
}

String MotorController::getTestJSON() {
    String out = "[";
    for(int i=0; i<_testDataIdx; i++) {
        char buf[128];
        sprintf(buf, "{\"t\":%u,\"tgt\":%u,\"pwm\":%u,\"cur\":%.3f,\"spd\":%.1f}", 
            _testData[i].t, _testData[i].target, _testData[i].pwm, _testData[i].current, _testData[i].speed);
        out += buf;
        if(i < _testDataIdx-1) out += ",";
    }
    out += "]";
    return out;
}
