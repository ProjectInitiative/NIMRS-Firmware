#ifndef CVREGISTRY_MOCK_H
#define CVREGISTRY_MOCK_H

namespace CV {
static constexpr uint16_t V_START = 2;
static constexpr uint16_t ACCEL = 3;
static constexpr uint16_t PWM_DITHER = 64;
static constexpr uint16_t PEDESTAL_FLOOR = 57;
static constexpr uint16_t LOAD_GAIN_SCALAR = 146;
static constexpr uint16_t LEARN_THRESHOLD = 147;
static constexpr uint16_t TRACK_VOLTAGE = 145;
static constexpr uint16_t MOTOR_R_ARM = 149;
} // namespace CV

struct CvDef {
  uint16_t id;
  const char *name;
  const char *desc;
};

const CvDef CV_DEFS[] = {{1, "Address", "DCC Address"}};
const size_t CV_DEFS_COUNT = 1;

#endif
