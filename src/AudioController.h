#ifndef AUDIO_CONTROLLER_H
#define AUDIO_CONTROLLER_H

#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <map>
#include <vector>

// Forward declarations
class AudioOutputI2S;
class AudioGenerator; // Base class
class AudioFileSourceLittleFS;

struct SoundAsset {
  uint8_t id;
  String name;
  String type; // "simple", "complex_loop", "toggle"
  String fileIntro;
  String fileLoop;
  String fileOutro;
};

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

  void loadAssets();
  const SoundAsset *getAsset(uint8_t id);

private:
  AudioController();

  AudioOutputI2S *_out;
  AudioGenerator *_generator; // Polymorphic generator (WAV or MP3)
  AudioFileSourceLittleFS *_file;

  bool _playing = false;
  std::map<uint8_t, SoundAsset> _assets;
  bool _lastFunctions[29] = {false};
};

#endif
