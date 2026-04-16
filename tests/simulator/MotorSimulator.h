#ifndef MOTOR_SIMULATOR_H
#define MOTOR_SIMULATOR_H

#include <cmath>
#include <random>
#include <vector>

/**
 * @brief High-fidelity DC Motor Simulator
 *
 * Models the electrical and mechanical dynamics of a DC motor:
 * V = I*R + L*di/dt + Ke*w
 * J*dw/dt = Kt*I - B*w - Load - StaticFriction
 */
class MotorSimulator {
public:
  struct Params {
    float R = 35.0f;               // Armature Resistance (Ohms)
    float L = 0.01f;               // Armature Inductance (Henries)
    float Ke = 0.015f;             // Back-EMF constant (V/RPM)
    float Kt = 0.015f;             // Torque constant (N*m/A)
    float J = 0.0001f;             // Rotational Inertia (kg*m^2)
    float B = 0.0005f;             // Viscous Friction (N*m*s/rad)
    float staticFriction = 0.001f; // Static Friction (N*m)
    int poles = 5;                 // Number of commutator poles
    float vTrack = 14.0f;          // Track voltage (Volts)
  };

  struct State {
    float current = 0.0f;      // Amps
    float rpm = 0.0f;          // RPM
    float angle = 0.0f;        // Radians
    float vApplied = 0.0f;     // Volts (from PWM)
    float temperature = 25.0f; // Celsius
  };

  MotorSimulator(const Params &params = Params());

  /**
   * @brief Step the simulation forward
   * @param dt Timestep in seconds
   * @param duty Applied duty cycle (-1.0 to 1.0)
   * @param loadTorque External load torque (N*m)
   */
  void step(float dt, float duty, float loadTorque = 0.0f);

  /**
   * @brief Get ADC samples (Synchronized with PWM frequency)
   * @param buffer Target buffer
   * @param count Number of samples
   * @param sampleRate Rate in Hz
   */
  void getAdcSamples(float *buffer, size_t count, float sampleRate);

  /**
   * @brief Inject current ripple into the current signal
   * Based on commutator poles and angular position.
   */
  float getInstantaneousCurrentWithRipple() const;

  /**
   * @brief Stochastic Effects (The Chaos Engine)
   */
  void applyStochasticLoad(float variance);
  void simulateThermalDrift(float dt);
  void applyDccNoise(float dropoutProb);

  const State &getState() const { return _state; }
  void setState(const State &s) { _state = s; }
  void setParams(const Params &p) { _params = p; }
  const Params &getParams() const { return _params; }

private:
  Params _params;
  State _state;

  // Noise and Chaos state
  mutable std::mt19937 _gen;
  mutable std::normal_distribution<float> _currentNoise;
  float _stochasticLoad = 0.0f;
  bool _dccDropoutActive = false;
};

#endif
