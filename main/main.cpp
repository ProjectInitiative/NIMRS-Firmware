/*
 * NIMRS-21Pin-Decoder Firmware
 * Platform: ESP32-S3
 */

#include "config.h"
#include "src/AudioController.h"
#include "src/BootLoopDetector.h"
#include "src/ConnectivityManager.h"
#include "src/DccController.h"
#include "src/LightingController.h"
#include "src/Logger.h"
#include "src/MotorController.h"
#include "src/SystemContext.h"

#include "Arduino.h"
#include "esp_ota_ops.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

ConnectivityManager connectivityManager;
LightingController lightingController;

// Task handles
TaskHandle_t ControlPlaneTaskHandle;
extern TaskHandle_t loopTaskHandle; // Defined in Arduino core main.cpp

// Real-time Control Plane Task (Core 0)
void controlPlaneTask(void *pvParameters) {
  // Initialize DCC hardware on Core 0 for interrupt affinity
  DccController::getInstance().setup();

  for (;;) {
    DccController::getInstance().loop();
    MotorController::getInstance().loop();
    lightingController.loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// Standard Arduino Loop Task
void arduinoLoopTask(void *pvParameters) {
  setup();
  for (;;) {
    loop();
    if (serialEventRun)
      serialEventRun();
  }
}

void setup() {
  // 0. Immediate hardware check
  Serial.begin(115200);
  delay(100);

  // 1. Logging is already started in app_main
  Log.println("NIMRS Decoder Starting...");

  DccController::getInstance().setupStorage();
  connectivityManager.setup();
  MotorController::getInstance().setup();
  lightingController.setup();
  AudioController::getInstance().setup();

  xTaskCreatePinnedToCore(controlPlaneTask, "ControlPlane", 16384, NULL, 5,
                          &ControlPlaneTaskHandle, 0);

#ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
#endif
}

void loop() {
  connectivityManager.loop();
  AudioController::getInstance().loop();

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 1000) {
    lastHeartbeat = millis();
#ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
#endif
  }
}

extern "C" void app_main() {
  // 1. Initialize NVS and Logger fully
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  Log.begin(115200);
  Log.startTask();

  // 2. Immediate boot check and crash loop detection
  // This must happen BEFORE initArduino() to catch early crashes
  BootLoopDetector::check();

  // 3. Standard ESP32-S3 / Arduino Core frequency & USB init
#ifdef F_CPU
  setCpuFrequencyMhz(F_CPU / 1000000);
#endif
#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
  Serial.begin();
#endif

  // 4. Core Arduino Hardware Init
  // This calls verifyRollbackLater() which we override to return true
  initArduino();

  // 5. Start the main Arduino loop task
  xTaskCreateUniversal(arduinoLoopTask, "loopTask", 8192, NULL, 1,
                       &loopTaskHandle, ARDUINO_RUNNING_CORE);
}
