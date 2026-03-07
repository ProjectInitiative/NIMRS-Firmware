# DRV8213DSG Stall Detection & Calibration Guide

## 1. Hardware Configuration (DSG Package)

The **DSG (WSON-8)** package is a single-supply variant. Unlike the RTE package, it lacks dedicated `nSTALL` and `TINRUSH` pins. All stall detection must be implemented in software using the **IPROPI** analog feedback.

- **Sense Resistor ($R_{IPROPI}$)**: 2.4 kΩ (User provided).
- **Operating Voltage ($V_{VM}$)**: 1.65 V to 11 V.
- **Headroom Requirement**: Maintain at least **1.25 V** between $V_{VM}$ and the maximum $V_{IPROPI}$ to ensure accuracy.

---

## 2. Gain & Voltage Formulas

The voltage read by the ESP32 ADC is determined by the **GAINSEL** pin state, which shifts the current scaling factor ($A_{IPROPI}$).

### Calculations for $R_{IPROPI} = 2400\Omega$

| GAINSEL State          | $A_{IPROPI}$ (Typ) | Resulting Gain ($V_{out} / I_{motor}$) |
| ---------------------- | ------------------ | -------------------------------------- |
| **Low** (to GND)       | $205\ \mu A/A$     | **~0.492 V per Amp**                   |
| **High-Z** (Open)      | $1050\ \mu A/A$    | **~2.520 V per Amp**                   |
| **High** (to $V_{CC}$) | $4900\ \mu A/A$    | **~11.760 V per Amp**                  |

Standard Formula:
$$V_{IPROPI} = (I_{motor} 	imes A_{IPROPI}) 	imes 2400$$

---

## 3. Implementation Logic

### Startup Blanking ($t_{INRUSH}$)

Motor current naturally spikes during startup (inrush current).

- **Action**: Your software must ignore the stall threshold for a duration ($t_{INRUSH}$) after the motor starts.
- **Typical Value**: 50ms to 200ms, depending on the locomotive's weight/flywheel.

### Stall Validation ($t_{STALL}$)

Because your hardware lacks a filtering capacitor, implement a **moving average** or **integrator** in the ESP32 code.

1. **Read**: Sample the ADC at high frequency (burst mode).
2. **Integrate**: Only trigger a stall if $V_{IPROPI} > Threshold$ persists for $t_{STALL}$ (e.g., 20–50ms).

---

## 4. Semi-Automated "Per-Motor" Calibration

To handle different engine/decoder attributes, implement this routine in your firmware:

1. **Baseline (Normal Load)**:
   - Command the engine to a steady speed (e.g., 50% PWM).
   - Record the average $V_{IPROPI}$ after $t_{INRUSH}$ settles. Define this as `V_NORMAL`.

2. **Stall Learning**:
   - While the motor is physically stalled, ramp the PWM until the engine draws its maximum safe current.
   - Record this value as `V_STALL_MAX`.

3. **Threshold Calculation**:
   - `Stall_Threshold = (V_STALL_MAX + V_NORMAL) / 2` (or roughly 80% of `V_STALL_MAX`).
   - Store this value in the ESP32-S3 NVS for that specific locomotive profile.

### BEMF Double-Check

Use your BEMF feedback for zero-rotation confirmation:

- **Condition**: If `V_IPROPI > Stall_Threshold` AND `BEMF_Feedback ≈ 0`, then **Stall = TRUE**.
