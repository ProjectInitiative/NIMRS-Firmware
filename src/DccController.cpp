#include "DccController.h"
#include "../config.h"
#include "CvRegistry.h"
#include "Logger.h"
#include "nimrs-pinout.h"
#include <EEPROM.h>
#include <WiFi.h>

DccController::DccController() {}

#ifdef UNIT_TEST
DccController &DccController::getInstance() {
  static DccController instance;
  return instance;
}

NmraDcc &DccController::getDcc() { return _dcc; }
#endif

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

  // Check version for automatic factory reset
  // This ensures new CVs are initialized when we bump the firmware version
  uint8_t currentVersion = _dcc.getCV(CV::DECODER_VERSION);
  uint8_t targetVersion = 0;

  // Find the target version from our registry
  for (size_t i = 0; i < CV_DEFS_COUNT; i++) {
    if (CV_DEFS[i].id == CV::DECODER_VERSION) {
      targetVersion = CV_DEFS[i].defaultValue;
      break;
    }
  }

  if (currentVersion != targetVersion) {
    Log.printf("DCC: Version mismatch (Saved: %d, Target: %d). Skipping "
               "auto-reset to avoid loop.\n",
               currentVersion, targetVersion);
  }

  Log.printf("DccController: Listening on Pin %d\n", Pinout::TRACK_LEFT_3V3);
}

void DccController::loop() { _dcc.process(); }

bool DccController::isPacketValid() {
  SystemContext &ctx = SystemContext::getInstance();
  ScopedLock lock(ctx);
  uint32_t lastPacket = ctx.getState().lastDccPacketTime;
  return (lastPacket > 0 && (millis() - lastPacket) < 2000);
}

void DccController::updateSpeed(uint8_t speed, bool direction) {
  SystemContext &ctx = SystemContext::getInstance();
  {
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();

    if (speed == 0 || speed == 1) {
      state.speed = 0;
    } else {
      state.speed = speed;
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
  // Intercept CV8 for Factory Reset
  if (CV == 8) {
    Log.println("DCC: Factory Reset Triggered (CV8)");
    notifyCVResetFactoryDefault();

    // Reset WiFi Credentials (erasing NVS config for WiFi)
    Log.println("DCC: Resetting WiFi Credentials...");
    WiFi.disconnect(true, true);

    // Restart to apply clean state
    Log.println("Rebooting in 1s...");
    delay(1000);
    ESP.restart();
    return Value;
  }

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
    targetSpeed = Speed;
  }

  {
    ScopedLock lock(ctx);
    SystemState &state = ctx.getState();

    // Check if this is a change from the last DCC command
    // Note: We do NOT update state.lastDcc* yet, because we need them for the
    // delta check below

    // Logic:
    // 1. If it's a NEW command from DCC, we switch source to DCC and apply it.
    // 2. If it's the SAME command as before:
    //    a. If we are already in DCC mode, we apply it (refresh).
    //    b. If we are in WEB mode, we IGNORE it (it's just a refresh packet,
    //    let Web rule).

    // Calculate if the DCC packet itself has changed from the PREVIOUS DCC
    // packet
    int dccDelta = abs((int)targetSpeed - (int)state.lastDccSpeed);
    bool isDccInternalChange =
        (dccDelta > 2) || (state.lastDccDirection != direction);

    // Only take control if we are already in DCC mode, OR if the DCC change is
    // significant
    if (state.speedSource == SOURCE_DCC || isDccInternalChange) {
      state.speed = targetSpeed;
      state.direction = direction;
      state.speedSource = SOURCE_DCC;
      state.dccAddress = Addr;

      if (isDccInternalChange) {
        Log.printf("DCC: Control Taken (Spd %d)\n", targetSpeed);
      }
    }

    // Always update last known DCC state so our delta remains relative to the
    // line
    state.lastDccSpeed = targetSpeed;
    state.lastDccDirection = direction;
    state.lastDccPacketTime = millis();
  }
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
