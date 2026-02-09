#ifndef DCC_CONTROLLER_H
#define DCC_CONTROLLER_H

#include <Arduino.h>
#include <NmraDcc.h>
#include "SystemContext.h"

// Forward declaration of callbacks
void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps);
void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState);
uint8_t notifyCVWrite(uint16_t CV, uint8_t Value);

class DccController {
public:
    static DccController& getInstance() {
        static DccController instance;
        return instance;
    }

    DccController();
    void setup();
    void loop();

    // Helper to process callbacks
    void updateSpeed(uint8_t speed, bool direction);
    void updateFunction(uint8_t functionIndex, bool active);
    
    // Access to raw DCC object for CV reading
    NmraDcc& getDcc() { return _dcc; }

private:
    NmraDcc _dcc;
};

#endif