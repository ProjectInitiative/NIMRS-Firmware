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

// Task handle
TaskHandle_t ControlPlaneTaskHandle;

// Real-time Control Plane Task (Core 0)
// This task handles the time-critical DCC decoding and motor/output loops.
void controlPlaneTask(void *pvParameters) {
    Log.println("ControlPlane: Task started on Core 0");
    
    // Initialize DCC on Core 0 to ensure interrupt affinity
    DccController::getInstance().setup();
    Log.printf("DCC: Configured Pin: %d (Core 0)\n", DCC_PIN);

    for(;;)
 {
        // High priority loops
        DccController::getInstance().loop();
        motorController.loop();
        lightingController.loop();
        
        // Yield 1ms to prevent watchdog issues and allow background OS tasks
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup() {
    // 1. Initialize Logging (Serial remains disabled to avoid Pin 1 conflict)
    // Log.begin(115200);
    Log.println("\n\nNIMRS Decoder Starting...");

    // 2. Connectivity (WiFi, Web, Filesystem) - Core 1
    connectivityManager.setup();

    // 3. Hardware Control - Setup pins
    motorController.setup();
    lightingController.setup();

    // 4. Create Real-time Task on Core 0
    xTaskCreatePinnedToCore(
        controlPlaneTask,        /* Task function. */
        "ControlPlane",          /* name of task. */
        8192,                    /* Stack size of task */
        NULL,                    /* parameter of the task */
        5,                       /* priority of the task (Higher than main loop) */
        &ControlPlaneTaskHandle, /* Task handle to keep track of created task */
        0);                      /* pin task to core 0 */

    // 5. Heartbeat Pin
    #ifdef STATUS_LED_PIN
    pinMode(STATUS_LED_PIN, OUTPUT);
    #endif
}

void loop() {
    // Main loop handles Web Server and lower-priority tasks (Core 1)
    connectivityManager.loop();
    
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 1000) {
        lastHeartbeat = millis();
        #ifdef STATUS_LED_PIN
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        #endif
        
        SystemContext& ctx = SystemContext::getInstance();
        ScopedLock lock(ctx);
        SystemState& state = ctx.getState();
        
        // Log status every second (at debug level)
        Log.debug("Status: Spd=%d Dir=%d WiFi=%d F0=%d F1=%d\n", 
            state.speed, state.direction, state.wifiConnected, state.functions[0], state.functions[1]);
    }
}
