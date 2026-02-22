// clang-format off
// TEST_SOURCES: src/ConnectivityManager.cpp src/BootLoopDetector.cpp tests/mocks/mocks.cpp
// TEST_FLAGS: -DSKIP_MOCK_CONNECTIVITY_MANAGER
// clang-format on
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
#include "WebAssets.h"

// Simple test framework
#define TEST_CASE(name) void name()
#define RUN_TEST(name)                                                         \
  std::cout << "Running " << #name << "... ";                                  \
  name();                                                                      \
  std::cout << "PASSED" << std::endl;

TEST_CASE(test_handleFileUpload_valid_mp3) {
  ConnectivityManager cm;

  // Clear mock state
  LittleFS.callCount_open = 0;
  LittleFS.lastOpenedPath = "";

  // Setup upload
  cm._server._upload.filename = "test.mp3";
  cm._server._upload.status = UPLOAD_FILE_START;

  // Call the handler
  cm.handleFileUpload();

  // Expect file to be opened
  if (LittleFS.callCount_open != 1) {
    std::cout << "FAILED: LittleFS.open not called" << std::endl;
    exit(1);
  }
  if (LittleFS.lastOpenedPath != "/test.mp3") {
    std::cout << "FAILED: Unexpected path: " << LittleFS.lastOpenedPath.c_str()
              << std::endl;
    exit(1);
  }
}

int main() {
  RUN_TEST(test_handleFileUpload_valid_mp3);
  return 0;
}
