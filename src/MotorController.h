#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include "DccController.h"
#include "SystemContext.h"
#include "nimrs-pinout.h"
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

  // Test Mode Stubs
  void startTest();
  String getTestJSON();

  MotorController(const MotorController &) = delete;
  MotorController &operator=(const MotorController &) = delete;

private:
  MotorController();

  unsigned long _lastCvUpdate = 0;
};
#endif
