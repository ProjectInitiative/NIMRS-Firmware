#include "LightingController.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "Logger.h"

// Set this to true if lights are ON when pin is LOW
static const bool INVERT_OUTPUTS = false;

LightingController::LightingController() {}

void LightingController::setup() {
  Log.println("OutputController: Initializing...");

  // Initialize all mapped pins as OUTPUT
  pinMode(Pinout::LIGHT_FRONT, OUTPUT);
  pinMode(Pinout::LIGHT_REAR, OUTPUT);
  pinMode(Pinout::AUX1, OUTPUT);
  pinMode(Pinout::AUX2, OUTPUT);
  pinMode(Pinout::AUX3, OUTPUT);
  pinMode(Pinout::AUX4, OUTPUT);
  pinMode(Pinout::AUX5, OUTPUT);
  pinMode(Pinout::AUX6, OUTPUT);
  pinMode(Pinout::INPUT1_AUX7, OUTPUT);
  pinMode(Pinout::INPUT2_AUX8, OUTPUT);

  // Default state: OFF
  uint8_t offVal = INVERT_OUTPUTS ? HIGH : LOW;
  digitalWrite(Pinout::LIGHT_FRONT, offVal);
  digitalWrite(Pinout::LIGHT_REAR, offVal);
  digitalWrite(Pinout::AUX1, offVal);
  digitalWrite(Pinout::AUX2, offVal);
  digitalWrite(Pinout::AUX3, offVal);
  digitalWrite(Pinout::AUX4, offVal);
  digitalWrite(Pinout::AUX5, offVal);
  digitalWrite(Pinout::AUX6, offVal);
  digitalWrite(Pinout::INPUT1_AUX7, offVal);
  digitalWrite(Pinout::INPUT2_AUX8, offVal);

  Log.println("OutputController: Ready.");
}

void LightingController::loop() {
  bool functions[29];
  bool direction;

  {
    SystemContext &ctx = SystemContext::getInstance();
    ScopedLock lock(ctx);
    memcpy(functions, ctx.getState().functions, 29);
    direction = ctx.getState().direction;
  }

  NmraDcc &dcc = DccController::getInstance().getDcc();
  static bool lastPinState[50] = {false};

  auto driveOutput = [&](const char *name, uint8_t pin, uint8_t fMap,
                         bool isFront, bool isRear) {
    bool active = false;
    if (fMap < 29) {
      if (fMap == 0) {
        if (functions[0]) {
          if (isFront)
            active = direction;
          else if (isRear)
            active = !direction;
          else
            active = true;
        }
      } else {
        active = functions[fMap];
      }
    }

    uint8_t physVal =
        active ? (INVERT_OUTPUTS ? LOW : HIGH) : (INVERT_OUTPUTS ? HIGH : LOW);
    digitalWrite(pin, physVal);

    if (active != lastPinState[pin]) {
      Log.debug("Output: %s (Pin %d) mapped to F%d -> %s\n", name, pin, fMap,
                active ? "ON" : "OFF");
      lastPinState[pin] = active;
    }
  };

  driveOutput("FRONT", Pinout::LIGHT_FRONT, dcc.getCV(CV::FRONT), true, false);
  driveOutput("REAR", Pinout::LIGHT_REAR, dcc.getCV(CV::REAR), false, true);
  driveOutput("AUX1", Pinout::AUX1, dcc.getCV(CV::AUX1), false, false);
  driveOutput("AUX2", Pinout::AUX2, dcc.getCV(CV::AUX2), false, false);

  // GPIO 35 is Input Only on S3
  driveOutput("AUX3", Pinout::AUX3, dcc.getCV(CV::AUX3), false, false);

  driveOutput("AUX4", Pinout::AUX4, dcc.getCV(CV::AUX4), false, false);

  // GPIO 17 often PSRAM
  driveOutput("AUX5", Pinout::AUX5, dcc.getCV(CV::AUX5), false, false);

  driveOutput("AUX6", Pinout::AUX6, dcc.getCV(CV::AUX6), false, false);
  driveOutput("AUX7", Pinout::INPUT1_AUX7, dcc.getCV(CV::AUX7), false, false);
  driveOutput("AUX8", Pinout::INPUT2_AUX8, dcc.getCV(CV::AUX8), false, false);
}
