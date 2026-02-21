#ifndef LITTLEFS_MOCK_H
#define LITTLEFS_MOCK_H

#include "Arduino.h"
#include "WebServer.h" // For File definition
#include <vector>

class LittleFSClass {
public:
  bool begin(bool format = true) { return true; }
  size_t totalBytes() { return 1000; }
  size_t usedBytes() { return 100; }
  bool exists(const String &path) { return false; }
  bool remove(const String &path) { return true; }
  File open(const String &path, const char *mode = "r") {
    lastOpenedPath = path; // Track for testing
    if (path == "/")
      return File("/", 0, true);
    // Return a valid file for testing write operations if needed,
    // or just return invalid to prevent writes.
    // For our security test, checking lastOpenedPath is enough.
    return File();
  }

  // Mock file system
  std::vector<File> mockFiles;
  String lastOpenedPath; // Added for testing
};

extern LittleFSClass LittleFS;

#endif
