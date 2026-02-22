#include "BootLoopDetector.h"
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace {
const char *PREF_NAMESPACE = "bootloop";
const char *PREF_KEY_CRASHES = "crashes";
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
  prefs.end();
}

void BootLoopDetector::rollback() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

  if (next && next != running) {
    // Set the next partition as the boot partition
    esp_ota_set_boot_partition(next);

    // Reset crash count to avoid immediate rollback on the other side
    // assuming the previous version was stable.
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putInt(PREF_KEY_CRASHES, 0);
    prefs.end();

    ESP.restart();
  }
}
