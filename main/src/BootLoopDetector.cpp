#include "BootLoopDetector.h"
#include "Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <Arduino.h>
#include <Preferences.h>
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

void BootLoopDetector::timerCallback(TimerHandle_t xTimer) {
  Log.println(
      "BootLoopDetector: Stability timer reached. Marking firmware as VALID.");
  BootLoopDetector::markSuccessful();
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
  info.rolledback = didRollback();
  info.runningVersion = "Unknown";
  info.crashedVersion = "Unknown";
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_app_desc_t desc;
  if (esp_ota_get_partition_description(running, &desc) == ESP_OK) {
    info.runningVersion = String(desc.version);
  }
  const esp_partition_t *invalid = esp_ota_get_last_invalid_partition();
  if (invalid && esp_ota_get_partition_description(invalid, &desc) == ESP_OK) {
    info.crashedVersion = String(desc.version);
    info.rolledback = true;
  }
  return info;
}

void BootLoopDetector::clearRollback() {
  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putBool(PREF_KEY_ROLLEDBACK, false);
  prefs.end();
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
