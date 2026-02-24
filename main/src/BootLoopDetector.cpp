#include "BootLoopDetector.h"
#include "Logger.h"
#include <Preferences.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace {
const char *PREF_NAMESPACE = "bootloop";
const char *PREF_KEY_CRASHES = "crashes";
const char *PREF_KEY_ROLLEDBACK = "rolledback";
} // namespace

void BootLoopDetector::check() {
  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, false); // Read-write

  int crashes = prefs.getInt(PREF_KEY_CRASHES, 0);
  crashes++;
  prefs.putInt(PREF_KEY_CRASHES, crashes);
  prefs.end();

  if (crashes >= CRASH_THRESHOLD) {
    rollback();
  }
}

void BootLoopDetector::markSuccessful() {
  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putInt(PREF_KEY_CRASHES, 0);
  // We do NOT clear the rolledback flag here automatically,
  // so the user can see it in the UI.
  prefs.end();

  // Inform the bootloader that this firmware is valid to prevent native
  // rollback
  esp_ota_mark_app_valid_cancel_rollback();
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

  // Try to find the version that crashed
  const esp_partition_t *invalid = esp_ota_get_last_invalid_partition();
  if (invalid) {
    if (esp_ota_get_partition_description(invalid, &desc) == ESP_OK) {
      info.crashedVersion = String(desc.version);
      info.rolledback = true; // Also count native rollback as rolledback
    }
  } else if (info.rolledback) {
    // If it was our custom rollback, the crashed app is in the "next" update
    // slot
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (next && esp_ota_get_partition_description(next, &desc) == ESP_OK) {
      info.crashedVersion = String(desc.version);
    }
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
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

  if (next && next != running) {
    // Set the next partition as the boot partition
    esp_ota_set_boot_partition(next);

    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    // Reset crash count to avoid immediate rollback on the other side
    prefs.putInt(PREF_KEY_CRASHES, 0);
    // Set rollback flag
    prefs.putBool(PREF_KEY_ROLLEDBACK, true);
    prefs.end();

    ESP.restart();
  }
}
