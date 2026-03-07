#include "BemfEstimator.h"
#include <algorithm>
#include <cmath>

BemfEstimator::BemfEstimator()
    : _rArmature(2.0f), _poles(5), _vApplied(0.0f), _iAvg(0.0f),
      _rippleFreq(0.0f), _vBemf(0.0f), _estimatedRpm(0.0f),
      _bemfConstant(0.002f), // Default guess: 0.002 V/RPM
      _bemfKFilter(0.001f),  // Very slow learning for Kv
      _rFilter(0.005f),      // Slow learning for R
      _useRipple(false) {
  _bemfKFilter.reset(_bemfConstant);
  _rFilter.reset(_rArmature);
}

void BemfEstimator::setMotorParams(float rArmature, int poles) {
  if (rArmature > 0.0f) {
    _rArmature = rArmature;
    _rFilter.reset(rArmature);
  }
  if (poles > 0)
    _poles = poles;
}

void BemfEstimator::updateLowSpeedData(float vApplied, float iAvg) {
  _vApplied = vApplied;
  _iAvg = iAvg;
}

void BemfEstimator::updateRippleFreq(float freqHz) { _rippleFreq = freqHz; }

void BemfEstimator::calculateEstimate() {
  // 1. Calculate Ripple RPM (Ground Truth speed when available)
  float rippleRpm = 0.0f;
  if (_rippleFreq > 0.0f) {
    rippleRpm = (_rippleFreq * 60.0f) / (2.0f * (float)_poles);
  }

  // 2. Dynamic Learning Logic
  // We can't solve one equation for two unknowns (R and Ke) simultaneously.
  // Strategy:
  // - At very low speed/high load (Start/Crawl), assume BEMF is ~0 and update
  // R.
  // - At medium/high speed with stable ripple, assume R is correct and update
  // Ke.

  if (_iAvg > 0.05f) { // Need significant current for reliable math
    float duty = (_vApplied > 0.1f) ? (_vApplied / 14.0f) : 0.0f; // Approx duty

    if (duty > 0.01f && duty < 0.12f) {
      // --- LOW SPEED: Learn Resistance ---
      // At < 12% duty, BEMF is very small (< 1V usually).
      // R = V_app / I (ignoring small BEMF for now)
      float instantR = _vApplied / _iAvg;
      if (instantR > 0.1f && instantR < 50.0f) {
        _rArmature = _rFilter.update(instantR);
      }
    } else if (rippleRpm > 800.0f && _vApplied > 4.0f) {
      // --- MEDIUM/HIGH SPEED: Learn BEMF Constant (Ke) ---
      // We have ground truth RPM from ripple.
      // Ke = (V_app - I*R) / RPM
      float vDrop = _iAvg * _rArmature;
      float estimatedBemf = _vApplied - vDrop;
      if (estimatedBemf > 1.0f) {
        float instantK = estimatedBemf / rippleRpm;
        // Sanity check for HO scale motors (usually 1-5 mV/RPM)
        if (instantK > 0.0005f && instantK < 0.01f) {
          _bemfConstant = _bemfKFilter.update(instantK);
        }
      }
    }
  }

  // 3. Final BEMF Calculation (using best known R)
  _vBemf = _vApplied - (_iAvg * _rArmature);
  if (_vBemf < 0.0f)
    _vBemf = 0.0f;

  // 4. Fusion Strategy
  float estimatedRpmFromBemf = 0.0f;
  if (_bemfConstant > 0.0f) {
    estimatedRpmFromBemf = _vBemf / _bemfConstant;
  }

  // Hysteresis for switching between Ripple and BEMF models
  if (rippleRpm > 600.0f) {
    _estimatedRpm = rippleRpm;
    _useRipple = true;
  } else if (rippleRpm < 400.0f) {
    _estimatedRpm = estimatedRpmFromBemf;
    _useRipple = false;
  } else {
    _estimatedRpm = _useRipple ? rippleRpm : estimatedRpmFromBemf;
  }
}

float BemfEstimator::getEstimatedRpm() const { return _estimatedRpm; }
float BemfEstimator::getBemfVoltage() const { return _vBemf; }
float BemfEstimator::getMeasuredResistance() const { return _rArmature; }

bool BemfEstimator::isStalled() const {
  // Stall condition: Applied voltage is high but RPM is very low
  return (_vApplied > 3.0f && _estimatedRpm < 20.0f);
}
