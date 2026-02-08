/*
 * NIMRS-21Pin-Decoder Firmware
 * Platform: ESP32-S3
 */

#include "config.h"
#include "src/SystemContext.h"
#include "src/ConnectivityManager.h"
#include "NmraDcc.h"

ConnectivityManager connectivityManager;
NmraDcc Dcc;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000);
    
    Serial.println("\n\nNIMRS Decoder Starting...");

    // 1. Connectivity
    connectivityManager.setup();

    // 2. DCC Setup
    #ifdef DCC_PIN
    Dcc.pin(0, DCC_PIN, 1);
    Dcc.init(MAN_ID_DIY, 10, CV29_ACCESSORY_DECODER | CV29_OUTPUT_ADDRESS_MODE, 0);
    Serial.printf("DCC: Initialized on Pin %d\n", DCC_PIN);
    #endif

    #ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    #endif
}

void loop() {
    connectivityManager.loop();
    Dcc.process();
    
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 1000) {
        lastHeartbeat = millis();
        #ifdef STATUS_LED_PIN
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        #endif
    }
}
