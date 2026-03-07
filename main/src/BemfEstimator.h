#ifndef BEMF_ESTIMATOR_H
#define BEMF_ESTIMATOR_H

#include "DspFilters.h"

class BemfEstimator {
public:
  BemfEstimator();

  // Configuration
  void setMotorParams(float rArmature, int poles);
  void setBemfConstant(float ke); // V/RPM

  // Runtime Updates
  void updateLowSpeedData(float vApplied, float iAvg);
  void updateRippleFreq(float freqHz);

  // Calculation
  void calculateEstimate();
  void reset();

  // Output
  float getEstimatedRpm() const;
  float getBemfVoltage() const;
  float getMeasuredResistance() const;
  float getBemfConstant() const { return _bemfConstant; }
  bool isStalled() const;
  bool useRipple() const { return _useRipple; }

private:
  float _rArmature; // Current learned/configured resistance
  int _poles;

  // Inputs
  float _vApplied;
  float _iAvg;
  float _rippleFreq;

  // Outputs
  float _vBemf;
  float _estimatedRpm;

  // Internal Tracking
  float _bemfConstant;    // V/RPM
  EmaFilter _bemfKFilter; // To smooth the learned Ke
  EmaFilter _rFilter;     // To smooth learned R
  EmaFilter _rpmFilter;   // To smooth final output

  bool _useRipple;
};

#endif
