#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>
#include <map>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

// Constants from NmraDcc.h (usually)
#define MAN_ID_DIY 13
#define DCC_DIR_FWD 1
#define DCC_DIR_REV 0

// Function Group Defines
#define FN_0_4 0
#define FN_5_8 1
#define FN_9_12 2
#define FN_13_20 3
#define FN_21_28 4

#define FN_BIT_00 0x10
#define FN_BIT_01 0x01
#define FN_BIT_02 0x02
#define FN_BIT_03 0x04
#define FN_BIT_04 0x08

struct DCC_MSG {
  uint8_t dummy;
};

class NmraDcc {
public:
  // State for verification
  int initCalledCount = 0;
  int pinCalledCount = 0;
  int processCalledCount = 0;
  int lastSetCvId = -1;
  int lastSetCvVal = -1;
  uint8_t pinArg = 0;

  std::map<int, uint8_t> _cvs;

  NmraDcc() {
      // Default version for auto-reset check
      _cvs[7] = 123; // Just a random version
  }

  void pin(uint8_t p, uint8_t isr) {
      pinCalledCount++;
      pinArg = p;
  }

  void init(uint8_t manufId, uint8_t version, uint8_t flags, uint8_t ops) {
      initCalledCount++;
  }

  void process() {
      processCalledCount++;
  }

  uint16_t getAddr() { return 3; }

  int getCV(int cv) {
      if (_cvs.find(cv) != _cvs.end()) return _cvs[cv];
      return 0;
  }

  void setCV(int cv, int val) {
      _cvs[cv] = val;
      lastSetCvId = cv;
      lastSetCvVal = val;
  }

  // Helper to verify calls
  void resetMock() {
      initCalledCount = 0;
      pinCalledCount = 0;
      processCalledCount = 0;
      lastSetCvId = -1;
      lastSetCvVal = -1;
      pinArg = 0;
      _cvs.clear();
  }
};

#endif
