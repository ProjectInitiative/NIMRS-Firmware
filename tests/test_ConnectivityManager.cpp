// TEST_SOURCES: src/ConnectivityManager.cpp tests/mocks/mocks.cpp
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Mock everything before including the class under test
#define private public
#include "ConnectivityManager.h"
#undef private

#include "DccController.h"
#include "LittleFS.h"
#include "SystemContext.h"

// Simple test framework
#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

TEST_CASE(test_handleControl_stop) {
  ConnectivityManager cm;
  SystemContext &ctx = SystemContext::getInstance();
  SystemState &state = ctx.getState();

  state.speed = 100;

  // Simulate web server "plain" argument for "stop" action
  cm._server.args["plain"] = "{\"action\":\"stop\"}";

  cm.handleControl();

  assert(cm._server.lastCode == 200);
  assert(state.speed == 0);
  assert(state.speedSource == SOURCE_WEB);
}

TEST_CASE(test_handleControl_set_speed) {
  ConnectivityManager cm;
  SystemContext &ctx = SystemContext::getInstance();
  SystemState &state = ctx.getState();

  state.speed = 0;

  // Simulate web server "plain" argument for "set_speed" action
  cm._server.args["plain"] = "{\"action\":\"set_speed\",\"value\":100}";

  cm.handleControl();

  // map(100, 0, 126, 0, 255) = (100 * 255) / 126 = 202
  assert(cm._server.lastCode == 200);
  assert(state.speed == 202);
  assert(state.speedSource == SOURCE_WEB);
}

TEST_CASE(test_handleFileList) {
  ConnectivityManager cm;
  LittleFS.mockFiles.clear();
  LittleFS.mockFiles.push_back(File("test.wav", 1234));
  LittleFS.mockFiles.push_back(File("config.json", 56));

  cm.handleFileList();

  assert(cm._server.lastCode == 200);
}

TEST_CASE(test_handleCV_read) {
  ConnectivityManager cm;
  cm._server.args["plain"] = "{\"cmd\":\"read\",\"cv\":1}";

  cm.handleCV();

  assert(cm._server.lastCode == 200);
  assert(cm._server.lastContent.indexOf("\"cv\":1") != -1);
}

TEST_CASE(test_handleCvAll_read) {
  ConnectivityManager cm;
  cm._server._method = HTTP_GET;

  cm.handleCvAll();

  assert(cm._server.lastCode == 200);
  // Verify content is generated (Mock ArduinoJson now serializes data)
  assert(cm._server.lastContent.indexOf("\"1\":") != -1);
}

int main() {
  RUN_TEST(test_handleControl_stop);
  RUN_TEST(test_handleControl_set_speed);
  RUN_TEST(test_handleFileList);
  RUN_TEST(test_handleCV_read);
  RUN_TEST(test_handleCvAll_read);
  std::cout << "All tests passed!" << std::endl;
  return 0;
}
