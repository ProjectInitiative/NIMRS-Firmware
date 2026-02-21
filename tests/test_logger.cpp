#include "Arduino.h"
#include "ArduinoJson.h"
#include "Logger.h"
#include <cassert>
#include <iostream>
#include <string>

// Test Helper Macros
#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

TEST_CASE(test_logger_structure) {
  String json = Log.getLogsJSON();
  // Should be array
  assert(json.startsWith("["));
  assert(json.endsWith("]"));
}

TEST_CASE(test_logger_add_and_retrieve) {
  String uniqueMsg = "UNIQUE_MSG_TEST_123";
  Log.println(uniqueMsg);

  String json = Log.getLogsJSON();

  // Check if json contains the message
  // json format is ["...[timestamp] UNIQUE_MSG_TEST_123..."]
  assert(json.indexOf(uniqueMsg) != -1);
}

TEST_CASE(test_logger_filter) {
  String msg1 = "FILTER_MATCH_THIS";
  String msg2 = "FILTER_IGNORE_THIS";

  Log.println(msg1);
  Log.println(msg2);

  String json = Log.getLogsJSON("FILTER_MATCH");

  assert(json.indexOf(msg1) != -1);
  assert(json.indexOf(msg2) == -1);
}

TEST_CASE(test_logger_data_filter) {
  // Telemetry data
  String dataMsg = "[NIMRS_DATA] Some data";
  Log.println(dataMsg);

  String json = Log.getLogsJSON("[NIMRS_DATA]");

  assert(json.indexOf("Some data") != -1);

  // Ensure normal logs are not here
  String normalMsg = "NORMAL_LOG_SHOULD_NOT_BE_IN_DATA";
  Log.println(normalMsg);

  json = Log.getLogsJSON("[NIMRS_DATA]");
  assert(json.indexOf(normalMsg) == -1);
}

int main() {
  // Initialize Logger (mock serial begin)
  Log.begin(115200);

  RUN_TEST(test_logger_structure);
  RUN_TEST(test_logger_add_and_retrieve);
  RUN_TEST(test_logger_filter);
  RUN_TEST(test_logger_data_filter);

  return 0;
}
