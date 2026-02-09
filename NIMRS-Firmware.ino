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
#include "src/Logger.h"

ConnectivityManager connectivityManager;
MotorController motorController;
LightingController lightingController;

void setup() {
    // 1. Initialize Logging (Sets up Serial)
    Log.begin(115200);
    
    Log.println("\n\nNIMRS Decoder Starting...");

    // 2. Connectivity (WiFi, Web, Filesystem)
    connectivityManager.setup();

    // 3. Hardware Control
    motorController.setup();
    lightingController.setup();

    // 4. DCC Input
    DccController::getInstance().setup();

    // 5. Heartbeat Pin
    #ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    #endif
}

void loop() {
    connectivityManager.loop();
    DccController::getInstance().loop();
    motorController.loop();
    lightingController.loop();
    
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 1000) {
        lastHeartbeat = millis();
        #ifdef STATUS_LED_PIN
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        #endif
        
        SystemState& state = SystemContext::getInstance().getState();
        // Log status every second
        Log.printf("Status: Spd=%d Dir=%d WiFi=%d F0=%d F1=%d\n", 
            state.speed, state.direction, state.wifiConnected, state.functions[0], state.functions[1]);
    }
}

