#ifndef SYSTEM_CONTEXT_MOCK_H
#define SYSTEM_CONTEXT_MOCK_H

#include "Arduino.h"

enum ControlSource { SOURCE_DCC, SOURCE_WEB };

struct SystemState {
    uint16_t dccAddress = 3;
    uint8_t speed = 0;
    bool direction = true;
    bool functions[29] = {false};
    ControlSource speedSource = SOURCE_DCC;
    bool wifiConnected = false;
};

class SystemContext {
public:
    static SystemContext &getInstance() { static SystemContext instance; return instance; }
    SystemState &getState() { return _state; }
    void lock() {}
    void unlock() {}
private:
    SystemState _state;
};

class ScopedLock {
public:
    ScopedLock(SystemContext &context) {}
};

#endif
