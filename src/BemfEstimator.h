#ifndef BEMF_ESTIMATOR_H
#define BEMF_ESTIMATOR_H

#include "DspFilters.h"

class BemfEstimator {
public:
  BemfEstimator();

  // Configuration
  void setMotorParams(float rArmature, int poles);

  // Runtime Updates
  void updateLowSpeedData(float vApplied, float iAvg);
  void updateRippleFreq(float freqHz);

  // Calculation
  void calculateEstimate();

  // Output
  float getEstimatedRpm() const;
  float getBemfVoltage() const;
  bool isStalled() const;

private:
  float _rArmature; // Ohms
  int _poles;

  // Inputs
  float _vApplied;
  float _iAvg;
  float _rippleFreq;

  // Outputs
  float _vBemf;
  float _estimatedRpm;

  // Internal
  float _bemfConstant;    // V/RPM
  EmaFilter _bemfKFilter; // To smooth the learned K

  bool _useRipple;
};

#endif
