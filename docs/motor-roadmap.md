# NIMRS Motor Control Roadmap: Model-Based Estimator

This document outlines the architecture for a **Sensorless Adaptive Motor Control System** using the DRV8213DSGR with IPROPI current feedback. This replaces the traditional "Open Loop + Kick Start" logic with a physics-based model to achieving smooth slow crawls and synchronized chuffing.

## Objective

Implement a control system that:

- Uses **IPROPI current feedback**.
- Estimates speed using a **DC Motor Model**.
- Integrates position (`theta`) for **Chuff Timing**.
- Detects disturbances (grades/loads).
- Adapts parameters (`R`, `Ke`) during steady-state to prevent drift.

**Target:** Smooth slow crawl + perceptually convincing chuff sync without physical sensors.

---

## Architecture

We will use a 4-layer structure:

1.  **Fast Torque/Current Observer** (1kHz - Loop): Reads Current, Estimates Speed.
2.  **Speed Estimator + PI Loop** (1kHz): Adjusts PWM to match Target Speed.
3.  **Disturbance Detector** (~200Hz): Detects grades/acceleration events.
4.  **Adaptive Parameter Estimator** (1Hz): Slowly corrects `R` and `Ke` to prevent drift.

### Motor Model

$$ V = I \cdot R + K_e \cdot \omega $$

- $V$: Applied Voltage ($PWM \times V_{supply}$)
- $I$: Measured Current (IPROPI)
- $R$: Winding Resistance (Estimated)
- $K_e$: Back-EMF Constant (Estimated)
- $\omega$: Angular Velocity (Estimated Speed)

**Speed Estimate:**
$$ \omega*{est} = \frac{V - I \cdot R*{est}}{K\_{est}} $$

---

## Implementation Stages

### Stage 1: Basic Motor Model & Speed PI [NEXT]

Implement the core physics engine to replace the current "Kick Start" logic.

- **Tasks:**
  - Add CVs for `MOTOR_R` (Resistance) and `MOTOR_KE` (EMF Constant).
  - Implement `estimateSpeed()` function using the $V - IR$ formula.
  - Implement **PI Speed Controller**:
    - Calculates `Error = TargetSpeed - EstSpeed`.
    - Adjusts PWM to minimize error.
- **Outcome:** Superior low-speed crawl and load compensation. "Kick Start" becomes native behavior of the PI loop (High Error -> High PWM -> Movement).

### Stage 2: Virtual Position Integration (Chuff Sync)

Use the estimated speed to drive the sound engine.

- **Tasks:**
  - Integrate $\theta_{est}$ (Position) over time: $\theta += \omega \times dt$.
  - Trigger audio events at 90°, 180°, 270°, 0°.
- **Outcome:** Chuffs that speed up/slow down with the _simulated_ wheel speed, not just the throttle setting.

### Stage 3: Adaptive Error Shunting

Prevent the model from drifting over time due to heat or wear.

- **Tasks:**
  - Implement **Steady-State Detection** (Stable Current & PWM).
  - Implement **Parameter Adaptation**: Slowly nudge `R_est` to match reality.
- **Outcome:** Long-term stability and consistent performance as the motor warms up.

---

## Data Structures

### State Variables

```cpp
struct MotorState {
    float R_est;        // Ohms
    float Ke_est;       // V/RPM
    float omega_est;    // RPM
    float theta_est;    // Degrees (0-360)

    bool disturbance;   // Flag
    bool steady_state;  // Flag
};
```

### New CVs

- **CV 140:** Motor Resistance (mOhms / 10)
- **CV 141:** Motor Ke (EMF Constant)
- **CV 142:** Speed PI - P Gain
- **CV 143:** Speed PI - I Gain

```

```
