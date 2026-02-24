#include "RippleDetector.h"
#include <cmath>

RippleDetector::RippleDetector()
    : _dcBlocker(0.9f), _state(false), _thresholdHigh(0.05f),
      _thresholdLow(-0.05f), _samplesSincePulse(0), _currentFreq(0.0f),
      _freqFilter(0.3f) {}

void RippleDetector::processBuffer(float *data, size_t len, float sampleRate) {
  if (len == 0 || sampleRate <= 0.0f)
    return;
  float sampleIntervalUs = 1000000.0f / sampleRate;

  for (size_t i = 0; i < len; i++) {
    _samplesSincePulse++;

    // 1. Remove DC Bias
    float sample = _dcBlocker.process(data[i]);

    // 2. Schmitt Trigger
    if (!_state && sample > _thresholdHigh) {
      _state = true;

      // Rising edge
      float dt = _samplesSincePulse * sampleIntervalUs;
      _samplesSincePulse = 0;

      // Filter noise: assume max 500Hz -> 2ms min period = 2000us
      if (dt > 2000.0f && dt < 200000.0f) {
        float instFreq = 1000000.0f / dt;
        _currentFreq = _freqFilter.update(instFreq);
      }
    } else if (_state && sample < _thresholdLow) {
      _state = false;
    }
  }
}

float RippleDetector::getFrequency() const {
  // Decay if no pulse
  // This is tricky as we don't know how much time passed outside processBuffer
  // We can assume getFrequency is called frequently.
  // Or we just return the last valid freq.
  // Let's just return current freq. The caller handles timeout if needed (by
  // checking timestamp?) But here we don't have global time. Ideally we should
  // decay it if _samplesSincePulse is huge. For now, let's leave it as last
  // detected freq.
  return _currentFreq;
}

void RippleDetector::reset() {
  _dcBlocker.reset();
  _state = false;
  _currentFreq = 0.0f;
  _samplesSincePulse = 0;
}
