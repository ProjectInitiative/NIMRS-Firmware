// clang-format off
// TEST_SOURCES: src/ConnectivityManager.cpp src/BootLoopDetector.cpp tests/mocks/mocks.cpp
// clang-format on

#include <cassert>
#include <iostream>
#include <vector>

// Access private members for testing
#define private public
#include "ConnectivityManager.h"
#undef private

#include "LittleFS.h"
#include "SystemContext.h"

// Simple test framework
#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

TEST_CASE(test_redundant_check) {
  // Setup
  LittleFS.mockFiles.clear();
  LittleFS.callCount_exists = 0;
  LittleFS.callCount_open = 0;

  // Add a mock file
  LittleFS.mockFiles.push_back(File("/index.html", 100));

  ConnectivityManager cm;
  cm._server._uri = "/index.html";

  // Act
  cm.handleStaticFile();

  // Assert
  std::cout << "  callCount_exists: " << LittleFS.callCount_exists << std::endl;
  std::cout << "  callCount_open: " << LittleFS.callCount_open << std::endl;

  // Expect 0 exists calls, 1 open call
  assert(LittleFS.callCount_exists == 0);
  assert(LittleFS.callCount_open == 1);
  assert(cm._server.lastCode != 404);
}

TEST_CASE(test_file_not_found) {
  // Setup
  LittleFS.mockFiles.clear();
  LittleFS.callCount_exists = 0;
  LittleFS.callCount_open = 0;

  ConnectivityManager cm;
  cm._server._uri = "/missing.html";

  // Act
  cm.handleStaticFile();

  // Assert
  std::cout << "  callCount_exists: " << LittleFS.callCount_exists << std::endl;
  std::cout << "  callCount_open: " << LittleFS.callCount_open << std::endl;

  // Expect 0 exists calls, 1 open call (which returns invalid file)
  assert(LittleFS.callCount_exists == 0);
  assert(LittleFS.callCount_open == 1);
  assert(cm._server.lastCode == 404);
}

int main() {
  RUN_TEST(test_redundant_check);
  RUN_TEST(test_file_not_found);
  std::cout << "All tests passed!" << std::endl;
  return 0;
}
