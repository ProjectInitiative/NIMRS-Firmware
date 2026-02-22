#include "BootLoopDetector.h"
#include <Preferences.h>
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
  // It could be cleared by a separate API call or on next boot.
  // For now, let's clear it if it was set *previously*?
  // Actually, keeping it persistent until explicitly cleared or next crash is
  // safer. But if we reboot cleanly, we don't want to see "rolled back"
  // forever. Let's compromise: If we are stable, we clear it? No, the user
  // needs to know *why* they are on the old version.
  prefs.end();
}

bool BootLoopDetector::didRollback() {
  Preferences prefs;
  prefs.begin(PREF_NAMESPACE, true);
  bool rb = prefs.getBool(PREF_KEY_ROLLEDBACK, false);
  prefs.end();
  return rb;
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
