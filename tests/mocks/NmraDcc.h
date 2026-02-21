#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <iostream>
#include <map>
#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

class NmraDcc {
public:
  std::map<int, int> _cvs;
  uint16_t getAddr() { return 3; }
  int getCV(int cv) { return _cvs[cv]; }
  void setCV(int cv, int val) { _cvs[cv] = val; }
  void reset() { _cvs.clear(); }
};

#endif
