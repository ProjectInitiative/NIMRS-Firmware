#ifndef LIGHTING_CONTROLLER_H
#define LIGHTING_CONTROLLER_H

#include <Arduino.h>
#include "SystemContext.h"

class LightingController {
public:
    LightingController();
    void setup();
    void loop();

private:
    // Pin assignments from CSV
    const uint8_t _frontPin = 12;
    const uint8_t _rearPin  = 10;
    const uint8_t _aux1Pin  = 8;
    const uint8_t _aux2Pin  = 9;
    
    // Logic inputs
    const uint8_t _in1Pin = 7;
    const uint8_t _in2Pin = 6;
};

#endif
