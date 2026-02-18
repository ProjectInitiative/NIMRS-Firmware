// MotorController.cpp - DIAGNOSTIC VERSION
// Run this first to find your motor's actual crawl current profile

#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cmath>

// DIAGNOSTIC MODE: Disable governor, use fixed PWM table
static constexpr bool   DIAGNOSTIC_MODE      = true;   // <-- SET TO false AFTER TUNING
static constexpr float  CRAWL_BASE_PWM       = 200.0f; // Start very low
static constexpr float  CRAWL_SPEED_THRESH   = 12.0f;
static constexpr float  LOAD_THRESH_CRAWL    = 0.50f;  // Will tune this after diagnostic
static constexpr float  LOAD_THRESH_NORMAL   = 0.70f;
static constexpr float  STALL_CURRENT        = 1.50f;  // Raised: 1.2A is NOT a stall for this motor
static constexpr float  STALL_KICK_PWM       = 100.0f; // Reduced kick strength
static constexpr float  INTEGRATOR_MAX       = 200.0f; // Reduced ceiling
static constexpr float  INTEGRATOR_MIN       = -150.0f;
static constexpr float  SLEW_NORMAL          = 1.8f;   // Slower slew
static constexpr float  SLEW_STALL_RECOVERY  = 8.0f;
static constexpr float  CURRENT_FILTER_SLOW  = 0.94f;  // Even slower for diagnosis
static constexpr float  CURRENT_FILTER_FAST  = 0.75f;
static constexpr int    ADC_SAMPLES          = 60;
static constexpr float  OFFSET_ADAPT_RATE    = 0.001f; // Slower adaptation

// Fixed PWM diagnostic table - will measure actual current at each PWM level
static const uint16_t DIAG_PWM_TABLE[29] = {
    0,    // Speed 0: stop
    180, 200, 220, 240, 260, 280, 300, 320, 340, 360,  // Steps 1-10: crawl range
    400, 440, 480, 520, 560, 600, 640, 680, 720, 760,  // Steps 11-20: mid range
    800, 840, 880, 920, 960, 1000, 1020, 1023          // Steps 21-28: high speed
};

MotorController::MotorController() : 
    _currentDirection(true), _currentSpeed(0.0f), _lastMomentumUpdate(0),
    _avgCurrent(0.0f), _fastCurrent(0.0f), _currentOffset(5.0f),
    _torqueIntegrator(0.0f), _stallKickActive(false), _stallKickTimer(0),
    _maxPwm(1023), _pwmFreq(100), _slewedPwm(0.0f)
{
    for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.05f;
}

void MotorController::setup() {
    Log.println("NIMRS: Motor Diagnostic Mode - Measuring Current Profile...");
    
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);
    
    pinMode(Pinout::MOTOR_IN1, OUTPUT);
    pinMode(Pinout::MOTOR_IN2, OUTPUT);
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
    pinMode(Pinout::MOTOR_CURRENT, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db);
    ledcSetup(_pwmChannel1, 100, 10);
    ledcSetup(_pwmChannel2, 100, 10);
    ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
    ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);
    
    float offsetAccum = 0.0f;
    for (int i = 0; i < 100; i++) {  // Longer calibration
        offsetAccum += (float)analogRead(Pinout::MOTOR_CURRENT);
        delay(2);
    }
    _currentOffset = offsetAccum / 100.0f;
    Log.printf("NIMRS: ADC offset calibrated to %.2f counts\n", _currentOffset);
    
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
        _torqueIntegrator = 0.0f;
        _avgCurrent = 0.0f;
        _fastCurrent = 0.0f;
        _stallKickActive = false;
    }

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

    // === CURRENT SENSING ===
    float rawPeak = (float)_getPeakADC();
    
    if (_currentSpeed < 0.1f) {
        _currentOffset += (rawPeak - _currentOffset) * OFFSET_ADAPT_RATE;
    }
    
    float netPeak = std::max(0.0f, rawPeak - _currentOffset);
    float currentAmps = netPeak * 0.00054f;
    
    _avgCurrent = (_avgCurrent * CURRENT_FILTER_SLOW) + (currentAmps * (1.0f - CURRENT_FILTER_SLOW));
    _fastCurrent = (_fastCurrent * CURRENT_FILTER_FAST) + (currentAmps * (1.0f - CURRENT_FILTER_FAST));

    // === PWM GENERATION ===
    int32_t finalPwm = 0;

    if (DIAGNOSTIC_MODE) {
        // ===================================
        // DIAGNOSTIC: Use fixed PWM table
        // ===================================
        uint8_t speedIdx = (uint8_t)std::min(27.0f, _currentSpeed);
        float targetPwm = DIAG_PWM_TABLE[speedIdx];
        
        // Gentle slew to avoid sudden jerks
        float diff = targetPwm - _slewedPwm;
        _slewedPwm += constrain(diff, -3.0f, 3.0f);
        finalPwm = (int32_t)_slewedPwm;
        
        // Log every 500ms during diagnostic
        static unsigned long lastDiag = 0;
        if (millis() - lastDiag > 500) {
            lastDiag = millis();
            Log.printf("DIAG: SPD=%d PWM=%d AMPS=%.3f (avg=%.3f, peak=%d)\n",
                speedIdx, finalPwm, _fastCurrent, _avgCurrent, (int)rawPeak);
        }
        
    } else {
        // ===================================
        // GOVERNOR MODE (use after tuning)
        // ===================================
        if (_currentSpeed > 0.1f) {
            bool isCrawl = (_currentSpeed < CRAWL_SPEED_THRESH);
            
            float speedNorm = _currentSpeed / 255.0f;
            float basePwm;
            if (isCrawl) {
                basePwm = CRAWL_BASE_PWM + (speedNorm * 60.0f);  // Gentler crawl curve
            } else {
                basePwm = CRAWL_BASE_PWM + (speedNorm * (float)(_maxPwm - (int)CRAWL_BASE_PWM));
            }

            float loadThreshold = isCrawl ? LOAD_THRESH_CRAWL : LOAD_THRESH_NORMAL;
            float loadError = _avgCurrent - loadThreshold;

            // Stall detection (only if current is truly excessive)
            bool isStalled = (_fastCurrent > STALL_CURRENT) && (_currentSpeed > 0.1f) && (_avgCurrent > STALL_CURRENT * 0.8f);
            
            if (isStalled && !_stallKickActive) {
                _stallKickActive = true;
                _stallKickTimer = now;
                _torqueIntegrator = std::min(_torqueIntegrator + STALL_KICK_PWM, INTEGRATOR_MAX);
                Log.println("NIMRS: Stall kick fired");
            }
            if (_stallKickActive && (now - _stallKickTimer > 100)) {
                _stallKickActive = false;
            }

            if (_avgCurrent < 0.02f) {
                // Truly zero current - ramp up cautiously
                if (!_stallKickActive) _torqueIntegrator += 0.5f;
            } else {
                // Governor correction - but MUCH gentler
                float settleRate = (loadError > 0) ? 1.2f : 0.3f;  // Slower braking, even slower boosting
                _torqueIntegrator -= (loadError * settleRate);
                _torqueIntegrator = constrain(_torqueIntegrator, INTEGRATOR_MIN, INTEGRATOR_MAX);
            }

            float targetPwm = basePwm + _torqueIntegrator;
            float slewRate = _stallKickActive ? SLEW_STALL_RECOVERY : SLEW_NORMAL;
            float diff = targetPwm - _slewedPwm;
            _slewedPwm += constrain(diff, -slewRate, slewRate);
            finalPwm = (int32_t)_slewedPwm;

        } else {
            _torqueIntegrator = 0.0f;
            _slewedPwm *= 0.85f;
            if (_slewedPwm < 1.0f) _slewedPwm = 0.0f;
            finalPwm = (int32_t)_slewedPwm;
        }
    }

    _drive((uint16_t)constrain(finalPwm, 0, 1023), _currentDirection);
    
    float loadFactor = (_torqueIntegrator - INTEGRATOR_MIN) / (INTEGRATOR_MAX - INTEGRATOR_MIN);
    SystemContext::getInstance().getState().loadFactor = loadFactor;
    streamTelemetry();
}

uint32_t MotorController::_getPeakADC() {
    uint32_t maxVal = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
        if (s > maxVal) maxVal = s;
    }
    return maxVal;
}

void MotorController::_drive(uint16_t speed, bool direction) {
    _lastPwmValue = (int16_t)speed;
    if (speed == 0) {
        ledcWrite(_pwmChannel1, _maxPwm);
        ledcWrite(_pwmChannel2, _maxPwm); 
        return;
    }
    uint32_t pulse = _maxPwm - speed;
    if (direction) {
        ledcWrite(_pwmChannel1, _maxPwm);
        ledcWrite(_pwmChannel2, pulse); 
    } else {
        ledcWrite(_pwmChannel1, pulse);
        ledcWrite(_pwmChannel2, _maxPwm);
    }
}

void MotorController::streamTelemetry() {
    static unsigned long lastS = 0;
    if (millis() - lastS > 150) {
        lastS = millis();
        SystemState &state = SystemContext::getInstance().getState();
        Log.printf("[NIMRS_DATA],%d,%.1f,%d,%.3f,%.3f,%.3f,%d,%.1f,%.1f,%d\n",
            state.speed, _currentSpeed, _lastPwmValue,
            _avgCurrent, _fastCurrent,
            _torqueIntegrator,
            (int)_stallKickActive,
            _currentOffset,
            _slewedPwm,
            (int)_getPeakADC());
    }
}

// Minimal implementation to satisfy linker, but functionality disabled in DIAGNOSTIC_MODE
void MotorController::_updateCvCache() {
    if (!DIAGNOSTIC_MODE) {
        // Only run full CV update when not in diagnostic mode
        if (millis() - _lastCvUpdate > 500) {
            _lastCvUpdate = millis();
            NmraDcc &dcc = DccController::getInstance().getDcc();
            _cvAccel = dcc.getCV(CV::ACCEL);
            _cvDecel = dcc.getCV(CV::DECEL);
            // ... (other CVs would go here)
        }
    }
}

void MotorController::_generateScurve(uint8_t intensity) {}
void MotorController::_saveBaselineTable() {}
void MotorController::_loadBaselineTable() {}
