#include "../src/MotorHal.h"

MotorHal &MotorHal::getInstance() {
  static MotorHal instance;
  return instance;
}

MotorHal::MotorHal() {}
void MotorHal::init() {}
void MotorHal::setDuty(float duty) {}
size_t MotorHal::getAdcSamples(float *buffer, size_t maxLen) { return 0; }
float MotorHal::getAdcSampleRate() const { return 20000.0f; }
