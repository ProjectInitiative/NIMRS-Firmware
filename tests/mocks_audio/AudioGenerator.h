#ifndef AUDIOGENERATOR_H
#define AUDIOGENERATOR_H
#include "AudioOutputI2S.h"
#include "AudioFileSource.h"

class AudioGenerator {
public:
  virtual bool begin(AudioFileSource *source, AudioOutputI2S *out) { return true; }
  virtual bool loop() { return true; }
  virtual bool stop() { return true; }
  virtual bool isRunning() { return true; }
  virtual ~AudioGenerator() {}
};
#endif
