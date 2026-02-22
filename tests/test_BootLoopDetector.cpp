// clang-format off
// TEST_SOURCES: src/BootLoopDetector.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DUNIT_TEST
// clang-format on

#include "../src/BootLoopDetector.h"
#include "mocks/Arduino.h"
#include "mocks/Preferences.h"
#include "mocks/esp_ota_ops.h"
#include <cassert>
#include <iostream>

// Helper to reset mocks
void reset_mocks() {
  Preferences::reset();
  ESP.reset();
  setMockRunningPartition(&mock_ota_0);
  esp_ota_set_boot_partition(&mock_ota_0);
}

void test_normal_boot() {
  std::cout << "Running test_normal_boot..." << std::endl;
  reset_mocks();

  // First boot: crashes = 1
  BootLoopDetector::check();
  Preferences prefs;
  assert(prefs.getInt("crashes", 0) == 1);
  assert(!ESP.restartCalled);

  // Mark successful: crashes = 0
  BootLoopDetector::markSuccessful();
  assert(prefs.getInt("crashes", 0) == 0);
  std::cout << "Passed." << std::endl;
}

void test_crash_loop() {
  std::cout << "Running test_crash_loop..." << std::endl;
  reset_mocks();
  Preferences prefs;

  // 1st boot: crashes = 1
  BootLoopDetector::check();
  assert(prefs.getInt("crashes", 0) == 1);
  assert(!ESP.restartCalled);

  // 2nd boot: crashes = 2
  BootLoopDetector::check();
  assert(prefs.getInt("crashes", 0) == 2);
  assert(!ESP.restartCalled);

  // 3rd boot: crashes = 3 -> trigger rollback
  BootLoopDetector::check();

  // Should have reset crashes to 0
  assert(prefs.getInt("crashes", 0) == 0);

  // Should have called restart
  assert(ESP.restartCalled);

  // Should have set boot partition to ota_1
  const esp_partition_t *boot = getMockBootPartition();
  assert(boot != NULL);
  assert(std::string(boot->label) == "ota_1");
  std::cout << "Passed." << std::endl;
}

void test_stability() {
  std::cout << "Running test_stability..." << std::endl;
  reset_mocks();
  Preferences prefs;

  // Simulate some crashes
  BootLoopDetector::check();
  BootLoopDetector::check();
  assert(prefs.getInt("crashes", 0) == 2);

  // Mark successful
  BootLoopDetector::markSuccessful();
  assert(prefs.getInt("crashes", 0) == 0);
  std::cout << "Passed." << std::endl;
}

int main() {
  test_normal_boot();
  test_crash_loop();
  test_stability();
  std::cout << "All BootLoopDetector tests passed!" << std::endl;
  return 0;
}
