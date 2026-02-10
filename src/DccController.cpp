#include "DccController.h"
#include "../config.h"
#include <EEPROM.h>
#include "Logger.h"

DccController::DccController() {}

void DccController::setup() {
    #ifdef DCC_PIN
    _dcc.pin(0, DCC_PIN, 1);
    
    if (!EEPROM.begin(512)) {
        Log.println("DccController: EEPROM Init Failed!");
    } else {
        Log.println("DccController: EEPROM Initialized");
    }
    
    // 0x02 = FLAGS_AUTO_FACTORY_DEFAULT
    _dcc.init(MAN_ID_DIY, 10, 0x02, 0); 
    
    // Enable internal pullup (1) - Mandatory for Optocoupler circuits!
    _dcc.pin(0, DCC_PIN, 1);
    
    Log.printf("DccController: Listening on Pin %d\n", DCC_PIN);
    #else
    Log.println("DccController: Error - DCC_PIN not defined!");
    #endif
}

void DccController::loop() {
    _dcc.process();
}

void DccController::updateSpeed(uint8_t speed, bool direction) {
    SystemState& state = SystemContext::getInstance().getState();
    
    if (speed == 0 || speed == 1) {
        state.speed = 0;
    } else {
        state.speed = map(speed, 2, 127, 5, 255);
    }
    
    state.direction = direction;
    state.lastDccPacketTime = millis();
    
    static uint8_t lastSpeed = 255;
    if (state.speed != lastSpeed) {
        Log.printf("DCC: Speed %d, Dir %s\n", state.speed, direction ? "FWD" : "REV");
        lastSpeed = state.speed;
    }
}

void DccController::updateFunction(uint8_t functionIndex, bool active) {
    SystemState& state = SystemContext::getInstance().getState();
    if (functionIndex < 29) {
        state.functions[functionIndex] = active;
        Log.printf("DCC: F%d %s\n", functionIndex, active ? "ON" : "OFF");
    }
}

// --- Global Callbacks ---

void notifyCVResetFactoryDefault() {
    Log.println("DCC: Factory Reset - Writing Defaults...");
    NmraDcc& dcc = DccController::getInstance().getDcc();
    
    dcc.setCV(1, 3);   // Primary Address
    dcc.setCV(2, 0);   // Vstart
    dcc.setCV(3, 2);   // Accel
    dcc.setCV(4, 2);   // Decel
    dcc.setCV(5, 255); // Vhigh
    dcc.setCV(6, 128); // Vmid
    dcc.setCV(29, 6);  // Config (28/128 steps, analog enabled)
    
    Log.println("DCC: Factory Reset Complete");
}

uint8_t notifyCVWrite(uint16_t CV, uint8_t Value) {
    Log.printf("DCC: Write CV%d = %d\n", CV, Value);
    EEPROM.write(CV, Value);
    EEPROM.commit();
    return Value;
}

void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed, DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) {
    SystemState& state = SystemContext::getInstance().getState();
    state.dccAddress = Addr;
    
    bool direction = (Dir == DCC_DIR_FWD);
    uint8_t targetSpeed = 0;
    
    if (Speed > 1) {
        targetSpeed = map(Speed, 2, 127, 5, 255);
    }
    
    state.speed = targetSpeed;
    state.direction = direction;
    state.lastDccPacketTime = millis();
    
    // Log packet for debug
    Log.debug("DCC Packet: Addr %d Spd %d\n", Addr, Speed);
}

void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp, uint8_t FuncState) {
    SystemState& state = SystemContext::getInstance().getState();
    
    uint8_t baseIndex = 0;
    switch(FuncGrp) {
        case FN_0_4:   baseIndex = 0; break;
        case FN_5_8:   baseIndex = 5; break;
        case FN_9_12:  baseIndex = 9; break;
        case FN_13_20: baseIndex = 13; break;
        case FN_21_28: baseIndex = 21; break;
        default: return;
    }

    if (FuncGrp == FN_0_4) {
        state.functions[0] = (FuncState & FN_BIT_00) ? true : false;
        state.functions[1] = (FuncState & FN_BIT_01) ? true : false;
        state.functions[2] = (FuncState & FN_BIT_02) ? true : false;
        state.functions[3] = (FuncState & FN_BIT_03) ? true : false;
        state.functions[4] = (FuncState & FN_BIT_04) ? true : false;
    } else {
        for (int i=0; i<8; i++) {
            if ((FuncState >> i) & 0x01) {
                state.functions[baseIndex + i] = true;
            } else {
                state.functions[baseIndex + i] = false;
            }
        }
    }
    // Log function updates could be useful here if we want to trace all packets
}

void notifyDccMsg(DCC_MSG * Msg) {
    // Only log if debug is enabled to avoid crashing the web UI with 8kHz logs!
    // But useful for verifying signal is being decoded at all.
    // Log.debug("DCC RAW: %d bytes\n", Msg->Size); 
}
