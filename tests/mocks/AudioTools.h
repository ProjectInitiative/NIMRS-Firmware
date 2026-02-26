#pragma once

#include "Arduino.h"
#include <LittleFS.h>

#define TX_MODE 1

struct I2SConfig {
  int pin_bck;
  int pin_ws;
  int pin_data;
  int channels;
  int sample_rate;
};

class AudioStream {
public:
  virtual ~AudioStream() = default;
};

class I2SStream : public AudioStream {
public:
  I2SStream() {}
  I2SConfig defaultConfig(int mode) { return I2SConfig(); }
  void begin(I2SConfig &config) {}
};

class VolumeStream : public AudioStream {
public:
  VolumeStream(I2SStream &stream) {}
  void begin(I2SConfig &config) {}
  void setVolume(float volume) {}
};

class AudioDecoder {
public:
  virtual ~AudioDecoder() = default;
};

class MP3DecoderHelix : public AudioDecoder {
public:
  MP3DecoderHelix() {}
};

class WAVDecoder : public AudioDecoder {
public:
  WAVDecoder() {}
};

class EncodedAudioStream : public AudioStream {
public:
  EncodedAudioStream(VolumeStream *out, AudioDecoder *dec) {}
  void setDecoder(AudioDecoder *dec) {}
  void begin() {}
  void end() {}
};

class StreamCopy {
public:
  StreamCopy(EncodedAudioStream &out, File &in) {}
  void begin(EncodedAudioStream &out, File &in) {}
  bool copy() { return false; }
};
