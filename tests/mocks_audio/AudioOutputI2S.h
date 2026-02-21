#ifndef AUDIOOUTPUTI2S_H
#define AUDIOOUTPUTI2S_H
#include <stdint.h>
class AudioOutputI2S {
public:
  void SetPinout(int, int, int) {}
  void SetOutputModeMono(bool) {}
  void SetGain(float) {}
  bool begin() { return true; }
};
#endif
