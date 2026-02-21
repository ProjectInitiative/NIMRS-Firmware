#ifndef LITTLEFS_MOCK_H
#define LITTLEFS_MOCK_H

#include "Arduino.h"
#include "WebServer.h" // For File definition
#include <vector>

class LittleFSClass {
public:
  int callCount_exists = 0;
  int callCount_open = 0;

  bool begin(bool format = true) { return true; }
  size_t totalBytes() { return 1000; }
  size_t usedBytes() { return 100; }

  bool exists(const String &path) {
    callCount_exists++;
    for (const auto &f : mockFiles) {
      if (String(f.name()) == path)
        return true;
    }
    return false;
  }

  bool remove(const String &path) { return true; }

  File open(const String &path, const char *mode = "r") {
    callCount_open++;
    lastOpenedPath = path; // Track for testing

    if (path == "/")
      return File("/", 0, true);

    for (auto &f : mockFiles) {
      if (String(f.name()) == path)
        return f;
    }

    return File();
  }

  // Mock file system
  std::vector<File> mockFiles;
  String lastOpenedPath; // Added for testing
};

extern LittleFSClass LittleFS;

#endif
