#include "DspFilters.h"

// --- EmaFilter ---

EmaFilter::EmaFilter(float alpha) : _alpha(alpha), _value(0.0f) {}

void EmaFilter::setAlpha(float alpha) {
  if (alpha < 0.0f)
    _alpha = 0.0f;
  else if (alpha > 1.0f)
    _alpha = 1.0f;
  else
    _alpha = alpha;
}

float EmaFilter::update(float input) {
  _value = (_alpha * input) + ((1.0f - _alpha) * _value);
  return _value;
}

float EmaFilter::getValue() const { return _value; }

void EmaFilter::reset(float initialValue) { _value = initialValue; }

// --- DcBlocker ---

DcBlocker::DcBlocker(float alpha)
    : _alpha(alpha), _prevInput(0.0f), _prevOutput(0.0f) {}

float DcBlocker::process(float input) {
  // y[n] = alpha * y[n-1] + alpha * (x[n] - x[n-1])
  float output = _alpha * _prevOutput + _alpha * (input - _prevInput);
  _prevInput = input;
  _prevOutput = output;
  return output;
}

void DcBlocker::reset() {
  _prevInput = 0.0f;
  _prevOutput = 0.0f;
}
