#ifndef CVREGISTRY_MOCK_H
#define CVREGISTRY_MOCK_H

#include <stdint.h>

namespace CV {
static constexpr uint16_t ADDR_SHORT = 1;
static constexpr uint16_t FRONT = 33;
static constexpr uint16_t REAR = 34;
static constexpr uint16_t AUX1 = 35;
static constexpr uint16_t AUX2 = 36;
static constexpr uint16_t AUX3 = 37;
static constexpr uint16_t AUX4 = 38;
static constexpr uint16_t AUX5 = 39;
static constexpr uint16_t AUX6 = 40;
static constexpr uint16_t AUX7 = 41;
static constexpr uint16_t AUX8 = 42;
}

struct CvDef {
  uint16_t id;
  const char *name;
  const char *desc;
};

const CvDef CV_DEFS[] = {{1, "Address", "DCC Address"}};
const size_t CV_DEFS_COUNT = 1;

#endif
