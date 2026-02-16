// #include "MotorController.h"
// #include "CvRegistry.h"
// #include "Logger.h"
// #include "driver/gpio.h"
// #include <algorithm> // For std::sort
// #include <cmath>

// MotorController::MotorController()
//     : _currentDirection(true), _currentSpeed(0.0f), _lastMomentumUpdate(0), 
//       _lastCvUpdate(0), _avgCurrent(0.0f), _currentOffset(0.0f),
//       _pwmResolution(12), _maxPwm(4095), _pwmFreq(16000), _cvBaselineAlpha(5),
//       _cvStictionKick(50), _cvDeltaCap(180), _cvPwmDither(0),
//       _cvBaselineReset(0), _cvCurveIntensity(0), _torqueIntegrator(0.0f),
//       _startupKickActive(false), _startupKickTime(0), _lastBaselineUpdate(0),
//       _adcIdx(0), _lastPwmValue(0), _lastSmoothingPwm(0), _stallStartTime(0),
//       _stallThreshold(0.8f), _cvAccel(3), _cvDecel(3), _cvVstart(40),
//       _cvVmid(128), _cvVhigh(255), _cvConfig(0), _cvPedestalFloor(35),
//       _cvLoadGain(128), _cvLoadGainScalar(128), _cvLearnThreshold(5),
//       _cvHardwareGain(0), _cvDriveMode(0) {
  
//   for (int i = 0; i < 16; i++) _adcHistory[i] = 0;

//   // Prototypical S-Curve Default (CV 67-94)
//   uint8_t sCurve[] = {1, 3, 7, 12, 18, 26, 35, 46, 58, 72, 87, 103, 119, 135, 
//                       151, 166, 181, 194, 206, 217, 226, 234, 241, 246, 250, 253, 254, 255};
  
//   for (int i = 0; i < 28; i++) {
//     _speedTable[i] = sCurve[i];
//   }

//   // Ensure SPEED_STEPS is 256 in MotorController.h
//   for (int i = 0; i < SPEED_STEPS; i++) {
//     _baselineTable[i] = 0.0f;
//   }
// }

// void MotorController::setup() {
//   // --- FIX 1: UNLOCK JTAG PINS (GPIO 39-41) ---
//   gpio_reset_pin((gpio_num_t)Pinout::MOTOR_IN1);
//   gpio_reset_pin((gpio_num_t)Pinout::MOTOR_IN2);
//   gpio_reset_pin((gpio_num_t)Pinout::VMOTOR_PG);

//   Log.println("\n╔════════════════════════════════════╗");
//   Log.println("║  MOTOR CONTROLLER BOOT SEQUENCE    ║");
//   Log.println("╚════════════════════════════════════╝\n");

//   // Step 1: Force Stop (IN/IN Brake)
//   pinMode(Pinout::MOTOR_IN1, OUTPUT);
//   pinMode(Pinout::MOTOR_IN2, OUTPUT);
//   digitalWrite(Pinout::MOTOR_IN1, LOW);
//   digitalWrite(Pinout::MOTOR_IN2, LOW);
//   delay(100);

//   // --- FIX 2: WAKE DRV8213 FROM SLEEP ---
//   // Must set to INPUT (High-Z) to wake the chip. 
//   // DO NOT use digitalWrite(..., LOW) as it sleeps the driver.
//   pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
//   pinMode(Pinout::MOTOR_CURRENT, INPUT); // GPIO 5
//   pinMode(Pinout::VMOTOR_PG, INPUT_PULLUP);
//   delay(50); // Wake-up time (~1ms required)

//   // Step 3: Configure ADC
//   analogReadResolution(12);
//   analogSetAttenuation(ADC_0db); // 1.1V Range for Max Sensitivity

//   // Step 4: Symmetric PWM Setup
//   ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
//   ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
//   ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
//   ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

//   ledcWrite(_pwmChannel1, 0);
//   ledcWrite(_pwmChannel2, 0);

//   // Step 5: High-Precision Calibration (Peak Method)
//   Log.println("Step 5: Calibrating current sensor...");
//   delay(500); 
//   float sumOffset = 0;
//   for (int i = 0; i < 100; i++) {
//     sumOffset += (float)_getPeakADC();
//     delay(2);
//   }
//   _currentOffset = sumOffset / 100.0f;
  
//   // If this is still 0.0, your physical R10 resistor is missing or broken.
//   Log.printf("  ✓ ADC Peak Offset = %.1f counts\n", _currentOffset);

//   _loadBaselineTable();

//   // Set Optimal Defaults for 5-pole motor if not already set
//   NmraDcc &dcc = DccController::getInstance().getDcc();
//   if (dcc.getCV(CV::V_START) == 0) {
//     Log.println("Step 7: Writing default CVs...");
//     dcc.setCV(CV::V_START, 60);
//     dcc.setCV(CV::PEDESTAL_FLOOR, 80);
//     dcc.setCV(CV::ACCEL, 25);
//     dcc.setCV(CV::DECEL, 20);
//     dcc.setCV(CV::LOAD_GAIN, 120);
//     dcc.setCV(CV::LOAD_GAIN_SCALAR, 80);
//     dcc.setCV(CV::STICTION_KICK, 80);
//     dcc.setCV(CV::BASELINE_ALPHA, 5);
//     dcc.setCV(CV::HARDWARE_GAIN, 1); // Default to Wake/High-Z
//     Log.println("  ✓ CVs written");
//   }

//   Log.printf("Power Good: %s\n", digitalRead(Pinout::VMOTOR_PG) == HIGH ? "✓ OK" : "✗ FAIL");
//   _drive(0, true);
// }

// uint32_t MotorController::_getPeakADC() {
//     uint32_t maxVal = 0;
//     // Burst sample to find the ON-time current peak
//     for(int i=0; i<32; i++) {
//         uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
//         if (s > maxVal) maxVal = s;
//     }
//     return maxVal;
// }

// void MotorController::loop() {
//   uint8_t targetSpeed;
//   bool reqDirection;

//   {
//     SystemContext &ctx = SystemContext::getInstance();
//     ScopedLock lock(ctx);
//     SystemState &state = ctx.getState();
//     targetSpeed = state.speed; // 0-255 range
//     reqDirection = state.direction;

//     if (state.lastDccPacketTime > 0 && (millis() - state.lastDccPacketTime) > 2000) {
//       _drive(0, true); _currentSpeed = 0; return;
//     }
//   }

//   targetSpeed = constrain(targetSpeed, 0, 255);
//   _updateCvCache();

//   // --- PEAK-BURST ADC FILTERING ---
//   // Capture the highest value in a burst to ignore PWM 'off' windows
//   float rawPeak = (float)_getPeakADC();

//   _adcHistory[_adcIdx] = rawPeak;
//   _adcIdx = (_adcIdx + 1) % 16;
//   float sumHistory = 0;
//   for (int i = 0; i < 16; i++) sumHistory += _adcHistory[i];
//   float smoothedPeak = sumHistory / 16.0f;

//   if (targetSpeed == 0 && _currentSpeed < 0.1f) {
//     _currentOffset = (_currentOffset * 0.995f) + (smoothedPeak * 0.005f);
//     _torqueIntegrator = 0.0f;
//     _startupKickActive = false;
//   }

//   // --- NOISE FLOOR DEADBAND ---
//   float netPeak = smoothedPeak - _currentOffset;
//   if (netPeak < 5.0f) netPeak = 0.0f; // Tight deadband

//   // Scaling for High-Z (1050 uA/A) + 2.4k Resistor + 0dB ADC (1.1V range)
//   // Amps = (netPeak / 4095 * 1.1V) / (1050e-6 * 2400)
//   _avgCurrent = netPeak * 0.000106f; 

//   unsigned long now = millis();

//   if (_avgCurrent > _stallThreshold) {
//     if (_stallStartTime == 0) _stallStartTime = now;
//     else if (now - _stallStartTime > 2000) {
//       _drive(0, true); _currentSpeed = 0;
//       Log.printf("SAFETY: MOTOR STALL DETECTED!\n");
//       return;
//     }
//   } else _stallStartTime = 0;

//   // Momentum
//   unsigned long dt = now - _lastMomentumUpdate;
//   if (dt >= 5) {
//     _lastMomentumUpdate = now;
//     float accelDelay = max(1, (int)_cvAccel) * 3.5f;
//     float decelDelay = max(1, (int)_cvDecel) * 3.5f;
//     float step = (float)dt / (targetSpeed > _currentSpeed ? accelDelay : decelDelay);
//     if (step > 2.0f) step = 2.0f; 
//     if (targetSpeed > _currentSpeed) _currentSpeed = min((float)targetSpeed, _currentSpeed + step);
//     else if (targetSpeed < _currentSpeed) _currentSpeed = max((float)targetSpeed, _currentSpeed - step);
//   }

//   // Direction Change
//   if (reqDirection != _currentDirection) {
//     if (_currentSpeed < 0.5f) {
//       _currentDirection = reqDirection;
//     } else {
//       _currentSpeed = max(0.0f, _currentSpeed - 5.0f);
//       return; 
//     }
//   }

//   int32_t pwmOutput = 0;
//   int speedIndex = constrain((int)_currentSpeed, 0, 255);

//   if (_currentSpeed > 0.1f) {
//     float speedNorm = _currentSpeed / 255.0f; 
//     uint32_t pwmStart = max((uint32_t)_cvVstart, (uint32_t)_cvPedestalFloor) * 16;
//     if (pwmStart < 205) pwmStart = 205; 

//     float basePwm = (float)pwmStart + (speedNorm * (float)(_maxPwm - pwmStart));

//     if (_cvConfig & 0x10) {
//       int tableIdx = constrain((int)(speedNorm * 27.0f), 0, 27);
//       speedNorm = (float)_speedTable[tableIdx] / 255.0f;
//       basePwm = (float)pwmStart + (speedNorm * (float)(_maxPwm - pwmStart));
//     }

//     // PI Load Compensation
//     float deltaI = _avgCurrent - _baselineTable[speedIndex];
//     if (deltaI < 0) deltaI = 0;

//     float Kp = 4.0f * ((float)_cvLoadGain / 255.0f);
//     float Ki = 0.04f * ((float)_cvLoadGainScalar / 255.0f);

//     _torqueIntegrator += deltaI * Ki;
//     float maxComp = ((float)_cvDeltaCap / 255.0f) * 500.0f;
//     _torqueIntegrator = constrain(_torqueIntegrator, 0.0f, maxComp);

//     if (deltaI < 0.005f) _torqueIntegrator *= 0.95f;

//     pwmOutput = (int32_t)(basePwm + (deltaI * Kp) + _torqueIntegrator);

//     // Startup Kick
//     if (_currentSpeed > 0.1f && _currentSpeed < 4.0f) {
//       if (!_startupKickActive) { _startupKickActive = true; _startupKickTime = now; pwmOutput += 200; } 
//       if (now - _startupKickTime > 150) _startupKickActive = false;
//     } else _startupKickActive = false;

//     // Baseline Learning
//     if (_avgCurrent > 0.005f && _currentSpeed > (float)_cvLearnThreshold && (now - _lastBaselineUpdate > 500)) {
//       _lastBaselineUpdate = now;
//       float alpha = ((float)_cvBaselineAlpha / 1000.0f);
//       _baselineTable[speedIndex] = (_baselineTable[speedIndex] * (1.0f - alpha)) + (_avgCurrent * alpha);
//     }
//   }

//   pwmOutput = constrain(pwmOutput, 0, (int32_t)_maxPwm);

//   int32_t pwmDelta = pwmOutput - _lastSmoothingPwm;
//   if (abs(pwmDelta) > 150) pwmOutput = _lastSmoothingPwm + (pwmDelta > 0 ? 150 : -150);
//   _lastSmoothingPwm = (uint16_t)pwmOutput;

//   _drive((uint16_t)pwmOutput, _currentDirection);

//   static unsigned long lastStream = 0;
//   if (now - lastStream > 1000) { lastStream = now; streamTelemetry(); }
// }

// /**
//  * SLOW DECAY (BRAKE-DRIVE)
//  * Essential for 5-pole motor torque.
//  */
// void MotorController::_drive(uint16_t speed, bool direction) {
//   _lastPwmValue = (int16_t)speed;

//   if (speed == 0) {
//     // Active Brake (Both High) keeps the current mirror active during stops
//     ledcWrite(_pwmChannel1, 4095);
//     ledcWrite(_pwmChannel2, 4095);
//     return;
//   }

//   // Slow Decay (Brake-Drive) Logic:
//   // Forward: IN1=HIGH, IN2=PWM_INV
//   // Reverse: IN1=PWM_INV, IN2=HIGH
//   uint32_t duty = _maxPwm - speed;

//   if (direction) {
//     ledcWrite(_pwmChannel1, _maxPwm);
//     ledcWrite(_pwmChannel2, duty);
//   } else {
//     ledcWrite(_pwmChannel1, duty);
//     ledcWrite(_pwmChannel2, _maxPwm);
//   }
// }

// void MotorController::_updateCvCache() {
//   if (millis() - _lastCvUpdate > 500) {
//     _lastCvUpdate = millis();
//     NmraDcc &dcc = DccController::getInstance().getDcc();

//     _cvAccel = dcc.getCV(CV::ACCEL);
//     _cvDecel = dcc.getCV(CV::DECEL);
//     _cvVstart = dcc.getCV(CV::V_START);
//     _cvVmid = dcc.getCV(CV::V_MID);
//     _cvVhigh = dcc.getCV(CV::V_HIGH);
//     _cvConfig = dcc.getCV(CV::CONFIG);
//     _cvLoadGain = dcc.getCV(CV::LOAD_GAIN);
//     _cvBaselineAlpha = dcc.getCV(CV::BASELINE_ALPHA);
//     _cvStictionKick = dcc.getCV(CV::STICTION_KICK);
//     _cvDeltaCap = dcc.getCV(CV::DELTA_CAP);
//     _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
//     _cvBaselineReset = dcc.getCV(CV::BASELINE_RESET);
//     _cvDriveMode = dcc.getCV(CV::DRIVE_MODE);
//     _cvPedestalFloor = dcc.getCV(CV::PEDESTAL_FLOOR);
//     _cvLoadGainScalar = dcc.getCV(CV::LOAD_GAIN_SCALAR);
//     _cvLearnThreshold = dcc.getCV(CV::LEARN_THRESHOLD);
//     _cvHardwareGain = dcc.getCV(CV::HARDWARE_GAIN);

//     // FIX: Only switch if changed to avoid PWM glitches
//     static uint8_t lastHardwareGain = 255;
//     if (_cvHardwareGain != lastHardwareGain) {
//       lastHardwareGain = _cvHardwareGain;
//       if (_cvHardwareGain == 1) {
//         pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); // Wake High-Z
//       } else {
//         pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
//         digitalWrite(Pinout::MOTOR_GAIN_SEL, _cvHardwareGain == 2 ? HIGH : LOW);
//       }
//     }

//     uint8_t newIntensity = dcc.getCV(CV::CURVE_INTENSITY);
//     if (newIntensity != _cvCurveIntensity) {
//       _cvCurveIntensity = newIntensity;
//       if (_cvCurveIntensity > 0) {
//         _generateScurve(_cvCurveIntensity);
//         for (int i=0; i<28; i++) dcc.setCV(67 + i, _speedTable[i]);
//       }
//     }
//   }
// }

// void MotorController::_generateScurve(uint8_t intensity) {
//   float k = 0.1f + ((float)intensity / 255.0f) * 11.9f;
//   for (int i = 0; i < 28; i++) {
//     float x = (float)i / 27.0f;
//     float sigmoid = 1.0f / (1.0f + exp(-k * (x - 0.5f)));
//     float s0 = 1.0f / (1.0f + exp(-k * (0.0f - 0.5f)));
//     float s1 = 1.0f / (1.0f + exp(-k * (1.0f - 0.5f)));
//     float normalized = (sigmoid - s0) / (s1 - s0);
//     _speedTable[i] = (uint8_t)(normalized * 255.0f);
//   }
// }

// void MotorController::streamTelemetry() {
//   SystemState &state = SystemContext::getInstance().getState();
//   int rawAdc = analogRead(Pinout::MOTOR_CURRENT);
//   float netAdc = (float)rawAdc - _currentOffset;

//   Log.printf("[NIMRS_DATA],%d,%.2f,%d,%.3f,%.2f,%d,%d,%.1f,%d,%.1f\n",
//              state.speed, _currentSpeed, _lastPwmValue, _avgCurrent,
//              state.loadFactor, 0, rawAdc, _currentOffset,
//              digitalRead(Pinout::MOTOR_GAIN_SEL), netAdc);
// }

// void MotorController::_saveBaselineTable() {
//   Preferences prefs;
//   prefs.begin("motor", false);
//   prefs.putBytes("baseline", _baselineTable, sizeof(_baselineTable));
//   prefs.end();
// }

// void MotorController::_loadBaselineTable() {
//   Preferences prefs;
//   prefs.begin("motor", true);
//   size_t len = prefs.getBytes("baseline", _baselineTable, sizeof(_baselineTable));
//   prefs.end();
//   if (len != sizeof(_baselineTable)) {
//     for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.0f;
//   }
// }


#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include <cmath>
#include <algorithm>

MotorController::MotorController() : 
    _currentDirection(true), 
    _currentSpeed(0.0f), 
    _lastMomentumUpdate(0),
    _lastCvUpdate(0),
    _avgCurrent(0.0f),
    _currentOffset(0.0f),
    _pwmResolution(10),
    _maxPwm(1023),
    _pwmFreq(16000)
{
    // Prototypical S-Curve Default
    uint8_t sCurve[] = {1, 3, 7, 12, 18, 26, 35, 46, 58, 72, 87, 103, 119, 135, 
                        151, 166, 181, 194, 206, 217, 226, 234, 241, 246, 250, 253, 254, 255};
    for (int i = 0; i < 28; i++) _speedTable[i] = sCurve[i];
    for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.0f;
}

void MotorController::setup() {
    Log.println("NIMRS: Initializing Blended Motor Control (10-bit / 16kHz)...");

    pinMode(Pinout::MOTOR_IN1, OUTPUT);
    pinMode(Pinout::MOTOR_IN2, OUTPUT);
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); // High-Z / Wake Driver
    pinMode(Pinout::MOTOR_CURRENT, INPUT);
    pinMode(Pinout::VMOTOR_PG, INPUT_PULLUP); 

    // Maximize sensitivity for the tiny IPROPI current mirror signal
    analogReadResolution(12);
    analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db); 

    ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
    ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
    ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
    ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);

    // Establish the 'Zero' baseline using the Peak-Burst method
    delay(500);
    float sum = 0;
    for(int i=0; i<50; i++) { sum += (float)_getPeakADC(); delay(5); }
    _currentOffset = sum / 50.0f;

    _loadBaselineTable();
    _drive(0, true);
    
    Log.printf("✓ Motor Hardware Ready. Peak Offset: %.1f | Gain: %s\n", 
                _currentOffset, digitalRead(Pinout::MOTOR_GAIN_SEL) ? "HI-Z" : "LOW");
}

/**
 * THE SENSOR FIX:
 * Burst-samples to catch the PWM 'ON' peaks, ignoring the '0' gaps.
 */
uint32_t MotorController::_getPeakADC() {
    uint32_t maxVal = 0;
    // Sample long enough to span ~5 full PWM cycles at 16kHz
    for(int i = 0; i < 32; i++) {
        uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
        if (s > maxVal) maxVal = s;
    }
    return maxVal;
}

void MotorController::loop() {
    uint8_t targetSpeed;
    bool reqDirection;

    {
        SystemContext &ctx = SystemContext::getInstance();
        ScopedLock lock(ctx);
        targetSpeed = ctx.getState().speed;
        reqDirection = ctx.getState().direction;
    }

    _updateCvCache();

    // 1. Current Sensing (Using Peak-Burst)
    float rawPeak = (float)_getPeakADC();
    float netPeak = rawPeak - _currentOffset;
    if (netPeak < 5.0f) netPeak = 0;

    // Scaling for 1050uA/A Gain + 2.4k Resistor
    float currentAmps = netPeak * 0.000106f; 
    _avgCurrent = (_avgCurrent * 0.95f) + (currentAmps * 0.05f);

    // 2. Momentum
    unsigned long now = millis();
    unsigned long dt = now - _lastMomentumUpdate;
    if (dt >= 5) {
        _lastMomentumUpdate = now;
        float accelDelay = max(1, (int)_cvAccel) * 3.5f;
        float decelDelay = max(1, (int)_cvDecel) * 3.5f;
        float step = (float)dt / (targetSpeed > _currentSpeed ? accelDelay : decelDelay);
        if (targetSpeed > _currentSpeed) _currentSpeed = min((float)targetSpeed, _currentSpeed + step);
        else if (targetSpeed < _currentSpeed) _currentSpeed = max((float)targetSpeed, _currentSpeed - step);
    }

    if (_currentSpeed < 1.0f && reqDirection != _currentDirection) _currentDirection = reqDirection;

    // 3. Drive Logic (With Hard Pedestal to overcome stiction)
    int32_t pwmOutput = 0;
    if (_currentSpeed > 0.1f) {
        // Pedestal to ensure motor starts turning immediately
        uint32_t pwmStart = max((uint32_t)_cvVstart, (uint32_t)100) * 4; 
        float speedNorm = _currentSpeed / 255.0f;
        pwmOutput = (int32_t)((float)pwmStart + (speedNorm * (float)(_maxPwm - pwmStart)));
        
        // Note: PI Loop integration happens here next!
    }

    pwmOutput = constrain(pwmOutput, 0, (int32_t)_maxPwm);
    _drive((uint16_t)pwmOutput, _currentDirection);

    streamTelemetry();
}

void MotorController::_drive(uint16_t speed, bool direction) {
    _lastPwmValue = (int16_t)speed;
    if (speed == 0) {
        // Brake: Both HIGH
        ledcWrite(_pwmChannel1, _maxPwm);
        ledcWrite(_pwmChannel2, _maxPwm);
        return;
    }

    // Slow Decay Drive (Brake-Drive)
    uint32_t invertedPwm = _maxPwm - speed;
    if (direction) {
        ledcWrite(_pwmChannel1, _maxPwm);
        ledcWrite(_pwmChannel2, invertedPwm); 
    } else {
        ledcWrite(_pwmChannel1, invertedPwm);
        ledcWrite(_pwmChannel2, _maxPwm);
    }
}

void MotorController::streamTelemetry() {
    static unsigned long lastStream = 0;
    if (millis() - lastStream > 200) {
        lastStream = millis();
        SystemState &state = SystemContext::getInstance().getState();
        
        // Net value for Dashboard Debug
        float currentNet = (float)_getPeakADC() - _currentOffset;

        Log.printf("[NIMRS_DATA],%d,%.2f,%d,%.3f,%.2f,%d,%d,%.1f,%d,%.1f\n",
            state.speed, _currentSpeed, _lastPwmValue, _avgCurrent, state.loadFactor,
            0, analogRead(Pinout::MOTOR_CURRENT), _currentOffset,
            digitalRead(Pinout::MOTOR_GAIN_SEL), currentNet
        );
    }
}

void MotorController::_updateCvCache() {
    if (millis() - _lastCvUpdate > 500) {
        _lastCvUpdate = millis();
        NmraDcc &dcc = DccController::getInstance().getDcc();
        _cvAccel = dcc.getCV(CV::ACCEL);
        _cvDecel = dcc.getCV(CV::DECEL);
        _cvVstart = dcc.getCV(CV::V_START);
        _cvLoadGain = dcc.getCV(CV::LOAD_GAIN);
        
        // Force Wake if CV 148 is set to 1
        if (dcc.getCV(CV::HARDWARE_GAIN) == 1) {
            pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
        }
    }
}

void MotorController::_saveBaselineTable() {
    Preferences prefs;
    prefs.begin("motor", false);
    prefs.putBytes("baseline", _baselineTable, sizeof(_baselineTable));
    prefs.end();
}

void MotorController::_loadBaselineTable() {
    Preferences prefs;
    prefs.begin("motor", true);
    prefs.getBytes("baseline", _baselineTable, sizeof(_baselineTable));
    prefs.end();
}

void MotorController::_generateScurve(uint8_t i) {} // Stub
