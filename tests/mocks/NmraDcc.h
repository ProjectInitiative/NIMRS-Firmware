#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

struct DCC_MSG {
  uint8_t Size;
  uint8_t Data[6];
};

// Constants needed by DccController.cpp
#define MAN_ID_DIY 13
#define DCC_DIR_FWD 1
#define DCC_DIR_REV 0

// Function Group Constants
#define FN_BIT_00 0x01
#define FN_BIT_01 0x02
#define FN_BIT_02 0x04
#define FN_BIT_03 0x08
#define FN_BIT_04 0x10

enum { FN_0_4, FN_5_8, FN_9_12, FN_13_20, FN_21_28 };

class NmraDcc {
public:
  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return 0; }
  void setCV(int cv, int val) {}

  // New methods
  void pin(uint8_t extIntPin, uint8_t extIntNum, uint8_t pullup = 1) {}
  void init(uint8_t manId, uint8_t verId, uint8_t flags,
            uint8_t autoFactoryDefault) {}
  void process() {}
};

#endif
