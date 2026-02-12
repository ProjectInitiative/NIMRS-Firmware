#include "DccController.h"
#include "../config.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "nimrs-pinout.h"
#include <EEPROM.h>

DccController::DccController() {}

// Forward declaration
void notifyCVResetFactoryDefault();

void DccController::setup() {
  // 1. Setup Pin first so init() knows which interrupt to attach
  _dcc.pin(Pinout::TRACK_LEFT_3V3, 1);

  if (!EEPROM.begin(512)) {
    Log.println("DccController: EEPROM Init Failed!");
  } else {
    Log.println("DccController: EEPROM Initialized");
  }

  // 2. Initialize library (attaches interrupt)
  // 0x02 = FLAGS_AUTO_FACTORY_DEFAULT
  _dcc.init(MAN_ID_DIY, 10, 0x02, 0);

  // Check registry for uninitialized mapping
  // If CV::AUX1 (35) is 255, we assume reset is needed.
  if (_dcc.getCV(CV::AUX1) == 255) {
    Log.println("DCC: Forcing Default Mapping...");
    notifyCVResetFactoryDefault();
  }

  Log.printf("DccController: Listening on Pin %d\n", Pinout::TRACK_LEFT_3V3);
}

void DccController::loop() { _dcc.process(); }

void DccController::updateSpeed(uint8_t speed, bool direction) {
  SystemContext &ctx = SystemContext::getInstance();
  {
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();

    if (speed == 0 || speed == 1) {
      state.speed = 0;
    } else {
      state.speed = map(speed, 2, 127, 5, 255);
    }

    state.direction = direction;
    state.lastDccPacketTime = millis();
  }

  Log.debug("DCC: Speed Update\n");
}

void DccController::updateFunction(uint8_t functionIndex, bool active) {
  SystemContext &ctx = SystemContext::getInstance();
  {
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();
    if (functionIndex < 29) {
      state.functions[functionIndex] = active;
    }
  }
  Log.debug("DCC: F%d %s\n", functionIndex, active ? "ON" : "OFF");
}

// --- Global Callbacks ---

void notifyCVResetFactoryDefault() {
  Log.println("DCC: Factory Reset - Writing Defaults...");
  NmraDcc &dcc = DccController::getInstance().getDcc();

  // Automate reset using Registry
  for (size_t i = 0; i < CV_DEFS_COUNT; i++) {
    dcc.setCV(CV_DEFS[i].id, CV_DEFS[i].defaultValue);
  }

  Log.println("DCC: Factory Reset Complete");
}

uint8_t notifyCVWrite(uint16_t CV, uint8_t Value) {
  Log.printf("DCC: Write CV%d = %d\n", CV, Value);
  EEPROM.write(CV, Value);
  EEPROM.commit();
  return Value;
}

void notifyDccSpeed(uint16_t Addr, DCC_ADDR_TYPE AddrType, uint8_t Speed,
                    DCC_DIRECTION Dir, DCC_SPEED_STEPS SpeedSteps) {
  SystemContext &ctx = SystemContext::getInstance();
  bool direction = (Dir == DCC_DIR_FWD);
  uint8_t targetSpeed = 0;

  if (Speed > 1) {
    targetSpeed = map(Speed, 2, 127, 5, 255);
  }

  {
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();
    state.dccAddress = Addr;
    state.speed = targetSpeed;
    state.direction = direction;
    state.lastDccPacketTime = millis();
  }

  Log.debug("DCC Packet: Addr %d Spd %d\n", Addr, Speed);
}

void notifyDccFunc(uint16_t Addr, DCC_ADDR_TYPE AddrType, FN_GROUP FuncGrp,
                   uint8_t FuncState) {
  SystemContext &ctx = SystemContext::getInstance();

  uint8_t baseIndex = 0;
  switch (FuncGrp) {
  case FN_0_4:
    baseIndex = 0;
    break;
  case FN_5_8:
    baseIndex = 5;
    break;
  case FN_9_12:
    baseIndex = 9;
    break;
  case FN_13_20:
    baseIndex = 13;
    break;
  case FN_21_28:
    baseIndex = 21;
    break;
  default:
    return;
  }

  {
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();
    if (FuncGrp == FN_0_4) {
      state.functions[0] = (FuncState & FN_BIT_00) ? true : false;
      state.functions[1] = (FuncState & FN_BIT_01) ? true : false;
      state.functions[2] = (FuncState & FN_BIT_02) ? true : false;
      state.functions[3] = (FuncState & FN_BIT_03) ? true : false;
      state.functions[4] = (FuncState & FN_BIT_04) ? true : false;
    } else {
      for (int i = 0; i < 8; i++) {
        state.functions[baseIndex + i] =
            ((FuncState >> i) & 0x01) ? true : false;
      }
    }
  }
}

void notifyDccMsg(DCC_MSG *Msg) {
  // DCC RAW logging disabled
}