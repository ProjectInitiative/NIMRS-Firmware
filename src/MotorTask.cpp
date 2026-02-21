#include "MotorTask.h"
#include "MotorHal.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "Logger.h"
#include <Arduino.h>
#include <cmath>

MotorTask& MotorTask::getInstance() {
    static MotorTask instance;
    return instance;
}

MotorTask::MotorTask() :
    _taskHandle(NULL),
    _currentFilter(0.1f), // Alpha for I_avg
    _targetSpeedStep(0),
    _targetDirection(true),
    _currentDuty(0.0f),
    _piErrorSum(0.0f),
    _kp(0.1f),
    _ki(0.01f),
    _trackVoltage(14.0f),
    _maxRpm(6000.0f) // Can be tunable
{
}

void MotorTask::start() {
    // 1. Initialize Hardware
    MotorHal::getInstance().init();

    // 2. Load CVs
    reloadCvs();

    // 3. Create Task
    xTaskCreatePinnedToCore(
        _taskEntry,
        "MotorTask",
        4096,
        this,
        10, // Priority
        &_taskHandle,
        1 // Core 1
    );
}

void MotorTask::_taskEntry(void* param) {
    MotorTask* self = (MotorTask*)param;
    self->_loop();
}

void MotorTask::_loop() {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // 50Hz

    // Buffer for ADC (20kHz * 20ms = 400 samples minimum)
    static float adcBuffer[1024];

    while(true) {
        // 1. Wait for next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 2. Read Sensors
        size_t samples = MotorHal::getInstance().getAdcSamples(adcBuffer, 1024);
        float sampleRate = MotorHal::getInstance().getAdcSampleRate();

        // 3. Process Ripple & Current
        // Average current (Low speed)
        float sumCurrent = 0.0f;
        for(size_t i=0; i<samples; i++) {
            sumCurrent += adcBuffer[i];
        }
        float instantAvg = (samples > 0) ? (sumCurrent / samples) : 0.0f;

        // Convert raw ADC to Amps (Need calibration factor)
        // Using existing factor from MotorController: 0.00054f
        float currentAmps = instantAvg * 0.00054f;

        // Update Filters
        float avgCurrent = _currentFilter.update(currentAmps);

        // Ripple Detector
        // Convert buffer to Amps for RippleDetector
        for(size_t i=0; i<samples; i++) {
            adcBuffer[i] *= 0.00054f;
        }
        _rippleDetector.processBuffer(adcBuffer, samples, sampleRate);
        float rippleFreq = _rippleDetector.getFrequency();

        // 4. Update Estimator
        float vApplied = _trackVoltage * fabs(_currentDuty);
        _estimator.updateLowSpeedData(vApplied, avgCurrent);
        _estimator.updateRippleFreq(rippleFreq);
        _estimator.calculateEstimate();

        float actualRpm = _estimator.getEstimatedRpm();

        // 5. Control Loop
        if (_targetSpeedStep == 0) {
            _currentDuty = 0.0f;
            _piErrorSum = 0.0f;
        } else {
            // Target RPM
            float targetRpm = (_targetSpeedStep / 255.0f) * _maxRpm;

            float error = targetRpm - actualRpm;

            // PI
            _piErrorSum += error;
            // Anti-windup
            if (_piErrorSum > 2000.0f) _piErrorSum = 2000.0f;
            if (_piErrorSum < -2000.0f) _piErrorSum = -2000.0f;

            float output = (_kp * error) + (_ki * _piErrorSum);

            float vControl = output;

            // Constrain Voltage
            if (vControl > _trackVoltage) vControl = _trackVoltage;
            if (vControl < 0.0f) vControl = 0.0f;

            // Calculate Duty
            float duty = vControl / _trackVoltage;
            if (!_targetDirection) duty = -duty;
            _currentDuty = duty;
        }

        MotorHal::getInstance().setDuty(_currentDuty);

        // 6. Telemetry Update
        _status.appliedVoltage = vApplied;
        _status.current = avgCurrent;
        _status.estimatedRpm = actualRpm;
        _status.rippleFreq = rippleFreq;
        _status.stalled = _estimator.isStalled();
        _status.duty = _currentDuty;
    }
}

void MotorTask::setTargetSpeed(uint8_t speedStep, bool forward) {
    _targetSpeedStep = speedStep;
    _targetDirection = forward;
}

void MotorTask::reloadCvs() {
    NmraDcc &dcc = DccController::getInstance().getDcc();

    // Read CVs
    uint16_t cvRa = dcc.getCV(CV::MOTOR_R_ARM); // 10mOhm units
    float rArm = cvRa * 0.01f; // Convert to Ohm

    uint16_t cvTv = dcc.getCV(CV::TRACK_VOLTAGE); // 100mV units
    _trackVoltage = cvTv * 0.1f; // Volts
    if (_trackVoltage < 5.0f) _trackVoltage = 12.0f; // Safety default

    uint16_t poles = dcc.getCV(CV::MOTOR_POLES);
    if (poles == 0) poles = 5;

    _estimator.setMotorParams(rArm, poles);

    // PI Gains
    uint16_t kp = dcc.getCV(CV::MOTOR_KP);
    uint16_t ki = dcc.getCV(CV::MOTOR_KI);

    _kp = kp * 0.01f;
    _ki = ki * 0.001f;
}

MotorTask::Status MotorTask::getStatus() const {
    return _status;
}
