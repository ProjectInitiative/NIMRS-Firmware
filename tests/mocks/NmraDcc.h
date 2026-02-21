#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

#define DCC_DIR_FWD 1
#define DCC_128_STEPS 128
#define DCC_ADDR_SHORT 1
#define FN_BIT_00 1
#define FN_BIT_01 2
#define FN_BIT_02 4
#define FN_BIT_03 8
#define FN_BIT_04 16
#define FN_0_4 0
#define FN_5_8 1
#define FN_9_12 2
#define FN_13_20 3
#define FN_21_28 4
#define MAN_ID_DIY 13

class NmraDcc {
public:
  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return 0; }
  void setCV(int cv, int val) {}
  void pin(uint8_t pin, uint8_t pullup) {}
  void init(uint8_t manId, uint8_t version, uint8_t flags, uint8_t something) {}
  void process() {}
};

struct DCC_MSG {
  uint8_t Size;
  uint8_t Data[6];
};

#endif
