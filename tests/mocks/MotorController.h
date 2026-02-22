#ifndef MOTORCONTROLLER_MOCK_H
#define MOTORCONTROLLER_MOCK_H

#include "Arduino.h"

class MotorController {
public:
  static MotorController &getInstance();
  MotorController();
  void startTest();
  String getTestJSON();

  enum class ResistanceState { IDLE, MEASURING, DONE, ERROR };
  void measureResistance();
  ResistanceState getResistanceState() const;
  float getMeasuredResistance() const;
};

#endif
