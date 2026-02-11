#ifndef LIGHTING_CONTROLLER_H
#define LIGHTING_CONTROLLER_H

#include <Arduino.h>
#include "SystemContext.h"
#include "nimrs-pinout.h"

class LightingController {
public:
    LightingController();
    void setup();
    void loop();

private:
    // No local pin definitions - using Pinout:: namespace
};

#endif
