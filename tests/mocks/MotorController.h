#ifndef MOTORCONTROLLER_MOCK_H
#define MOTORCONTROLLER_MOCK_H

#include "Arduino.h"

class MotorController {
public:
    static MotorController &getInstance();
    MotorController();
    void startTest();
    String getTestJSON();
};

#endif
