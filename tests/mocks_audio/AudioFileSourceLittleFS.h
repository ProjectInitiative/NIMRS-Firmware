#ifndef AUDIOFILESOURCE_LITTLEFS_MOCK_H
#define AUDIOFILESOURCE_LITTLEFS_MOCK_H

#include "AudioFileSource.h"
#include "LittleFS.h"
#include <Arduino.h>

class AudioFileSourceLittleFS : public AudioFileSource {
public:
  File _file;

  AudioFileSourceLittleFS(const char *path) {
    _file = LittleFS.open(path, "r");
  }
  virtual ~AudioFileSourceLittleFS() { _file.close(); }
  virtual bool isOpen() override { return _file; }
  virtual uint32_t getSize() override { return _file.size(); }
  virtual uint32_t getPos() override { return 0; }
  virtual uint32_t read(void *data, uint32_t len) override { return 0; }
  virtual bool seek(int32_t pos, int dir) override { return false; }
  virtual bool close() override { _file.close(); return true; }
};

#endif
