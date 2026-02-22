// TEST_SOURCES: src/Logger.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DSKIP_MOCK_LOGGER
#include "../src/Logger.h"
#include <iostream>
#include <cassert>
#include <string>

// Helper to check if string contains substring
bool contains(const String &s, const String &sub) {
  return s.indexOf(sub) != -1;
}

void test_initial_logs() {
  std::cout << "Testing initial logs..." << std::endl;
  // Initialize Logger
  Log.begin(115200);

  String json = Log.getLogsJSON();
  // Expect "Logger: Initialized"
  assert(contains(json, "Logger: Initialized"));
  assert(json.startsWith("["));
  assert(json.endsWith("]"));
  std::cout << "Passed." << std::endl;
}

void test_add_logs() {
  std::cout << "Testing adding logs..." << std::endl;
  Log.println("Test log 1");
  Log.println("Test log 2");

  String json = Log.getLogsJSON();

  assert(contains(json, "Test log 1"));
  assert(contains(json, "Test log 2"));
  std::cout << "Passed." << std::endl;
}

void test_filter_logs() {
  std::cout << "Testing filtered logs..." << std::endl;
  Log.println("Error: Something bad");
  Log.println("Info: Something good");

  String json = Log.getLogsJSON("Error");
  assert(contains(json, "Error: Something bad"));

  // Should NOT contain "Info: Something good"
  assert(!contains(json, "Info: Something good"));
  // Should NOT contain previous logs
  assert(!contains(json, "Test log 1"));

  std::cout << "Passed." << std::endl;
}

void test_data_logs() {
    std::cout << "Testing data logs..." << std::endl;
    // Data logs are separate buffer.
    Log.println("[NIMRS_DATA] value=123");
    Log.println("Normal log");

    // Test data filter
    String json = Log.getLogsJSON("[NIMRS_DATA]");
    assert(contains(json, "[NIMRS_DATA] value=123"));
    assert(!contains(json, "Normal log"));

    // Test that normal filter doesn't pick up data logs
    String jsonNormal = Log.getLogsJSON("value=123");
    assert(!contains(jsonNormal, "value=123"));

    std::cout << "Passed." << std::endl;
}

int main() {
    test_initial_logs();
    test_add_logs();
    test_filter_logs();
    test_data_logs();
    return 0;
}
