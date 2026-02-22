#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

MotorController::MotorController()
    : _currentSpeed(0.0f), _lastMomentumUpdate(0) {}

void MotorController::setup() {
  Log.println("NIMRS: Hybrid Motor Control (MotorTask)");
  MotorTask::getInstance().start();
}

void MotorController::loop() {
  SystemState &state = SystemContext::getInstance().getState();
  uint8_t targetSpeed = state.speed;
  bool direction = state.direction;

  _updateCvCache();

  // --- Resistance Measurement Completion Check ---
  static ResistanceState lastResState = ResistanceState::IDLE;
  ResistanceState currentResState =
      MotorTask::getInstance().getResistanceState();
  if (lastResState == ResistanceState::MEASURING &&
      currentResState == ResistanceState::DONE) {
    float r = MotorTask::getInstance().getMeasuredResistance();
    uint16_t cvVal = (uint16_t)(r * 100.0f);
    NmraDcc &dcc = DccController::getInstance().getDcc();
    dcc.setCV(CV::MOTOR_R_ARM, cvVal);
    Log.printf("MotorController: Saved Resistance CV%d = %d\n", CV::MOTOR_R_ARM,
               cvVal);
  }
  lastResState = currentResState;
  // ---------------------------------------------

  // MOMENTUM (CV3)
  // Note: Using CV3 (Accel) for both accel and decel for simplicity as per
  // previous implementation. Ideally CV4 should be used for decel.
  unsigned long now = millis();
  unsigned long dt = now - _lastMomentumUpdate;
  if (dt >= 10) {
    _lastMomentumUpdate = now;

    float accelDelay = std::max(1, (int)_cvAccel) * 5.0f;
    float step = (float)dt / accelDelay;

    if ((float)targetSpeed > _currentSpeed) {
      _currentSpeed += step;
      if (_currentSpeed > targetSpeed)
        _currentSpeed = targetSpeed;
    } else if ((float)targetSpeed < _currentSpeed) {
      _currentSpeed -= step;
      if (_currentSpeed < targetSpeed)
        _currentSpeed = targetSpeed;
    }
  }

  // Update MotorTask
  // _currentSpeed is 0-255 float.
  MotorTask::getInstance().setTargetSpeed((uint8_t)_currentSpeed, direction);

  streamTelemetry();
}

void MotorController::streamTelemetry() {
  static unsigned long lastS = 0;
  if (millis() - lastS > 150) {
    lastS = millis();
    SystemState &state = SystemContext::getInstance().getState();
    MotorTask::Status status = MotorTask::getInstance().getStatus();

    // Log format:
    // [NIMRS_DATA],target_speed,current_speed,pwm_val,current_amps,rpm,raw_adc
    // pwm_val: 0-1023 (derived from duty)
    int pwm = (int)(fabs(status.duty) * 1023.0f);

    Log.printf("[NIMRS_DATA],%d,%.1f,%d,%.3f,%.3f,%d\n", state.speed,
               _currentSpeed, pwm, status.current,
               status.estimatedRpm,               // Replaces unused 0.0f
               (int)(status.current / 0.00054f)); // Reconstruct raw ADC approx
  }
}

void MotorController::_updateCvCache() {
  if (millis() - _lastCvUpdate > 500) {
    _lastCvUpdate = millis();
    NmraDcc &dcc = DccController::getInstance().getDcc();
    _cvAccel = dcc.getCV(CV::ACCEL);

    // Also update MotorTask CVs occasionally
    MotorTask::getInstance().reloadCvs();
  }
}

// Delegation
void MotorController::startTest() { MotorTask::getInstance().startTest(); }
String MotorController::getTestJSON() {
  return MotorTask::getInstance().getTestJSON();
}
void MotorController::measureResistance() {
  MotorTask::getInstance().measureResistance();
}
MotorController::ResistanceState MotorController::getResistanceState() const {
  return MotorTask::getInstance().getResistanceState();
}
float MotorController::getMeasuredResistance() const {
  return MotorTask::getInstance().getMeasuredResistance();
}
