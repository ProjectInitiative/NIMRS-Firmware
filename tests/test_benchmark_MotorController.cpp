// TEST_SOURCES: src/MotorController.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DSKIP_MOCK_MOTOR_CONTROLLER

#include "../src/MotorController.h"
#include "../src/SystemContext.h"
#include "Arduino.h"
#include <chrono>
#include <iostream>
#include <string>

// Mock data needs to be populated
void populateTestData() {
  MotorController &mc = MotorController::getInstance();
  mc.setup(); // Setup calls _drive which uses mocked ledcWrite
  mc.startTest();

  // Populate _testData by simulating time
  // Loop 65 times to ensure we hit MAX_TEST_POINTS (60)
  for (int i = 0; i < 65; i++) {
    _mockMillis += 55; // 50ms interval check in loop()
    mc.loop();
  }
}

int main() {
  populateTestData();
  MotorController &mc = MotorController::getInstance();

  // Verify we have data
  String initialJson = mc.getTestJSON();
  if (initialJson.length() < 100) { // Should be > 3000
    std::cerr << "Error: Test data not populated correctly. Length: " << initialJson.length() << std::endl;
    std::cout << "JSON: " << initialJson.c_str() << std::endl;
    return 1;
  }

  // Basic correctness check
  if (!initialJson.startsWith("[") || !initialJson.endsWith("]")) {
    std::cerr << "Error: Invalid JSON format (brackets missing)." << std::endl;
    return 1;
  }
  if (initialJson.indexOf("\"t\":") == -1 || initialJson.indexOf("\"spd\":") == -1) {
    std::cerr << "Error: JSON missing keys." << std::endl;
    return 1;
  }

  // Benchmark
  auto start = std::chrono::high_resolution_clock::now();
  volatile int len = 0;
  int iterations = 1000;
  for (int i = 0; i < iterations; ++i) {
    String s = mc.getTestJSON();
    len += s.length();
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> diff = end - start;

  std::cout << "Time: " << diff.count() << " s" << std::endl;
  std::cout << "Average time per call: " << (diff.count() / iterations) * 1e6 << " us" << std::endl;

  return 0;
}
