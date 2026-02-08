#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>

struct SystemState {
    uint16_t dccAddress = 3;
    uint8_t speed = 0;
    bool direction = true; // true = forward
    bool functions[29] = {false};
    
    // Status flags
    bool wifiConnected = false;
    uint32_t lastDccPacketTime = 0;
};

class SystemContext {
public:
    static SystemContext& getInstance() {
        static SystemContext instance;
        return instance;
    }
    
    SystemState& getState() { return _state; }

private:
    SystemContext() {}
    SystemState _state;
};

#endif
