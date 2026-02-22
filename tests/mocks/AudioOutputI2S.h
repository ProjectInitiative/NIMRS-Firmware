#ifndef AUDIO_OUTPUT_I2S_MOCK_H
#define AUDIO_OUTPUT_I2S_MOCK_H

#include "Arduino.h"

class AudioOutputI2S {
public:
  void SetPinout(int bclk, int lrclk, int dout) {}
  void SetOutputModeMono(bool mono) {}
  bool SetGain(float gain) { return true; }
};

#endif
