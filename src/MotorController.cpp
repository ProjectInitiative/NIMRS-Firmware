// MotorController.cpp - Improved for 5-pole skewed HO slow crawl

#include "MotorController.h"
#include "CvRegistry.h"
#include "Logger.h"
#include <algorithm>
#include <cmath>

// --- Tuning Constants (easier to find and adjust) ---
// Broadway Limited motors typically need ~220-260 counts minimum at the rail
static constexpr float  CRAWL_BASE_PWM       = 240.0f;  // Start lower than 280
static constexpr float  CRAWL_SPEED_THRESH   = 12.0f;   // Speed steps considered "crawl"
static constexpr float  LOAD_THRESH_CRAWL    = 0.30f;   // Amps: free-running crawl target
static constexpr float  LOAD_THRESH_NORMAL   = 0.55f;
static constexpr float  STALL_CURRENT        = 0.90f;   // Amps: probable stall
static constexpr float  STALL_KICK_PWM       = 180.0f;  // Extra counts added on stall detect
static constexpr float  INTEGRATOR_MAX       = 250.0f;
static constexpr float  INTEGRATOR_MIN       = -180.0f; // Less braking headroom than boosting
static constexpr float  SLEW_NORMAL          = 2.5f;
static constexpr float  SLEW_STALL_RECOVERY  = 12.0f;   // Fast slew on stall recovery
static constexpr float  CURRENT_FILTER_SLOW  = 0.92f;   // For steady-state load reading
static constexpr float  CURRENT_FILTER_FAST  = 0.70f;   // For spike/stall detection
static constexpr int    ADC_SAMPLES          = 80;       // Slightly fewer, faster loop
static constexpr float  OFFSET_ADAPT_RATE    = 0.002f;  // How fast offset self-calibrates

MotorController::MotorController() : 
    _currentDirection(true), _currentSpeed(0.0f), _lastMomentumUpdate(0),
    _avgCurrent(0.0f), _fastCurrent(0.0f), _currentOffset(5.0f),
    _torqueIntegrator(0.0f), _stallKickActive(false), _stallKickTimer(0),
    _maxPwm(1023), _pwmFreq(100)
{
    for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.05f;
}

void MotorController::setup() {
    Log.println("NIMRS: Initializing High-Torque Crawl Mode (100Hz / Dual-Filter Governor)...");
    pinMode(Pinout::MOTOR_IN1, OUTPUT);
    pinMode(Pinout::MOTOR_IN2, OUTPUT);
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); 
    pinMode(Pinout::MOTOR_CURRENT, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_0db);
    ledcSetup(_pwmChannel1, 100, 10);
    ledcSetup(_pwmChannel2, 100, 10);
    ledcAttachPin(Pinout::MOTOR_IN1, _pwmChannel1);
    ledcAttachPin(Pinout::MOTOR_IN2, _pwmChannel2);
    
    // Self-calibrate offset at rest (motor stopped, no current flowing)
    // Sample for 50ms to get a stable zero-current baseline
    float offsetAccum = 0.0f;
    for (int i = 0; i < 50; i++) {
        offsetAccum += (float)analogRead(Pinout::MOTOR_CURRENT);
        delay(1);
    }
    _currentOffset = offsetAccum / 50.0f;
    Log.printf("NIMRS: ADC offset calibrated to %.2f counts\n", _currentOffset);
    
    _drive(0, true);
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

    // === 1. DIRECTION CHANGE HANDLING ===
    // On direction change, reset integrator to avoid carry-over momentum fighting you
    if (reqDirection != _currentDirection) {
        _currentDirection = reqDirection;
        _torqueIntegrator = 0.0f;
        _avgCurrent = 0.0f;
        _fastCurrent = 0.0f;
        _stallKickActive = false;
    }

    // === 2. MOMENTUM (unchanged, works fine) ===
    unsigned long now = millis();
    unsigned long dt = now - _lastMomentumUpdate;
    if (dt >= 5) {
        _lastMomentumUpdate = now;
        float accelDelay = std::max(1, (int)_cvAccel) * 10.0f; 
        float step = (float)dt / accelDelay;
        if (targetSpeed > _currentSpeed)
            _currentSpeed = std::min((float)targetSpeed, _currentSpeed + step);
        else if (targetSpeed < _currentSpeed)
            _currentSpeed = std::max((float)targetSpeed, _currentSpeed - step);
    }

    // === 3. DUAL-SPEED CURRENT SENSING ===
    float rawPeak = (float)_getPeakADC();
    
    // Adaptive offset: when motor is stopped, drift the offset toward actual reading
    // This prevents the "always in boost" bug if offset drifts over temperature
    if (_currentSpeed < 0.1f) {
        _currentOffset += (rawPeak - _currentOffset) * OFFSET_ADAPT_RATE;
    }
    
    float netPeak = std::max(0.0f, rawPeak - _currentOffset);
    float currentAmps = netPeak * 0.00054f;

    // Slow filter: steady-state load tracking (what your original had)
    _avgCurrent = (_avgCurrent * CURRENT_FILTER_SLOW) + (currentAmps * (1.0f - CURRENT_FILTER_SLOW));
    
    // Fast filter: stall/spike detection — reacts in ~3-5 loop cycles
    _fastCurrent = (_fastCurrent * CURRENT_FILTER_FAST) + (currentAmps * (1.0f - CURRENT_FILTER_FAST));

    // === 4. STALL DETECTION & KICK ===
    // A skewed 5-pole motor stalls between poles at low speed. 
    // Detect it fast and inject a brief power kick to push past the dead spot.
    bool isStalled = (_fastCurrent > STALL_CURRENT) && (_currentSpeed > 0.1f);
    
    if (isStalled && !_stallKickActive) {
        _stallKickActive = true;
        _stallKickTimer = now;
        // Immediately boost integrator to kick past the dead pole
        _torqueIntegrator = std::min(_torqueIntegrator + STALL_KICK_PWM, INTEGRATOR_MAX);
        Log.println("NIMRS: Stall kick fired");
    }
    // Cancel kick after 80ms — enough for one pole pitch at crawl speed
    if (_stallKickActive && (now - _stallKickTimer > 80)) {
        _stallKickActive = false;
    }

    // === 5. THE IMPROVED GOVERNOR ===
    int32_t finalPwm = 0;

    if (_currentSpeed > 0.1f) {
        bool isCrawl = (_currentSpeed < CRAWL_SPEED_THRESH);
        
        // Speed-scaled base PWM
        // At crawl speeds, use a lower, flatter curve — the motor needs
        // consistent torque, not a steep ramp that overshoots
        float speedNorm = _currentSpeed / 255.0f;
        float basePwm;
        if (isCrawl) {
            // Flat crawl floor: stays near CRAWL_BASE_PWM with slight scaling
            basePwm = CRAWL_BASE_PWM + (speedNorm * 80.0f);
        } else {
            basePwm = CRAWL_BASE_PWM + (speedNorm * (float)(_maxPwm - (int)CRAWL_BASE_PWM));
        }

        // Load threshold scales with speed — at higher speeds, more current is normal
        float loadThreshold = isCrawl ? LOAD_THRESH_CRAWL : LOAD_THRESH_NORMAL;
        
        // Use slow filter for governor, fast filter for stall only
        float loadError = _avgCurrent - loadThreshold;

        if (_avgCurrent < 0.015f) {
            // True zero current: motor likely not moving at all or ADC fault
            // Ramp up, but slower than before to avoid overshoot on light locos
            if (!_stallKickActive) _torqueIntegrator += 0.8f;
        } else {
            // Normal PI-style correction
            // Positive loadError = drawing too much = slow down (subtract)
            // Negative loadError = too light = speed up (add)
            float settleRate = (loadError > 0) ? 2.0f : 0.5f;
            _torqueIntegrator -= (loadError * settleRate);
            _torqueIntegrator = constrain(_torqueIntegrator, INTEGRATOR_MIN, INTEGRATOR_MAX);
        }

        float targetPwm = basePwm + _torqueIntegrator;

        // Variable slew: fast recovery from stall, slow otherwise
        float slewRate = _stallKickActive ? SLEW_STALL_RECOVERY : SLEW_NORMAL;
        float diff = targetPwm - _slewedPwm;
        _slewedPwm += constrain(diff, -slewRate, slewRate);
        finalPwm = (int32_t)_slewedPwm;

    } else {
        // Stopped: bleed down slew and reset integrator
        _torqueIntegrator = 0.0f;
        _slewedPwm *= 0.85f; // Soft stop, avoids hard jerk to zero
        if (_slewedPwm < 1.0f) _slewedPwm = 0.0f;
        finalPwm = (int32_t)_slewedPwm;
    }

    _drive((uint16_t)constrain(finalPwm, 0, 1023), _currentDirection);
    
    // loadFactor now represents integrator relative to its asymmetric range
    float loadFactor = (_torqueIntegrator - INTEGRATOR_MIN) / (INTEGRATOR_MAX - INTEGRATOR_MIN);
    SystemContext::getInstance().getState().loadFactor = loadFactor;
    streamTelemetry();
}

void MotorController::_updateCvCache() {
  if (millis() - _lastCvUpdate > 500) {
    _lastCvUpdate = millis();
    NmraDcc &dcc = DccController::getInstance().getDcc();

    _cvAccel = dcc.getCV(CV::ACCEL);
    _cvDecel = dcc.getCV(CV::DECEL);
    _cvVstart = dcc.getCV(CV::V_START);
    _cvVmid = dcc.getCV(CV::V_MID);
    _cvVhigh = dcc.getCV(CV::V_HIGH);
    _cvConfig = dcc.getCV(CV::CONFIG);
    _cvLoadGain = dcc.getCV(CV::LOAD_GAIN);
    _cvBaselineAlpha = dcc.getCV(CV::BASELINE_ALPHA);
    _cvStictionKick = dcc.getCV(CV::STICTION_KICK);
    _cvDeltaCap = dcc.getCV(CV::DELTA_CAP);
    _cvPwmDither = dcc.getCV(CV::PWM_DITHER);
    _cvBaselineReset = dcc.getCV(CV::BASELINE_RESET);
    _cvDriveMode = dcc.getCV(CV::DRIVE_MODE);
    _cvPedestalFloor = dcc.getCV(CV::PEDESTAL_FLOOR);
    _cvLoadGainScalar = dcc.getCV(CV::LOAD_GAIN_SCALAR);
    _cvLearnThreshold = dcc.getCV(CV::LEARN_THRESHOLD);
    _cvHardwareGain = dcc.getCV(CV::HARDWARE_GAIN);

    // FIX: Only switch if changed to avoid PWM glitches
    static uint8_t lastHardwareGain = 255;
    if (_cvHardwareGain != lastHardwareGain) {
      lastHardwareGain = _cvHardwareGain;
      if (_cvHardwareGain == 1) {
        pinMode(Pinout::MOTOR_GAIN_SEL, INPUT); // Wake High-Z
        _activeGainMode = 1;
      } else if (_cvHardwareGain == 2) {
        pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
        digitalWrite(Pinout::MOTOR_GAIN_SEL, HIGH);
        _activeGainMode = 2;
      } else {
        pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
        digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW);
        _activeGainMode = 0;
      }
    }

    uint8_t newIntensity = dcc.getCV(CV::CURVE_INTENSITY);
    if (newIntensity != _cvCurveIntensity) {
      _cvCurveIntensity = newIntensity;
      if (_cvCurveIntensity > 0) {
        _generateScurve(_cvCurveIntensity);
        for (int i=0; i<28; i++) dcc.setCV(67 + i, _speedTable[i]);
      }
    }
  }
}

void MotorController::_generateScurve(uint8_t intensity) {
  float k = 0.1f + ((float)intensity / 255.0f) * 11.9f;
  for (int i = 0; i < 28; i++) {
    float x = (float)i / 27.0f;
    float sigmoid = 1.0f / (1.0f + exp(-k * (x - 0.5f)));
    float s0 = 1.0f / (1.0f + exp(-k * (0.0f - 0.5f)));
    float s1 = 1.0f / (1.0f + exp(-k * (1.0f - 0.5f)));
    float normalized = (sigmoid - s0) / (s1 - s0);
    _speedTable[i] = (uint8_t)(normalized * 255.0f);
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
  size_t len = prefs.getBytes("baseline", _baselineTable, sizeof(_baselineTable));
  prefs.end();
  if (len != sizeof(_baselineTable)) {
    for (int i = 0; i < SPEED_STEPS; i++) _baselineTable[i] = 0.05f;
  }
}

uint32_t MotorController::_getPeakADC() {
    uint32_t maxVal = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        uint32_t s = analogRead(Pinout::MOTOR_CURRENT);
        if (s > maxVal) maxVal = s;
    }
    return maxVal;
}

void MotorController::_drive(uint16_t speed, bool direction) {
    _lastPwmValue = (int16_t)speed;
    if (speed == 0) {
        ledcWrite(_pwmChannel1, _maxPwm);
        ledcWrite(_pwmChannel2, _maxPwm); 
        return;
    }
    uint32_t pulse = _maxPwm - speed;
    if (direction) {
        ledcWrite(_pwmChannel1, _maxPwm);
        ledcWrite(_pwmChannel2, pulse); 
    } else {
        ledcWrite(_pwmChannel1, pulse);
        ledcWrite(_pwmChannel2, _maxPwm);
    }
}

void MotorController::streamTelemetry() {
    static unsigned long lastS = 0;
    if (millis() - lastS > 150) {
        lastS = millis();
        SystemState &state = SystemContext::getInstance().getState();
        Log.printf("[NIMRS_DATA],%d,%.1f,%d,%.3f,%.3f,%.3f,%d,%.1f,%.1f,%d\n",
            state.speed, _currentSpeed, _lastPwmValue,
            _avgCurrent, _fastCurrent,        // Now logging both filters
            _torqueIntegrator,                 // Raw integrator value (easier to tune)
            (int)_stallKickActive,             // See when kicks fire
            _currentOffset,                    // Watch for drift
            _slewedPwm,                         // Actual commanded PWM before constrain
            (int)_getPeakADC());
    }
}