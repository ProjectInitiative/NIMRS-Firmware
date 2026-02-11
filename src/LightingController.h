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
    // Physical GPIO Mapping (Finalized based on Pin Finder results)
    const uint8_t _frontPin = 13; // Verified: Drives FL (Pin 8)
    const uint8_t _rearPin  = 11; // Verified: Drives RL (Pin 7)
    
    // Tentative assignments for AUX outputs
    const uint8_t _aux1Pin  = 8;  
    const uint8_t _aux2Pin  = 9;
    const uint8_t _aux3Pin  = 17;
    const uint8_t _aux4Pin  = 10;
    const uint8_t _aux5Pin  = 14;
    const uint8_t _aux6Pin  = 12;
    
    // Logic inputs
    const uint8_t _in1Pin = 7;
    const uint8_t _in2Pin = 6;
};

#endif