#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <stdint.h>
#include <map>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

extern std::map<int, int> _mockCVs;

class NmraDcc {
public:
  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return _mockCVs[cv]; }
  void setCV(int cv, int val) { _mockCVs[cv] = val; }
};

#endif
