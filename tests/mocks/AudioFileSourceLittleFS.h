#ifndef AUDIO_FILE_SOURCE_LITTLEFS_MOCK_H
#define AUDIO_FILE_SOURCE_LITTLEFS_MOCK_H

#include "Arduino.h"
#include "LittleFS.h"

class AudioFileSourceLittleFS {
public:
  File _file;

  AudioFileSourceLittleFS() {}

  AudioFileSourceLittleFS(const char *path) { open(path); }

  bool open(const char *path) {
    _file = LittleFS.open(path, "r");
    return _file;
  }

  virtual ~AudioFileSourceLittleFS() { _file.close(); }
};

#endif
