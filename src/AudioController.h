#ifndef AUDIO_CONTROLLER_H
#define AUDIO_CONTROLLER_H

#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>

// Forward declarations
class AudioOutputI2S;
class AudioGenerator; // Base class
class AudioFileSourceLittleFS;

class AudioController {
public:
  static AudioController &getInstance() {
    static AudioController instance;
    return instance;
  }

  void setup();
  void loop();

  void playFile(const char *filename);
  void stop();

private:
  AudioController();

  AudioOutputI2S *_out;
  AudioGenerator *_generator; // Polymorphic generator (WAV or MP3)
  AudioFileSourceLittleFS *_file;

  bool _playing = false;
};

#endif
