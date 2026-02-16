#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "driver/gpio.h"
#include <algorithm>
#include <cmath>

MotorController::MotorController() : 
    _currentDirection(true), _currentSpeed(0.0f), _lastMomentumUpdate(0),
    _lastCvUpdate(0), _avgCurrent(0.0f), _currentOffset(0.0f),
    _torqueIntegrator(0.0f), _pwmResolution(10), _maxPwm(1023),
    _pwmFreq(16000), _activeGainMode(1) // Default to 1 (High-Z)
{
    uint8_t sCurve[] = {1, 3, 7, 12, 18, 26, 35, 46, 58, 72, 87, 103, 119, 135, 
                        151, 166, 181, 194, 206, 217, 226, 234, 241, 246, 250, 253, 254, 255};
    for (int i = 0; i < 28; i++) _speedTable[i] = sCurve[i];
    for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.06f; 
}

void MotorController::setup() {
    Log.println("NIMRS: Initializing Final Control Stack...");

    // 1. PIN RESET & UNLOCK
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
    gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);
    
    // Disable pulls to ensure clean High-Z for Medium Gain
    gpio_pulldown_dis((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
    gpio_pullup_dis((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
    
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
    pinMode(Pinout::MOTOR_IN1, OUTPUT);
    pinMode(Pinout::MOTOR_IN2, OUTPUT);
    pinMode(Pinout::MOTOR_CURRENT, INPUT);

    // 2. ADC & PWM
    analogReadResolution(12);
    analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db); 

    ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
    ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
    ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
    ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

    // 3. PEAK-BURST CALIBRATION
    delay(500);
    float sum = 0;
    for(int i=0; i<64; i++) { sum += (float)_getPeakADC(); delay(2); }
    _currentOffset = sum / 64.0f;

    _loadBaselineTable();
    _drive(0, true);
    Log.printf("✓ Hardware Calibrated. Offset: %.1f\n", _currentOffset);
}

uint32_t MotorController::_getPeakADC() {
    uint32_t maxVal = 0;
    // Burst sample to capture IPROPI peaks during PWM ON-time
    for(int i = 0; i < 32; i++) {
        uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
        if (s > maxVal) maxVal = s;
    }
    return maxVal;
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

    // 1. PEAK SENSING
    float rawPeak = (float)_getPeakADC();
    float netPeak = std::max(0.0f, rawPeak - _currentOffset);
    
    // Using GAIN_LO scalar (205uA/A) since hardware defaults there if floating
    float currentAmps = netPeak * 0.00054f; 
    _avgCurrent = (_avgCurrent * 0.85f) + (currentAmps * 0.15f);

    // 2. MOMENTUM
    unsigned long now = millis();
    unsigned long dt = now - _lastMomentumUpdate;
    if (dt >= 5) {
        _lastMomentumUpdate = now;
        float accelDelay = std::max(1, (int)_cvAccel) * 3.5f;
        float decelDelay = std::max(1, (int)_cvDecel) * 3.5f;
        float step = (float)dt / (targetSpeed > _currentSpeed ? accelDelay : decelDelay);
        if (targetSpeed > _currentSpeed) _currentSpeed = std::min((float)targetSpeed, _currentSpeed + step);
        else if (targetSpeed < _currentSpeed) _currentSpeed = std::max((float)targetSpeed, _currentSpeed - step);
    }

    if (_currentSpeed < 1.0f && reqDirection != _currentDirection) _currentDirection = reqDirection;

    // 3. PI LOAD COMPENSATION
    int32_t pwmOutput = 0;
    int speedIndex = constrain((int)_currentSpeed, 0, 127);

    if (_currentSpeed > 0.1f) {
        // Pedestal (110) for starting torque
        uint32_t pwmStart = std::max((uint32_t)_cvVstart, (uint32_t)110) * 4; 
        float speedNorm = _currentSpeed / 255.0f;
        float basePwm = (float)pwmStart + (speedNorm * (float)(_maxPwm - pwmStart));

        float deltaI = _avgCurrent - _baselineTable[speedIndex];
        if (deltaI < 0) deltaI = 0;

        // Balanced PI Logic
        float pTerm = deltaI * (float)_cvLoadGain * 10.0f; 
        _torqueIntegrator += (deltaI * 0.3f); 
        
        float maxBoost = ((float)_cvDeltaCap / 255.0f) * 500.0f;
        _torqueIntegrator = std::min(_torqueIntegrator, maxBoost);

        if (deltaI < 0.005f) _torqueIntegrator *= 0.96f;

        pwmOutput = (int32_t)(basePwm + pTerm + _torqueIntegrator);
    } else {
        _torqueIntegrator = 0;
    }

    pwmOutput = constrain(pwmOutput, 0, (int32_t)_maxPwm);
    _drive((uint16_t)pwmOutput, _currentDirection);

    SystemContext::getInstance().getState().loadFactor = std::min(_torqueIntegrator / 250.0f, 1.0f);
    streamTelemetry();
}

void MotorController::_drive(uint16_t speed, bool direction) {
    _lastPwmValue = (int16_t)speed;
    if (speed == 0) {
        ledcWrite(_pwmChannel1, _maxPwm); ledcWrite(_pwmChannel2, _maxPwm); 
        return;
    }
    uint32_t invertedPwm = _maxPwm - speed;
    if (direction) {
        ledcWrite(_pwmChannel1, _maxPwm); ledcWrite(_pwmChannel2, invertedPwm); 
    } else {
        ledcWrite(_pwmChannel1, invertedPwm); ledcWrite(_pwmChannel2, _maxPwm);
    }
}

void MotorController::streamTelemetry() {
    static unsigned long lastStream = 0;
    if (millis() - lastStream > 150) {
        lastStream = millis();
        SystemState &state = SystemContext::getInstance().getState();
        
        // Report Logic-State for Gain instead of physical pin voltage
        Log.printf("[NIMRS_DATA],%d,%.2f,%d,%.3f,%.2f,%d,%d,%.1f,%d,%.1f\n",
            state.speed, _currentSpeed, _lastPwmValue, _avgCurrent, state.loadFactor,
            0, analogRead(Pinout::MOTOR_CURRENT), _currentOffset,
            _activeGainMode, (float)_getPeakADC() - _currentOffset
        );
    }
}

void MotorController::_updateCvCache() {
    if (millis() - _lastCvUpdate > 500) {
        _lastCvUpdate = millis();
        NmraDcc &dcc = DccController::getInstance().getDcc();
        _cvAccel = dcc.getCV(CV::ACCEL);
        _cvDecel = dcc.getCV(CV::DECEL);
        _cvVstart = dcc.getCV(CV::V_START);
        _cvLoadGain = dcc.getCV(CV::LOAD_GAIN);
        _cvDeltaCap = dcc.getCV(CV::DELTA_CAP);
        
        uint8_t targetGain = dcc.getCV(CV::HARDWARE_GAIN); 
        if (targetGain == 1) { // HI-Z / Medium
            pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
            _activeGainMode = 1;
        } else if (targetGain == 2) { // Logic HIGH / High Gain
            pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
            digitalWrite(Pinout::MOTOR_GAIN_SEL, HIGH);
            _activeGainMode = 2;
        } else { // Logic LOW / Sleep/Low Gain
            pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
            digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW);
            _activeGainMode = 0;
        }
    }
}

void MotorController::_saveBaselineTable() {
    Preferences prefs;
    prefs.begin("motor", false);
    prefs.putBytes("baseline", _baselineTable, sizeof(_baselineTable));
    prefs.end();
}

void MotorController::_loadBaselineTable() {
    Preferences prefs;
    prefs.begin("motor", true);
    prefs.getBytes("baseline", _baselineTable, sizeof(_baselineTable));
    prefs.end();
}