#ifndef MOTOR_HAL_H
#define MOTOR_HAL_H

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

  // Telemetry / Sensing
  // Copy latest ADC samples to the provided buffer.
  // Returns number of samples copied.
  size_t getAdcSamples(float *buffer, size_t maxLen);

  // Get the configured ADC sample rate in Hz
  float getAdcSampleRate() const;

  // Direct voltage reading (for low speed average)
  // Or we compute average from buffer?
  // Let's expose raw average too for convenience if needed,
  // but the Estimator will likely calculate it from the buffer to be
  // consistent.

private:
  MotorHal();

  // Internal state
  float _sampleRate;
};

#endif
