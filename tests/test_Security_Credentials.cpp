// TEST_SOURCES: src/ConnectivityManager.cpp tests/mocks/mocks.cpp
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Mock everything before including the class under test
#define private public
#include "ConnectivityManager.h"
#undef private

#include "Arduino.h"
#include "DccController.h"
#include "LittleFS.h"
#include "SystemContext.h"

// Simple test framework
void run_test(const char *name, void (*test_func)()) {
  std::cout << "Running " << name << "... ";
  mockLogBuffer.clear();
  test_func();
  std::cout << "PASSED" << std::endl;
}

void test_handleStatus_no_credentials() {
  ConnectivityManager cm;
  // Initialize context with some values
  // Note: SystemContext is a singleton, so it persists across tests if not
  // reset
  SystemContext::getInstance().getState().wifiConnected = true;

  // Simulate call
  cm.handleStatus();

  std::string response = cm._server.lastContent.c_str();

  // Log response for debugging if needed
  std::cout << "Response: " << response << std::endl;

  // Check that sensitive keys are NOT present
  // "pass" is substring of "password", so checking "pass" might be too broad if
  // we have "compass" or something But checking "\"pass\"" (JSON key) is safer.
  assert(response.find("\"pass\"") == std::string::npos);
  assert(response.find("\"password\"") == std::string::npos);
  assert(response.find("\"web_pass\"") == std::string::npos);

  // Verify expected fields are present to ensure we got a valid JSON response
  assert(response.find("\"wifi\":true") != std::string::npos);
  assert(response.find("\"address\"") != std::string::npos);
  assert(response.find("\"hostname\"") != std::string::npos);
}

void test_handleWifiSave_no_credentials_log() {
  ConnectivityManager cm;

  std::string secretPass = "SecretPassword123";
  std::string ssid = "TestNetwork";

  // Simulate request
  cm._server.args["ssid"] = String(ssid.c_str());
  cm._server.args["pass"] = String(secretPass.c_str());

  cm.handleWifiSave();

  // Check logs
  std::string logs = mockLogBuffer;
  // std::cout << "Logs: " << logs << std::endl;

  // Verify SSID is logged (as per current implementation)
  assert(logs.find(ssid) != std::string::npos);

  // Verify Password is NOT logged
  if (logs.find(secretPass) != std::string::npos) {
    std::cerr << "FAILED: Password found in logs!" << std::endl;
    std::cerr << "Logs content: " << logs << std::endl;
    exit(1);
  }
}

int main() {
  run_test("test_handleStatus_no_credentials",
           test_handleStatus_no_credentials);
  run_test("test_handleWifiSave_no_credentials_log",
           test_handleWifiSave_no_credentials_log);
  std::cout << "Security Credentials tests passed!" << std::endl;
  return 0;
}
