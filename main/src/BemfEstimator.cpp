#include "BemfEstimator.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

BemfEstimator::BemfEstimator()
    : _rArmature(35.0f), _poles(5), _vApplied(0.0f), _iAvg(0.0f),
      _rippleFreq(0.0f), _vBemf(0.0f), _estimatedRpm(0.0f),
      _bemfConstant(0.015f), // Default guess: 15mV/RPM
      _bemfKFilter(0.001f),  // Slow learning for Ke
      _rFilter(1.0f), _rpmFilter(0.05f), _useRipple(false) {
  _bemfKFilter.reset(_bemfConstant);
}

void BemfEstimator::setMotorParams(float rArmature, int poles) {
  if (rArmature > 0.0f) {
    _rArmature = rArmature;
  }
  if (poles > 0)
    _poles = poles;
}

void BemfEstimator::setBemfConstant(float ke) {
  if (ke > 0.0f) {
    _bemfConstant = ke;
    _bemfKFilter.reset(ke);
  }
}

void BemfEstimator::updateLowSpeedData(float vApplied, float iAvg) {
  _vApplied = vApplied;
  _iAvg = iAvg;
}

void BemfEstimator::updateRippleFreq(float freqHz) { _rippleFreq = freqHz; }

void BemfEstimator::calculateEstimate() {
  // 1. Calculate Theoretical Stall Current (Ohm's Law)
  float iStallTheoretical =
      (_rArmature > 0.1f) ? (_vApplied / _rArmature) : 0.0f;

  // 2. Authoritarian Stall Detection
  bool physicallyStalled = false;
  if (_vApplied > 0.5f && _iAvg > 0.01f) {
    if (_iAvg > (iStallTheoretical * 0.98f)) {
      physicallyStalled = true;
    }
  }

  // 3. BEMF Calculation
  float vDrop = _iAvg * _rArmature;
  _vBemf = _vApplied - vDrop;
  if (_vBemf < 0.0f)
    _vBemf = 0.0f;

  float estimatedRpmFromBemf = 0.0f;
  if (_bemfConstant > 0.0f) {
    estimatedRpmFromBemf = _vBemf / _bemfConstant;
  }

  // 4. Ripple RPM
  float rippleRpm = 0.0f;
  if (_rippleFreq > 0.0f) {
    rippleRpm = (_rippleFreq * 60.0f) / (2.0f * (float)_poles);
  }

  // 5. Fusion Logic with Noise Floor Clamping
  bool rippleValid = (rippleRpm > 0.0f && _iAvg > 0.20f && _vApplied > 3.0f);

  float rawEstimate = 0.0f;
  if (rippleValid) {
    rawEstimate = rippleRpm;
    _useRipple = true;

    // --- DYNAMIC Ke LEARNING ---
    // If ripple is solid and we are moving fast, learn the V/RPM constant
    if (rippleRpm > 800.0f && _vApplied > 4.0f && _vBemf > 1.0f) {
      float instantK = _vBemf / rippleRpm;
      if (instantK > 0.005f && instantK < 0.03f) {
        _bemfConstant = _bemfKFilter.update(instantK);
      }
    }
  } else if (_vApplied > 2.0f) {
    // Only trust BEMF math if there is enough voltage to beat the noise floor
    rawEstimate = estimatedRpmFromBemf;
    _useRipple = false;
  } else {
    // Low speed with no ripple: output 0 to prevent PI stutter
    rawEstimate = 0.0f;
    _useRipple = false;
  }

  // 6. Stall Guard Override
  if (physicallyStalled || _vApplied < 0.4f) {
    rawEstimate = 0.0f;
  }

  // Apply smoothing filter to final output
  _estimatedRpm = _rpmFilter.update(rawEstimate);

  // Hard force to zero if voltage is stopped
  if (_vApplied < 0.05f) {
    _estimatedRpm = 0.0f;
    _rpmFilter.reset(0.0f);
  }
}

void BemfEstimator::reset() {
  _rArmature = 35.0f;
  _bemfConstant = 0.015f;
  _rpmFilter.reset(0.0f);
  _useRipple = false;
  _bemfKFilter.reset(0.015f);
  Log.println("BemfEstimator: Reset to Static 35 Ohm Baseline");
}

float BemfEstimator::getEstimatedRpm() const { return _estimatedRpm; }
float BemfEstimator::getBemfVoltage() const { return _vBemf; }
float BemfEstimator::getMeasuredResistance() const { return _rArmature; }

bool BemfEstimator::isStalled() const {
  return (_vApplied > 2.0f && _estimatedRpm < 10.0f);
}
