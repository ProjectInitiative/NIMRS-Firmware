#ifndef AUDIO_CONTROLLER_H
#define AUDIO_CONTROLLER_H

#include "SystemContext.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <map>
#include <vector>

#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"

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

private:
  AudioController();

  I2SStream *_i2s;
  VolumeStream *_volume;
  EncodedAudioStream *_decoder;
  MP3DecoderHelix *_mp3;
  WAVDecoder *_wav;
  StreamCopy *_copier;
  File _file; // Using LittleFS File

  bool _playing = false;
  std::map<uint8_t, SoundAsset> _assets;
  bool _lastFunctions[29] = {false};
};

#endif
