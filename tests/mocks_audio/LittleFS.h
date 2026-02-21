#ifndef LITTLEFS_MOCK_AUDIO_H
#define LITTLEFS_MOCK_AUDIO_H

#include "Arduino.h"
#include "WebServer.h"
#include <vector>

class LittleFSClass {
public:
  static int callCount_exists;
  static int callCount_open;

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
    if (path == "/")
      return File("/", 0, true);
    for (const auto &f : mockFiles) {
      if (String(f.name()) == path) {
        return f;
      }
    }
    return File();
  }

  std::vector<File> mockFiles;
};

extern LittleFSClass LittleFS;

#endif
