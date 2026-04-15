# PID Simulator & Testing Apparatus Architecture

This document outlines the design and implementation plan for a high-fidelity motor simulator designed to refine and validate the NIMRS PID control loops.

## 1. Overview

The goal is to create a "Digital Twin" of the motor and engine mechanics that can run in a CI/CD environment or locally as a development tool. It replaces the physical hardware (`MotorHal`) with a mathematical model of DC motor physics.

## 2. Mathematical Model

The simulator implements a second-order DC motor model:

### Electrical Equation:

$$V(t) = I(t) \cdot R + L \cdot \frac{dI(t)}{dt} + K_e \cdot \omega(t)$$
Where:

- $V(t)$: Applied voltage (PWM Duty $\cdot$ Track Voltage)
- $I(t)$: Armature current
- $R$: Armature resistance (Ohms)
- $L$: Armature inductance (Henries)
- $K_e$: Back-EMF constant (V/rad/s or V/RPM)
- $\omega(t)$: Angular velocity (RPM)

### Mechanical Equation:

$$J \cdot \frac{d\omega(t)}{dt} = K_t \cdot I(t) - B \cdot \omega(t) - \tau_{load}(t)$$
Where:

- $J$: Rotational inertia (kg·m²)
- $K_t$: Torque constant (N·m/A, typically $K_t \approx K_e$)
- $B$: Viscous friction coefficient
- $\tau_{load}$: External load torque (Simulates hills, friction, or stalls)

## 3. Architecture Components

### 3.1 `MotorSimulator` (Engine)

A C++ class that maintains the state ($\omega, I$) and integrates the differential equations using Euler or Runge-Kutta integration at a high frequency (e.g., 100kHz).

**Capabilities:**

- **Noise Injection:** Gaussian noise added to Current and BEMF signals.
- **Ripple Generation:** Generates synthetic commutator ripple pulses based on current $\omega$ and motor poles.
- **Load Profiles:** Ability to set static load, slope-based load, or stochastic load.

### 3.2 `SimulatedMotorHal` (Interface)

A specialized implementation of `MotorHal` for the test environment.

- Links directly to `MotorSimulator`.
- `setDuty()` updates the simulator's input voltage.
- `getAdcSamples()` pulls "sampled" current from the simulator's internal buffer, mimicking the 20kHz synchronized sampling of the ESP32 MCPWM.

### 3.3 `PIDTestHarness` (Runner)

A CLI tool that orchestrates the simulation.

- Runs the real `MotorTask` logic in a loop.
- Steps the `MotorSimulator` in lock-step.
- **Scenarios:** Defines a suite of test cases (Fast accel, Slow crawl, Sudden stall, Reverse under load).
- **Telemetry:** Outputs CSV/JSON data for analysis and plotting.

## 4. Integration with Build System

- **Nix Flake:** Add a `motor-sim` output that builds the harness.
- **Tools:** Python scripts in `tools/` to visualize the simulator output (using Matplotlib/Plotly).

## 5. Action Plan

### Phase 1: Core Simulator Implementation

1. [ ] Create `tests/simulator/MotorSimulator.h/cpp`.
2. [ ] Implement the ODE solver for motor dynamics.
3. [ ] Implement ripple pulse generation logic.
4. [ ] Unit test the simulator in isolation to ensure it follows physics.

### Phase 1.5: The Chaos Engine (Stochastic Effects)

1. [ ] Implement `applyStochasticLoad()` to simulate random friction spikes.
2. [ ] Implement `simulateThermalDrift()` to slowly increase $R$ during high-load runs.
3. [ ] Implement `applyDccNoise()` to simulate dirty power interrupts.

### Phase 2: Hardware Abstraction & Mocking

1. [ ] Create `tests/mocks/SimulatedMotorHal.cpp`.
2. [ ] Implement synchronized ADC buffer filling from the simulator.
3. [ ] Integrate with `MotorTask` in a dedicated test file `tests/test_PID_Simulator.cpp`.

### Phase 3: Scenario & Edge Case Development

1. [ ] **Scenario: Clean Start/Stop** - Verify basic PI tracking.
2. [ ] **Scenario: Hard Stall** - Verify `lowSpeedStall` detection and shutdown.
3. [ ] **Scenario: Heavy Train** - Increase inertia $J$ and friction $B$.
4. [ ] **Scenario: Dirty Track** - Inject high-frequency noise and dropouts in $V(t)$.
5. [ ] **Scenario: High-Speed Flywheel** - Test behavior with large momentum.

### Phase 4: Tooling & Automation

1. [ ] Create `tools/plot_sim_results.py`.
2. [ ] Add `motor-sim` command to `nix/scripts.nix`.
3. [ ] Integrate simulator into `ci-ready` checks.

## 6. Detailed Edge Cases to Simulate

| Case                 | Description                                            | Expected PID Behavior                                              |
| -------------------- | ------------------------------------------------------ | ------------------------------------------------------------------ |
| **Cold Start**       | Motor starts from 0 RPM with high static friction.     | Stiction kick should overcome friction; PI should not overshoot.   |
| **Instant Stall**    | Load torque $\tau_{load}$ exceeds $K_t \cdot I_{max}$. | `MotorTask` detects stall via $di/dt$ and shuts down.              |
| **Back-Driving**     | External torque pushes motor faster than target.       | PID should reduce duty; check for "regenerative" current handling. |
| **Commutator Noise** | High variance in $R$ during rotation.                  | `BemfEstimator` should filter noise and maintain RPM estimate.     |
| **DCC Dropout**      | Track voltage $V_{track}$ drops to 0 momentarily.      | PID should hold state or gracefully recover without jerking.       |
| **Thermal Drift**    | $R$ increases as motor heats up.                       | `BemfEstimator` must update its internal model to avoid RPM error. |
| **Gear Backlash**    | Mechanical "slop" between motor and load.              | PID must be damped enough to avoid limit-cycle oscillations.       |
| **ADC Saturation**   | Current exceeds 12-bit ADC range.                      | Firmware should handle "clamped" data safely without crashing.     |
