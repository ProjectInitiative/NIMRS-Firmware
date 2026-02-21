#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

class NmraDcc {
public:
  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return 0; }
  void setCV(int cv, int val) {}
};

#endif
