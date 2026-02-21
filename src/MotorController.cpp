#include "MotorController.h"
#include "MotorTask.h"
#include "CvRegistry.h"
#include "Logger.h"

MotorController::MotorController() {}

void MotorController::setup() {
    Log.println("NIMRS: Hybrid Sensorless Mode");
    // MotorTask handles hardware init and starts the task on Core 1
    MotorTask::getInstance().start();
}

void MotorController::loop() {
    // 1. Update Target Speed from System State
    SystemState &state = SystemContext::getInstance().getState();
    MotorTask::getInstance().setTargetSpeed(state.speed, state.direction);
    
    // 2. Periodic CV Update (every 1 second)
    if (millis() - _lastCvUpdate > 1000) {
        _lastCvUpdate = millis();
        MotorTask::getInstance().reloadCvs();
    }
    
    // 3. Telemetry
    streamTelemetry();
}

void MotorController::streamTelemetry() {
    static unsigned long lastS = 0;
    if (millis() - lastS > 150) {
        lastS = millis();
        SystemState &state = SystemContext::getInstance().getState();
        MotorTask::Status status = MotorTask::getInstance().getStatus();
        
        // CSV Format: TargetSpeed, EstRPM, Duty, Current, RippleFreq, Stalled
        Log.printf("[NIMRS_DATA],%d,%.1f,%.2f,%.3f,%.1f,%d\n",
            state.speed,
            status.estimatedRpm,
            status.duty,
            status.current,
            status.rippleFreq,
            status.stalled);
    }
}
