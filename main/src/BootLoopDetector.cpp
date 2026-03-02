#include "BootLoopDetector.h"
#include "Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs_flash.h>

namespace {
const char *PREF_NAMESPACE = "bootloop";
const char *PREF_KEY_ROLLEDBACK = "rolledback";
const int STABILITY_TIME_MS = 30000;
TimerHandle_t stabilityTimer = nullptr;
} // namespace

extern "C" {
void notifyCVResetFactoryDefault(); // In DccController.cpp
}

void BootLoopDetector::performFactoryReset() {
  Log.println("!!! FACTORY RESET TRIGGERED !!!");

  // 1. Reset all DCC CVs to defaults
  notifyCVResetFactoryDefault();

  // 2. Wipe WiFi and Networking settings
  // This effectively wipes the NVS 'nvs.net' or standard Arduino WiFi storage
  Log.println("Factory Reset: Clearing WiFi credentials...");
  WiFi.disconnect(true, true);

  // 3. TODO: Potentially wipe specific custom NVS namespaces or LittleFS if
  // needed nvs_flash_erase();

  Log.println("Factory Reset: Complete. Rebooting in 1s...");
  delay(1000);
  esp_restart();
}

void BootLoopDetector::timerCallback(TimerHandle_t xTimer) {
  Log.println(
      "BootLoopDetector: Stability timer reached. Running health check...");

  if (performHealthCheck()) {
    Log.println(
        "BootLoopDetector: Health check PASSED. Marking firmware as VALID.");
    BootLoopDetector::markSuccessful();
  } else {
    Log.println("BootLoopDetector: Health check FAILED! Triggering Rollback.");
    rollback();
  }
}

bool BootLoopDetector::performHealthCheck() {
  // --- HEALTH CHECK CODE HERE ---
  // This is called after 30 seconds of uptime.
  // Return true if the system is healthy, false to trigger a rollback.

  // 1. Check if WiFi/Connectivity is functional (if enabled)
  // 2. Check if critical tasks are still responding
  // 3. Check for specific "soft brick" conditions

  return true; // Default to healthy for now
}

void BootLoopDetector::startStabilityTimer() {
  if (stabilityTimer == nullptr) {
    stabilityTimer =
        xTimerCreate("StabilityTimer", pdMS_TO_TICKS(STABILITY_TIME_MS),
                     pdFALSE, nullptr, timerCallback);
    if (stabilityTimer) {
      xTimerStart(stabilityTimer, 0);
      Log.printf("BootLoopDetector: Stability timer started (%d ms)\n",
                 STABILITY_TIME_MS);
    } else {
      Log.println("BootLoopDetector: Failed to create stability timer!");
    }
  }
}

void BootLoopDetector::check() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (!running)
    return;

  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
    if (state == ESP_OTA_IMG_PENDING_VERIFY || state == ESP_OTA_IMG_NEW) {
      Log.printf("Boot: MONITORING unverified firmware on %s...\n",
                 running->label);
      startStabilityTimer();
    } else {
      Log.printf("Boot: Running on %s (state: %d)\n", running->label, state);
    }
  }
}

void BootLoopDetector::markSuccessful() {
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (err == ESP_OK) {
    Log.println("BootLoopDetector: Firmware marked VALID.");
  } else if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    // This is expected if already valid or running from factory
  } else {
    Log.printf("BootLoopDetector: Failed to mark valid (0x%x)\n", err);
  }
}

bool BootLoopDetector::didRollback() {
  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, true);
  bool rb = prefs.getBool(PREF_KEY_ROLLEDBACK, false);
  prefs.end();
  return rb;
}

RollbackInfo BootLoopDetector::getRollbackInfo() {
  RollbackInfo info;
  info.rolledback = false;
  info.runningVersion = "Unknown";
  info.crashedVersion = "Unknown";

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_app_desc_t desc;
  if (esp_ota_get_partition_description(running, &desc) == ESP_OK) {
    info.runningVersion = String(desc.version);
  }

  // A rollback occurred if:
  // 1. The NVS flag is set (our custom/older logic)
  // 2. The bootloader found an invalid partition (native rollback)
  bool nativeRollback = false;
  const esp_partition_t *invalid = esp_ota_get_last_invalid_partition();
  if (invalid && esp_ota_get_partition_description(invalid, &desc) == ESP_OK) {
    info.crashedVersion = String(desc.version);
    nativeRollback = true;
  }

  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, true);
  bool nvsRolledback = prefs.getBool(PREF_KEY_ROLLEDBACK, false);
  bool acknowledged = prefs.getBool("acknowledged", false);
  prefs.end();

  if ((nativeRollback || nvsRolledback) && !acknowledged) {
    info.rolledback = true;
  }

  return info;
}

void BootLoopDetector::clearRollback() {
  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putBool(PREF_KEY_ROLLEDBACK, false);
  prefs.putBool("acknowledged", true);
  prefs.end();
  Log.println("BootLoopDetector: Rollback warning acknowledged.");
}
void BootLoopDetector::rollback() {
  Log.println("!!! ROLLING BACK !!!");
  if (esp_ota_mark_app_invalid_rollback_and_reboot() != ESP_OK) {
    Log.println("Native rollback failed. Forcing manual switch.");
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next) {
      esp_ota_set_boot_partition(next);
      Preferences prefs;
      prefs.begin(PREF_NAMESPACE, false);
      prefs.putBool(PREF_KEY_ROLLEDBACK, true);
      prefs.end();
      esp_restart();
    }
  }
}
