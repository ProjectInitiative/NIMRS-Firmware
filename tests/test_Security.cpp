// clang-format off
// TEST_SOURCES: src/ConnectivityManager.cpp src/BootLoopDetector.cpp tests/mocks/mocks.cpp
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

TEST_CASE(test_case_insensitive_upload) {
  ConnectivityManager cm;
  LittleFS.lastOpenedPath = "";

  // Simulate UPLOAD_FILE_START with uppercase extension
  cm._server._upload.status = UPLOAD_FILE_START;
  cm._server._upload.filename = "sound.WAV";

  cm.handleFileUpload();

  // Should HAVE opened /sound.WAV
  assert(LittleFS.lastOpenedPath == "/sound.WAV");
}

TEST_CASE(test_null_byte_injection) {
  ConnectivityManager cm;
  LittleFS.lastOpenedPath = "";

  // Simulate UPLOAD_FILE_START with null byte injection
  // "malicious.html" + '\0' + ".json"
  std::string malicious = "malicious.html";
  malicious += '\0';
  malicious += ".json";

  cm._server._upload.status = UPLOAD_FILE_START;
  cm._server._upload.filename = String(malicious);

  cm.handleFileUpload();

  // The upload should be blocked because of the null byte.
  // If it was allowed, lastOpenedPath would be set.
  if (LittleFS.lastOpenedPath != "") {
    std::cout << "FAILED: Expected blocked upload, but opened: "
              << LittleFS.lastOpenedPath << std::endl;
  }
  assert(LittleFS.lastOpenedPath == "");
}

TEST_CASE(test_trailing_null_byte_upload) {
  ConnectivityManager cm;
  LittleFS.lastOpenedPath = "";

  // Simulate UPLOAD_FILE_START with a valid file having a trailing null byte
  // This simulates the reported issue where "Null byte detected" blocks valid
  // uploads
  std::string valid_with_null("test.mp3\0", 9);

  cm._server._upload.status = UPLOAD_FILE_START;
  cm._server._upload.filename = String(valid_with_null);

  cm.handleFileUpload();

  // With the fix, this should PASS (opened as /test.mp3)
  assert(LittleFS.lastOpenedPath == "/test.mp3");
}

int main() {
  RUN_TEST(test_malicious_upload);
  RUN_TEST(test_path_traversal);
  RUN_TEST(test_valid_upload);
  RUN_TEST(test_case_insensitive_upload);
  RUN_TEST(test_null_byte_injection);
  RUN_TEST(test_trailing_null_byte_upload);
  std::cout << "Security tests passed!" << std::endl;
  return 0;
}
