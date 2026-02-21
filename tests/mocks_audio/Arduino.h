#ifndef ARDUINO_MOCK_AUDIO_H
#define ARDUINO_MOCK_AUDIO_H

#include "../mocks/Arduino.h"

#define HIGH 1
#define LOW 0
#define OUTPUT 1
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}

#endif
