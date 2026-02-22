#ifndef AUDIO_GENERATOR_MP3_MOCK_H
#define AUDIO_GENERATOR_MP3_MOCK_H

#include "AudioGenerator.h"

class AudioGeneratorMP3 : public AudioGenerator {
public:
  bool _running = false;

  bool begin(AudioFileSourceLittleFS *source, AudioOutputI2S *output) override {
    _running = true;
    return true;
  }
  bool isRunning() override { return _running; }
  bool loop() override { return true; }
  bool stop() override {
    _running = false;
    return true;
  }
};

#endif
