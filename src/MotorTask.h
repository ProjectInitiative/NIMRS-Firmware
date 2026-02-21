#ifndef MOTOR_TASK_H
#define MOTOR_TASK_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "MotorHal.h"
#include "BemfEstimator.h"
#include "RippleDetector.h"
#include "DspFilters.h"

class MotorTask {
public:
    static MotorTask& getInstance();

    // Start the FreeRTOS task on Core 1
    void start();

    // Set Target Speed (0-255) and Direction
    void setTargetSpeed(uint8_t speedStep, bool forward);

    // Update CVs from Registry
    void reloadCvs();

    // Telemetry
    struct Status {
        float appliedVoltage;
        float current;
        float estimatedRpm;
        float rippleFreq;
        bool stalled;
        float duty;
    };
    Status getStatus() const;

private:
    MotorTask();

    static void _taskEntry(void* param);
    void _loop();

    TaskHandle_t _taskHandle;

    // Components
    BemfEstimator _estimator;
    RippleDetector _rippleDetector;
    EmaFilter _currentFilter; // For low speed average (I_avg)

    // Control State
    uint8_t _targetSpeedStep;
    bool _targetDirection;
    float _currentDuty; // Current PWM output

    // PI State
    float _piErrorSum;

    // CV Cache
    float _kp;
    float _ki;
    float _trackVoltage;
    float _maxRpm; // Calculated from CVs? Or hardcoded limit?

    // Telemetry
    Status _status;
};

#endif
