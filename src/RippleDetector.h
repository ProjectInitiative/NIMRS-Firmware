#ifndef RIPPLE_DETECTOR_H
#define RIPPLE_DETECTOR_H

#include "DspFilters.h"
#include <cstdint>
#include <stddef.h>

class RippleDetector {
public:
  RippleDetector();
  void processBuffer(float *data, size_t len, float sampleRate);
  float getFrequency() const;
  void reset();

private:
  DcBlocker _dcBlocker;
  bool _state; // true = above threshold, false = below
  float _thresholdHigh;
  float _thresholdLow;

  // Frequency tracking
  uint32_t _samplesSincePulse;
  float _currentFreq;
  EmaFilter _freqFilter;
};

#endif
