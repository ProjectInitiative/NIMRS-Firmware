#ifndef UPDATE_H
#define UPDATE_H

#include "Arduino.h"
#include <stddef.h>
#include <stdint.h>

#define U_FLASH 0
#define U_SPIFFS 100
#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class UpdateClass {
public:
  bool begin(size_t size = 0, int command = 0) { return true; }
  size_t write(uint8_t *data, size_t len) { return len; }
  size_t writeStream(Stream &data) { return 0; }
  bool end(bool evenIfRemaining = false) { return true; }
  void printError(Stream &out) {}
  bool hasError() { return false; }
  size_t remaining() { return 0; }
  bool isFinished() { return true; }
  void onProgress(void (*fn)(size_t, size_t)) {}
};

extern UpdateClass Update;

#endif
