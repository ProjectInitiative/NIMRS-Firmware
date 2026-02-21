#include "BemfEstimator.h"
#include <cmath>

BemfEstimator::BemfEstimator()
    : _rArmature(2.0f), // Default
      _poles(5), _vApplied(0.0f), _iAvg(0.0f), _rippleFreq(0.0f), _vBemf(0.0f),
      _estimatedRpm(0.0f), _bemfConstant(0.002f), // Default guess: 0.002 V/RPM
      _bemfKFilter(0.01f),                        // Slow learning
      _useRipple(false) {
  _bemfKFilter.reset(_bemfConstant);
}

void BemfEstimator::setMotorParams(float rArmature, int poles) {
  if (rArmature > 0.0f)
    _rArmature = rArmature;
  if (poles > 0)
    _poles = poles;
}

void BemfEstimator::updateLowSpeedData(float vApplied, float iAvg) {
  _vApplied = vApplied;
  _iAvg = iAvg;
}

void BemfEstimator::updateRippleFreq(float freqHz) { _rippleFreq = freqHz; }

void BemfEstimator::calculateEstimate() {
  // 1. Calculate BEMF Voltage (Low Speed Model)
  // V_bemf = V_app - I * R
  float vDrop = _iAvg * _rArmature;
  _vBemf = _vApplied - vDrop;
  if (_vBemf < 0.0f)
    _vBemf = 0.0f;

  // 2. Calculate Ripple RPM (High Speed Model)
  // 2 * poles commutations per rev
  float rippleRpm = 0.0f;
  if (_rippleFreq > 0.0f) {
    rippleRpm = (_rippleFreq * 60.0f) / (2.0f * _poles);
  }

  // 3. Learning the BEMF Constant (Kv)
  // Only learn if RPM > 500 (stable ripple) and V_bemf > 1.0V
  if (rippleRpm > 500.0f && _vBemf > 1.0f) {
    float instantaneousK = _vBemf / rippleRpm;
    if (instantaneousK > 0.0001f && instantaneousK < 0.01f) {
      _bemfConstant = _bemfKFilter.update(instantaneousK);
    }
  }

  // 4. Fusion Strategy
  float estimatedRpmFromBemf = 0.0f;
  if (_bemfConstant > 0.0f) {
    estimatedRpmFromBemf = _vBemf / _bemfConstant;
  }

  // Hysteresis
  if (rippleRpm > 600.0f) {
    _estimatedRpm = rippleRpm;
    _useRipple = true;
  } else if (rippleRpm < 400.0f) {
    _estimatedRpm = estimatedRpmFromBemf;
    _useRipple = false;
  } else {
    // Crossover region
    if (_useRipple)
      _estimatedRpm = rippleRpm;
    else
      _estimatedRpm = estimatedRpmFromBemf;
  }
}

float BemfEstimator::getEstimatedRpm() const { return _estimatedRpm; }

float BemfEstimator::getBemfVoltage() const { return _vBemf; }

bool BemfEstimator::isStalled() const {
  return (_vApplied > 2.0f && _estimatedRpm < 10.0f);
}
