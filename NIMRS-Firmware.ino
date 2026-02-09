/*
 * NIMRS-21Pin-Decoder Firmware
 * Platform: ESP32-S3
 */

#include "config.h"
#include "src/SystemContext.h"
#include "src/ConnectivityManager.h"
#include "src/DccController.h"
#include "src/MotorController.h"
#include "src/LightingController.h"

ConnectivityManager connectivityManager;
DccController dccController;
MotorController motorController;
LightingController lightingController;

void setup() {
    Serial.begin(115200);
    unsigned long start = millis();
    while (!Serial && millis() - start < 2000);
    
    Serial.println("\n\nNIMRS Decoder Starting...");

    // 1. Connectivity
    connectivityManager.setup();

    // 2. Hardware Control
    motorController.setup();
    lightingController.setup();

    // 3. DCC Input
    dccController.setup();

    // 4. Heartbeat Pin
    #ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    #endif
}

void loop() {
    // Communication & Inputs
    connectivityManager.loop();
    dccController.loop();

    // Output Control
    motorController.loop();
    lightingController.loop();
    
    // Heartbeat & Debug
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate > 1000) {
        lastUpdate = millis();
        #ifdef STATUS_LED_PIN
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        #endif
        
        SystemState& state = SystemContext::getInstance().getState();
        Serial.printf("Status: Spd=%d Dir=%d WiFi=%d F0=%d F1=%d\n", 
            state.speed, state.direction, state.wifiConnected, state.functions[0], state.functions[1]);
    }
}
