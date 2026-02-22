#ifndef AUDIO_GENERATOR_MOCK_H
#define AUDIO_GENERATOR_MOCK_H

#include "Arduino.h"

class AudioFileSourceLittleFS;
class AudioOutputI2S;

class AudioGenerator {
public:
  virtual bool begin(AudioFileSourceLittleFS *source,
                     AudioOutputI2S *output) = 0;
  virtual bool isRunning() = 0;
  virtual bool loop() = 0;
  virtual bool stop() = 0;
  virtual ~AudioGenerator() {}
};

#endif
