#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "MotorTask.h"
#include "SystemContext.h"
#include <Arduino.h>

class MotorController {
public:
  static MotorController &getInstance() {
    static MotorController instance;
    return instance;
  }

  void setup();
  void loop();
  void streamTelemetry();

  // Test Mode (Delegate to MotorTask)
  void startTest();
  String getTestJSON();

  // Resistance Measurement (Delegate to MotorTask)
  using ResistanceState = MotorTask::ResistanceState;
  void measureResistance();
  ResistanceState getResistanceState() const;
  float getMeasuredResistance() const;

  MotorController(const MotorController &) = delete;
  MotorController &operator=(const MotorController &) = delete;

private:
  MotorController();

  // Momentum State
  uint8_t _cvAccel = 4; // CV3
  uint8_t _cvDecel = 4; // CV4 (Assuming separate decel, though code used Accel
                        // for both mostly)
  // Actually code used _cvAccel for both directions?
  // Let's keep _cvAccel.

  float _currentSpeed = 0.0f; // Filtered speed (0-255)
  unsigned long _lastMomentumUpdate = 0;

  // CV Cache
  unsigned long _lastCvUpdate = 0;
  void _updateCvCache();
};
#endif
