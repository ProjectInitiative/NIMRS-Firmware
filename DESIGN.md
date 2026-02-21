# NIMRS Motor Control: Sensorless Hybrid Architecture

## 1. The Core Problem: Torque vs. Speed Control

The current `MotorController.cpp` acts as a **Torque Controller**. It uses the DRV8213's IPROPI pin to measure current ($I_{motor}$) and applies proportional power to overcome mechanical binds in the running gear.

**The Flaw (Rubber-Banding):**

1. Motor hits a tight spot in the side rods; current spikes.
2. The PI loop dumps in power (torque) to break the stiction.
3. The bind clears, but the high torque remains momentarily.
4. The motor violently surges forward.
5. Current drops, the PI loop cuts power, and the motor coasts into the next bind and stalls.

**The Solution: Speed Control via BEMF**
We must track the *actual speed* of the motor. When speed drops, we add torque. The millisecond the bind clears and the motor attempts to surge, the speed measurement spikes, allowing the PI loop to instantly brake the motor. This yields a flawless, ultra-low-speed crawl.

## 2. Hardware Constraints & Sensorless Physics

The current PCB lacks a voltage divider across the `OUT1`/`OUT2` pins. Therefore, we cannot measure Back-EMF directly without hardware bodges. We must calculate speed mathematically using the existing IPROPI current mirror.

The fundamental motor equation:
$$ V_{bemf} = V_{applied} - (I_{motor} \times R_{armature}) $$

* **$V_{bemf}$ (Back-EMF):** Proportional to actual RPM.
* **$V_{applied}$ (Applied Voltage):** Calculated in firmware ($V_{track} \times DutyCycle$).
* **$I_{motor}$ (Current):** Read continuously via the IPROPI ADC pin.
* **$R_{armature}$ (Armature Resistance):** A physical constant of the copper windings, measured manually.

## 3. The Hybrid Strategy

A 5-pole motor at 2 RPM generates too little BEMF for clean high-speed math, and its commutation ripples are too infrequent. We will use a dual-model approach:

* **Low-Speed (Speed Steps 1-15): State Observer Math**
* Uses the $V_{bemf}$ equation.
* Relies on the instantaneous current spike of a mechanical bind to mathematically force $V_{bemf}$ to near-zero, triggering the PI loop to push through the stall.


* **High-Speed (Speed Steps 15+): Ripple Counting**
*
* Uses DMA sampling to count the physical commutation spikes (10 per revolution for a 5-pole motor).
* Immune to track voltage fluctuations and temperature drift.



## 4. System Architecture (Dual-Core ESP32-S3)

To handle the DSP overhead without blocking DCC packet decoding, the motor control will be isolated into specialized modules.

### Hardware Abstraction

* **`MotorHal.cpp`**
* Initializes MCPWM for the DRV8213.
* Configures ADC1 and continuous DMA sampling on the IPROPI pin.
* Synchronizes ADC reads to the center of the PWM "ON" cycle (critical for clean data at low duty cycles).



### Signal Processing Pipeline

* **`DspFilters.cpp`**
* Low-pass filter (EMA) to extract stable $I_{avg}$ for the low-speed math model.
* DC-bias removal to isolate AC commutation ripples.


* **`RippleDetector.cpp`**
* Analyzes the filtered DMA buffer array.
* Uses a Schmitt trigger algorithm to count local current drops.
* Outputs a raw commutation frequency (Hz).



### Core Mathematical Modeling

* **`BemfEstimator.cpp`**
* Ingests $V_{applied}$, $I_{motor}$, and Ripple Hz.
* Executes the low-speed $V_{bemf}$ equation.
* Applies crossover logic and hysteresis to seamlessly blend between the mathematical estimate and the physical ripple count.



### Control Execution

* **`MotorTask.cpp`**
* A FreeRTOS task pinned to Core 1.
* Executes the main real-time PI control loop at a fixed 50Hz.
* Takes the RPM output from `BemfEstimator` and adjusts the `MotorHal` PWM to maintain the target speed.



## 5. Modifying `MotorController.cpp`

The existing monolith will be stripped down.

* **Remove:** The "Torque Punch" (`loadError`) logic.
* **Replace With:** The BEMF Error loop.

**Conceptual Loop Refactor:**

```cpp
// 1. Calculate Applied Voltage
float vApplied = vTrack * (finalPwm / 1023.0f);

// 2. Feed the Estimator
_estimator.updateLowSpeedData(vApplied, _avgCurrent);
_estimator.calculateEstimate();

// 3. Calculate Speed Error (Target RPM vs Actual RPM)
float targetRpm = speedNorm * MAX_RPM;
float actualRpm = _estimator.getEstimatedRpm();
float speedError = targetRpm - actualRpm;

// 4. PI Output (Allows negative correction to prevent surges)
float pTerm = speedError * Kp;
float iTerm = _piErrorSum * Ki;
float pidCorrection = constrain(pTerm + iTerm, -MAX_PUNCH, MAX_PUNCH);

finalPwm = basePwm + pidCorrection;

```

## 6. Project Action Items

### Hardware / User Tasks (Kyle)

1. **Measure Armature Resistance ($R_a$):** Use a multimeter across pins 18 and 19 of the 21-pin socket with the decoder removed. Slowly rotate the flywheel, take a few readings, and average them.
2. **Measure Track Voltage ($V_{track}$):** Confirm the exact DC voltage hitting the DRV8213 `VM` pin from your power supply.

### Firmware / AI Tasks (Gemini)

1. Draft `BemfEstimator.h` and `BemfEstimator.cpp` to establish the math engine.
2. Draft `RippleDetector.h` and `RippleDetector.cpp` for the high-speed DSP.
3. Draft `MotorHal.cpp` to set up the crucial ADC DMA buffer and PWM synchronization.
4. Refactor `MotorController.cpp` into `MotorTask.cpp` to integrate the components into the Core 1 FreeRTOS loop.
