#ifndef DCCCONTROLLER_MOCK_H
#define DCCCONTROLLER_MOCK_H

#include "Arduino.h"
#include "NmraDcc.h"

class DccController {
public:
    static DccController &getInstance();
    DccController();
    NmraDcc &getDcc();
    bool isPacketValid();
private:
    NmraDcc _dcc;
};

#endif
