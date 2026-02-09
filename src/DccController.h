#ifndef DCC_CONTROLLER_H
#define DCC_CONTROLLER_H

#include <Arduino.h>
#include <NmraDcc.h>
#include "SystemContext.h"

// Forward declaration of callbacks
void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps);
void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState);

class DccController {
public:
    DccController();
    void setup();
    void loop();

    // Helper to process callbacks
    void updateSpeed(uint8_t speed, bool direction);
    void updateFunction(uint8_t functionIndex, bool active);

private:
    NmraDcc _dcc;
};

#endif
