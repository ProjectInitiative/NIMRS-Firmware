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

TEST_CASE(test_malicious_upload) {
  ConnectivityManager cm;
  LittleFS.lastOpenedPath = "";

  // Simulate UPLOAD_FILE_START with malicious file
  cm._server._upload.status = UPLOAD_FILE_START;
  cm._server._upload.filename = "malicious.html";

  cm.handleFileUpload();

  // Should NOT have opened /malicious.html
  assert(LittleFS.lastOpenedPath != "/malicious.html");
}

TEST_CASE(test_path_traversal) {
  ConnectivityManager cm;
  LittleFS.lastOpenedPath = "";

  // Simulate UPLOAD_FILE_START with path traversal
  cm._server._upload.status = UPLOAD_FILE_START;
  cm._server._upload.filename = "../config.json";

  cm.handleFileUpload();

  // Should NOT have opened file
  assert(LittleFS.lastOpenedPath != "../config.json");
  assert(LittleFS.lastOpenedPath != "/../config.json");
}

TEST_CASE(test_valid_upload) {
  ConnectivityManager cm;
  LittleFS.lastOpenedPath = "";

  // Simulate UPLOAD_FILE_START with valid file
  cm._server._upload.status = UPLOAD_FILE_START;
  cm._server._upload.filename = "sound.wav";

  cm.handleFileUpload();

  // Should HAVE opened /sound.wav
  assert(LittleFS.lastOpenedPath == "/sound.wav");
}

int main() {
  RUN_TEST(test_malicious_upload);
  RUN_TEST(test_path_traversal);
  RUN_TEST(test_valid_upload);
  std::cout << "Security tests passed!" << std::endl;
  return 0;
}
