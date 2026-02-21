#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>
#include <map>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

// DCC Constants
#define DCC_DIR_FWD 1
#define DCC_DIR_REV 0

// Manufacturer ID
#define MAN_ID_DIY 13

// Bit masks for Function Groups
#define FN_BIT_00 0x10
#define FN_BIT_01 0x01
#define FN_BIT_02 0x02
#define FN_BIT_03 0x04
#define FN_BIT_04 0x08

// Function Groups
#define FN_0_4   0
#define FN_5_8   1
#define FN_9_12  2
#define FN_13_20 3
#define FN_21_28 4

// Mock for NmraDcc
class NmraDcc {
public:
  // State for verification
  static std::map<int, int> cvs;
  static int lastPin;
  static int lastPullup;
  static bool initCalled;
  static bool processCalled;

  static void reset() {
    cvs.clear();
    lastPin = -1;
    lastPullup = -1;
    initCalled = false;
    processCalled = false;
  }

  uint16_t getAddr() { return 3; }

  int getCV(int cv) {
    if (cvs.find(cv) != cvs.end()) return cvs[cv];
    return 0;
  }

  void setCV(int cv, int val) {
    cvs[cv] = val;
  }

  // New methods
  void pin(int pin, int pullup) {
    lastPin = pin;
    lastPullup = pullup;
  }

  void init(uint8_t manuId, uint8_t ver, uint8_t flags, uint8_t reserved) {
    initCalled = true;
  }

  void process() {
    processCalled = true;
  }
};

// Define statics in a cpp file or header if using C++17 inline variables
// Since this is a header-only mock used in multiple tests, we should use inline or define in test cpp
// But existing tests include this header.
// Use inline variables if C++17 (Makefile says -std=c++17)

inline std::map<int, int> NmraDcc::cvs;
inline int NmraDcc::lastPin = -1;
inline int NmraDcc::lastPullup = -1;
inline bool NmraDcc::initCalled = false;
inline bool NmraDcc::processCalled = false;

struct DCC_MSG {
  uint8_t Size;
  uint8_t Data[6];
};

#endif
