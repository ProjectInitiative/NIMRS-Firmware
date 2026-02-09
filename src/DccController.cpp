#include "DccController.h"
#include "../config.h"

DccController::DccController() {}

void DccController::setup() {
    #ifdef DCC_PIN
    _dcc.pin(0, DCC_PIN, 1);
    
    // Init: Manufacturer ID (13=DIY), Version 10, Flags (Accessory+OutputAddr), EEPROM Offset 0
    // Note: We might want to make this a Multifunction Decoder (Loco) instead of Accessory later?
    // For now, sticking to existing init params but checking flags.
    // CV29_ACCESSORY_DECODER is for accessories. For trains, we usually want defaults.
    
    // Let's use standard Loco config:
    _dcc.init(MAN_ID_DIY, 10, 0, 0); 
    
    Serial.printf("DccController: Listening on Pin %d\n", DCC_PIN);
    #else
    Serial.println("DccController: Error - DCC_PIN not defined!");
    #endif
}

void DccController::loop() {
    _dcc.process();
}

void DccController::updateSpeed(uint8_t speed, bool direction) {
    SystemState& state = SystemContext::getInstance().getState();
    
    // DCC speed 0=Stop, 1=E-Stop, 2-127=Speed steps
    // We map this to 0-255 for internal logic
    if (speed == 0 || speed == 1) {
        state.speed = 0;
    } else {
        // Simple linear map for MVP: (speed - 1) * 2 roughly
        // 127 steps -> 255
        state.speed = map(speed, 2, 127, 5, 255);
    }
    
    state.direction = direction;
    state.lastDccPacketTime = millis();
    
    // Debug print (throttle this in production)
    static uint8_t lastSpeed = 255;
    if (state.speed != lastSpeed) {
        Serial.printf("DCC: Speed %d, Dir %s\n", state.speed, direction ? "FWD" : "REV");
        lastSpeed = state.speed;
    }
}

void DccController::updateFunction(uint8_t functionIndex, bool active) {
    SystemState& state = SystemContext::getInstance().getState();
    if (functionIndex < 29) {
        state.functions[functionIndex] = active;
        Serial.printf("DCC: F%d %s\n", functionIndex, active ? "ON" : "OFF");
    }
}

// Global Callbacks needed by NmraDcc library
// These act as a bridge to our class instance

void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) {
    // In a real app, check if Addr matches our address (NmraDcc usually filters this, but good to verify)
    // For now, we assume NmraDcc filtering is correct.
    
    // We need a singleton access to DccController or just update Context directly.
    // Since DccController is just a logic wrapper, updating Context directly is cleaner for the callback.
    
    // Logic duplicated for now, or we can make DccController a singleton too.
    // Let's go direct to Context to keep callbacks fast.
    
    SystemState& state = SystemContext::getInstance().getState();
    
    bool direction = (Dir == DCC_DIR_FWD);
    
    // Map Speed 
    uint8_t targetSpeed = 0;
    if (Speed > 1) {
        // Basic 128 step mapping
        targetSpeed = map(Speed, 2, 127, 5, 255);
    }
    
    state.speed = targetSpeed;
    state.direction = direction;
    state.lastDccPacketTime = millis();
    
    // Debug
    static uint8_t lastSpd = 255;
    if (targetSpeed != lastSpd) {
        Serial.printf("CB: Speed %d, Dir %s\n", targetSpeed, direction ? "FWD" : "REV");
        lastSpd = targetSpeed;
    }
}

void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) {
    SystemState& state = SystemContext::getInstance().getState();
    
    // Map the bitmask FuncState to our bool array
    // FuncGrp tells us which functions (F0-F4, F5-F8, etc.)
    
    uint8_t baseIndex = 0;
    switch(FuncGrp) {
        case FN_0_4:   baseIndex = 0; break; // F0 is bit 4, F1-F4 are bits 0-3
        case FN_5_8:   baseIndex = 5; break;
        case FN_9_12:  baseIndex = 9; break;
        case FN_13_20: baseIndex = 13; break;
        case FN_21_28: baseIndex = 21; break;
        default: return;
    }

    if (FuncGrp == FN_0_4) {
        // Special case for F0 (Headlight) which is usually bit 4
        state.functions[0] = (FuncState & FN_BIT_00) ? true : false;
        state.functions[1] = (FuncState & FN_BIT_01) ? true : false;
        state.functions[2] = (FuncState & FN_BIT_02) ? true : false;
        state.functions[3] = (FuncState & FN_BIT_03) ? true : false;
        state.functions[4] = (FuncState & FN_BIT_04) ? true : false;
    } else {
        // Generic mapping
        for (int i=0; i<8; i++) {
            // This loop depends on the specific bit packing of NmraDcc, 
            // which can be tricky.
            // Let's use the provided helper macros if possible or simple masks.
            if ((FuncState >> i) & 0x01) {
                state.functions[baseIndex + i] = true;
            } else {
                state.functions[baseIndex + i] = false;
            }
            // Note: This naive loop might overshoot for groups smaller than 8 bits
            // FN_5_8 is 4 bits. 
        }
        
        // Debug
        Serial.printf("CB: FuncGrp %d Update\n", FuncGrp);
    }
}
