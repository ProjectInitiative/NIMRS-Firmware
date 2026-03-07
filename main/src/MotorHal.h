#ifndef MOTOR_HAL_H
#define MOTOR_HAL_H

#include "esp_adc/adc_continuous.h"
#include <stddef.h>
#include <stdint.h>

class MotorHal {
public:
  static MotorHal &getInstance();

  // Initialization
  void init();

  // Motor Control
  // duty: -1.0 (Full Reverse) to 1.0 (Full Forward). 0.0 = Stop/Brake.
  void setDuty(float duty);

  // Hardware Configuration
  void setHardwareGain(uint8_t mode); // 0=Low, 1=High-Z, 2=High, 3=Med

  // Telemetry / Sensing
  // Copy latest ADC samples to the provided buffer.
  size_t getAdcSamples(float *buffer, size_t maxLen);

  // Get the configured ADC sample rate in Hz
  float getAdcSampleRate() const;

private:
  MotorHal();

  // Internal state
  float _sampleRate;
  uint8_t _lastGain;
  adc_continuous_handle_t _adcHandle;
};

#endif
