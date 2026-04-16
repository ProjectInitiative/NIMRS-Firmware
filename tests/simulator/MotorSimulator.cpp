#include "MotorSimulator.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MotorSimulator::MotorSimulator(const Params &params)
    : _params(params), _currentNoise(0.0f, 0.005f) {
  std::random_device rd;
  _gen = std::mt19937(rd());
}

void MotorSimulator::step(float dt, float duty, float loadTorque) {
  // 1. Calculate Applied Voltage with Chaos (Dropouts)
  float vIn = _params.vTrack * duty;
  if (_dccDropoutActive) {
    vIn = 0.0f;
  }
  _state.vApplied = vIn;

  // 2. Adjust Resistance for Temperature (approx +0.4%/degC for Copper)
  float rTemp = _params.R * (1.0f + 0.004f * (_state.temperature - 25.0f));

  // 3. Electrical Step: di/dt = (V - I*R - Ke*omega) / L
  // omega in rad/s = rpm * (2*pi/60)
  float omega = (_state.rpm * 2.0f * (float)M_PI) / 60.0f;
  float backEmf = _params.Ke * _state.rpm;
  float dIdt = (vIn - (_state.current * rTemp) - backEmf) / _params.L;
  _state.current += dIdt * dt;

  // 4. Mechanical Step: dw/dt = (Kt*I - B*w - Load - Friction) / J
  float torqueMotor = _params.Kt * _state.current;
  float torqueFriction = _params.B * omega;
  float totalLoad = loadTorque + _stochasticLoad;

  // Static Friction Modeling (Stiction)
  float torqueStatic = 0.0f;
  if (std::abs(_state.rpm) < 1.0f) { // Near zero speed
    if (std::abs(torqueMotor - totalLoad) < _params.staticFriction) {
      torqueMotor = totalLoad; // Stuck
      _state.rpm = 0.0f;
      omega = 0.0f;
    } else {
      torqueStatic = (torqueMotor > totalLoad) ? _params.staticFriction
                                               : -_params.staticFriction;
    }
  } else {
    torqueStatic =
        (_state.rpm > 0) ? _params.staticFriction : -_params.staticFriction;
  }

  float netTorque = torqueMotor - torqueFriction - totalLoad - torqueStatic;
  float dOmegadt = netTorque / _params.J;
  omega += dOmegadt * dt;

  _state.rpm = (omega * 60.0f) / (2.0f * (float)M_PI);

  // 5. Integrate Angle
  _state.angle += omega * dt;
  _state.angle = std::fmod(_state.angle, 2.0f * (float)M_PI);
  if (_state.angle < 0)
    _state.angle += 2.0f * (float)M_PI;
}

void MotorSimulator::getAdcSamples(float *buffer, size_t count,
                                   float sampleRate) {
  float subDt = 1.0f / sampleRate;
  State savedState = _state;

  for (size_t i = 0; i < count; i++) {
    // Sub-step for high-fidelity ADC trace (ripple and noise)
    step(subDt, _state.vApplied / _params.vTrack, 0.0f);
    buffer[i] = getInstantaneousCurrentWithRipple();
  }
  // Restore physics state to keep main sim loop consistent
  _state = savedState;
}

float MotorSimulator::getInstantaneousCurrentWithRipple() const {
  // Commutator Ripple: Sinusoidal approximation of brush transitions
  // Ripple frequency = poles * 2 crossings per pole per revolution
  float rippleFactor =
      1.0f + 0.15f * std::sin(_state.angle * _params.poles * 2.0f);
  float noise = _currentNoise(_gen);
  return (_state.current * rippleFactor) + noise;
}

void MotorSimulator::applyStochasticLoad(float variance) {
  std::normal_distribution<float> dist(0.0f, variance);
  _stochasticLoad = dist(_gen);
}

void MotorSimulator::simulateThermalDrift(float dt) {
  // Simple thermal model: Heating proportional to I^2, Cooling to (T - Amb)
  float pIn = (_state.current * _state.current) * _params.R;
  float pOut = (_state.temperature - 25.0f) * 0.01f; // Thermal resistance
  _state.temperature += (pIn - pOut) * dt * 0.1f;    // Thermal mass factor
}

void MotorSimulator::applyDccNoise(float dropoutProb) {
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  _dccDropoutActive = (dist(_gen) < dropoutProb);
}
