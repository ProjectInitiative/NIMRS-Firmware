#include "../../main/src/MotorHal.h"
#include "../simulator/MotorSimulator.h"
#include <cstring>
#include <vector>

// Singleton/Global for the simulator to be accessed by the test harness
static MotorSimulator *g_sim = nullptr;

void setGlobalMotorSimulator(MotorSimulator *sim) { g_sim = sim; }

MotorHal &MotorHal::getInstance() {
  static MotorHal instance;
  return instance;
}

MotorHal::MotorHal() : _lastGain(1), _currentDuty(0.0f) {}

void MotorHal::init() {
  // No-op in simulation
}

void MotorHal::setDuty(float duty) {
  _currentDuty = duty;
  // In a real system, this updates MCPWM.
  // Here, the test harness will step the simulator using this duty.
}

size_t MotorHal::getAdcSamples(float *buffer, size_t maxLen) {
  if (!g_sim)
    return 0;

  // Mimic the 20kHz sampling of the ESP32
  g_sim->getAdcSamples(buffer, maxLen, getAdcSampleRate());
  return maxLen;
}

float MotorHal::getAdcSampleRate() const { return 20000.0f; }

float MotorHal::getCurrentScalar() const {
  // In simulation, we assume the simulator returns actual Amps,
  // so we return 1.0 here and bypass the ADC volt/gain math
  return 1.0f;
}

bool MotorHal::readFault() { return false; }

void MotorHal::setHardwareGain(uint8_t mode) { _lastGain = mode; }

float MotorHal::getLatestCurrentAdc() const {
  if (!g_sim)
    return 0.0f;
  return g_sim->getState().current;
}
