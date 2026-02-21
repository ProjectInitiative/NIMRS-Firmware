#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

// Enums and Constants used by DccController
#define MAN_ID_DIY 13
#define FLAGS_AUTO_FACTORY_DEFAULT 0x02

// Direction
#define DCC_DIR_FWD 1
#define DCC_DIR_REV 0

// Function Groups (simplified mock values)
enum { FN_0_4, FN_5_8, FN_9_12, FN_13_20, FN_21_28 };

// Function Bits
#define FN_BIT_00 0x01
#define FN_BIT_01 0x02
#define FN_BIT_02 0x04
#define FN_BIT_03 0x08
#define FN_BIT_04 0x10

struct DCC_MSG {
  // Add members if accessed by notifyDccMsg
};

class NmraDcc {
public:
  void pin(int pin, int interrupt) {}
  void init(uint8_t manId, uint8_t version, uint8_t flags, uint8_t unk) {}
  void process() {}

  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return 0; } // Mock value
  void setCV(int cv, int val) {}
};

#endif
