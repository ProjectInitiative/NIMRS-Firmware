#ifndef AUDIOFILESOURCE_H
#define AUDIOFILESOURCE_H
#include <stdint.h>
class AudioFileSource {
public:
  virtual ~AudioFileSource() {}
  virtual bool isOpen() { return false; }
  virtual uint32_t getSize() = 0;
  virtual uint32_t getPos() = 0;
  virtual uint32_t read(void *data, uint32_t len) = 0;
  virtual bool seek(int32_t pos, int dir) = 0;
  virtual bool close() = 0;
};
#endif
