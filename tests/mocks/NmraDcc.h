#ifndef NMRADCC_MOCK_H
#define NMRADCC_MOCK_H

#include <map>
#include <stdint.h>

typedef uint8_t DCC_ADDR_TYPE;
typedef uint8_t DCC_DIRECTION;
typedef uint8_t DCC_SPEED_STEPS;
typedef uint8_t FN_GROUP;

class NmraDcc {
public:
  uint16_t getAddr() { return 3; }
  int getCV(int cv) {
    if (_cvs.find(cv) != _cvs.end()) {
      return _cvs[cv];
    }
    return 0;
  }
  void setCV(int cv, int val) { _cvs[cv] = val; }
  void reset() { _cvs.clear(); }

private:
  std::map<int, int> _cvs;
};

#endif
