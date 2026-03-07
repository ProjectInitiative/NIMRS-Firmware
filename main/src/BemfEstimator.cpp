#include "BemfEstimator.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

BemfEstimator::BemfEstimator()
    : _rArmature(20.0f), _poles(5), _vApplied(0.0f), _iAvg(0.0f),
      _rippleFreq(0.0f), _vBemf(0.0f), _estimatedRpm(0.0f),
      _bemfConstant(0.015f), // Default guess: 15mV/RPM (Typical HO)
      _bemfKFilter(0.001f),  // Very slow learning for Ke
      _rFilter(0.002f),      // Even slower learning for R
      _rpmFilter(0.1f),      // Faster filter for responsiveness
      _useRipple(false) {
  _bemfKFilter.reset(_bemfConstant);
  _rFilter.reset(_rArmature);
  _rpmFilter.reset(0.0f);
}

void BemfEstimator::setMotorParams(float rArmature, int poles) {
  if (rArmature > 0.0f) {
    _rArmature = rArmature;
    _rFilter.reset(rArmature);
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
  // 1. Calculate Ripple RPM
  float rippleRpm = 0.0f;
  if (_rippleFreq > 0.0f) {
    rippleRpm = (_rippleFreq * 60.0f) / (2.0f * (float)_poles);
  }

  // 2. Dynamic Learning Logic
  if (_iAvg > 0.10f) { // Lower threshold to allow learning on high-R motors
    float duty = (_vApplied > 0.1f) ? (_vApplied / 14.0f) : 0.0f;

    // Learn R only at very low speeds where BEMF is negligible
    if (duty > 0.02f && duty < 0.10f) {
      float instantR = _vApplied / _iAvg;
      // HO motors floor: 8 ohms.
      if (instantR > 8.0f && instantR < 60.0f) {
        _rArmature = _rFilter.update(instantR);
      }
    } else if (rippleRpm > 1000.0f && _vApplied > 5.0f) {
      // Learn Ke only at high speeds with stable ripple
      float vDrop = _iAvg * _rArmature;
      float estimatedBemf = _vApplied - vDrop;
      if (estimatedBemf > 1.5f) {
        float instantK = estimatedBemf / rippleRpm;
        if (instantK > 0.0005f && instantK < 0.01f) {
          _bemfConstant = _bemfKFilter.update(instantK);
        }
      }
    }
  }

  // 3. Final BEMF Calculation
  float vDrop = _iAvg * _rArmature;
  _vBemf = _vApplied - vDrop;

  // CRITICAL: Force BEMF to 0 if we aren't applying power or if current is too
  // low to move.
  if (_vApplied < 0.1f) {
    _vBemf = 0.0f;
  }

  // If we are stalled (low current, low voltage), definitely no BEMF.
  if (_iAvg < 0.15f && _vApplied < 3.0f) {
    _vBemf = 0.0f;
  }

  if (_vBemf < 0.0f)
    _vBemf = 0.0f;

  // 4. Fusion Strategy
  float estimatedRpmFromBemf = 0.0f;
  if (_bemfConstant > 0.0f) {
    estimatedRpmFromBemf = _vBemf / _bemfConstant;
  }

  // 5. Hysteresis & Stall Detection
  // Only trust ripple if we have significant power AND current.
  bool rippleValid = (rippleRpm > 0.0f && _iAvg > 0.35f && _vApplied > 3.0f);

  float rawEstimate = 0.0f;
  if (rippleValid && rippleRpm > 1200.0f) {
    rawEstimate = rippleRpm;
    _useRipple = true;
  } else if (!rippleValid || rippleRpm < 1000.0f) {
    rawEstimate = estimatedRpmFromBemf;
    _useRipple = false;
  } else {
    rawEstimate = _useRipple ? rippleRpm : estimatedRpmFromBemf;
  }

  // 6. Final Stall Guard
  // If we don't have valid ripple and voltage is low, assume 0 RPM.
  // This keeps the PI loop driving harder.
  if (!rippleValid && _vApplied < 3.0f && rawEstimate < 200.0f) {
    rawEstimate = 0.0f;
  }

  // Apply smoothing filter to final output
  _estimatedRpm = _rpmFilter.update(rawEstimate);

  // Hard force to zero if stopped
  if (_vApplied < 0.01f) {
    _estimatedRpm = 0.0f;
    _rpmFilter.reset(0.0f);
  }
}

void BemfEstimator::reset() {
  _rArmature = 20.0f;
  _bemfConstant = 0.015f;
  _rFilter.reset(20.0f);
  _bemfKFilter.reset(0.015f);
  _rpmFilter.reset(0.0f);
  _useRipple = false;
  Log.println("BemfEstimator: Model Reset to Defaults (20 Ohm, 15mV/RPM)");
}

float BemfEstimator::getEstimatedRpm() const { return _estimatedRpm; }
float BemfEstimator::getBemfVoltage() const { return _vBemf; }
float BemfEstimator::getMeasuredResistance() const { return _rArmature; }

bool BemfEstimator::isStalled() const {
  return (_vApplied > 3.5f && _estimatedRpm < 20.0f);
}
