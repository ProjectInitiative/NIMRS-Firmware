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
    // Physical GPIO Mapping
    const uint8_t _frontPin = 12; // F0 Forward
    const uint8_t _rearPin  = 10; // F0 Reverse
    const uint8_t _aux1Pin  = 8;  // F1
    const uint8_t _aux2Pin  = 9;  // F2
    const uint8_t _aux3Pin  = 17; // F3
    const uint8_t _aux4Pin  = 13; // F4
    const uint8_t _aux5Pin  = 14; // F5
    const uint8_t _aux6Pin  = 11; // F6
    
    // Logic inputs
    const uint8_t _in1Pin = 7;
    const uint8_t _in2Pin = 6;
};

#endif