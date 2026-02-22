#ifndef DCC_CONTROLLER_H
#define DCC_CONTROLLER_H

#include "SystemContext.h"
#include <Arduino.h>
#include <NmraDcc.h>

// Forward declaration of callbacks
void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed,
                    DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps);
void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp,
                   uint8_t FuncState);
uint8_t notifyCVWrite(uint16_t CV, uint8_t Value);

class DccController {
public:
#ifndef UNIT_TEST
  static DccController &getInstance() {
    static DccController instance;
    return instance;
  }
#else
  static DccController &getInstance();
#endif

  DccController();
  void setup();
  void setupStorage();
  void loop();

  // Helper to process callbacks
  void updateSpeed(uint8_t speed, bool direction);
  void updateFunction(uint8_t functionIndex, bool active);

  // Access to raw DCC object for CV reading
#ifndef UNIT_TEST
  NmraDcc &getDcc() { return _dcc; }
#else
  NmraDcc &getDcc();
#endif

  // Check if we have received a valid packet recently
  bool isPacketValid();

private:
  NmraDcc _dcc;
};

#endif
