#include "MotorHal.h"
#include "Logger.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <cmath>
#include <driver/mcpwm.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

MotorHal::MotorHal()
    : _sampleRate(100000.0f), _lastGain(255), _adcHandle(NULL) {}

MotorHal &MotorHal::getInstance() {
  static MotorHal instance;
  return instance;
}

void MotorHal::init() {
  // --- 0. Pin Modes ---
  gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);
  pinMode(Pinout::MOTOR_CURRENT, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(Pinout::MOTOR_CURRENT, ADC_11db);

  // Default Gain Mode: High-Z (Medium)
  setHardwareGain(1);

  // --- 1. MCPWM Setup ---
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, Pinout::MOTOR_IN1);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, Pinout::MOTOR_IN2);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 20000;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

  // Start timer once and keep it running
  mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);

  // Initial State: Brake
  setDuty(0.0f);

  Log.println("MotorHal: Initialized (Fixed Brake Logic)");
}

void MotorHal::setHardwareGain(uint8_t mode) {
  if (_lastGain == mode)
    return;
  _lastGain = mode;

  switch (mode) {
  case 0: // Low
    pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
    digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW);
    break;
  case 1: // High-Z
  default:
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
    gpio_set_pull_mode((gpio_num_t)Pinout::MOTOR_GAIN_SEL, GPIO_FLOATING);
    break;
  case 2: // High
    pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
    digitalWrite(Pinout::MOTOR_GAIN_SEL, HIGH);
    break;
  }
}

void MotorHal::setDuty(float duty) {
  // CRITICAL: Stop logic must be extremely robust
  if (fabs(duty) < 0.01f) {
    // Both High = BRAKE (Slow Decay)
    // We use generator level override to force signal HIGH regardless of
    // timer/duty
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
    return;
  }

  if (duty > 1.0f)
    duty = 1.0f;
  if (duty < -1.0f)
    duty = -1.0f;
  float dutyPercent = fabs(duty) * 100.0f;

  if (duty > 0) {
    // FWD: IN1 High, IN2 PWM Active Low
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, dutyPercent);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B,
                        MCPWM_DUTY_MODE_1);
  } else {
    // REV: IN2 High, IN1 PWM Active Low
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, dutyPercent);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
                        MCPWM_DUTY_MODE_1);
  }
}

size_t MotorHal::getAdcSamples(float *buffer, size_t maxLen) {
  size_t count = std::min(maxLen, (size_t)128);

  uint32_t start = micros();
  for (size_t i = 0; i < count; i++) {
    buffer[i] = (float)analogRead(Pinout::MOTOR_CURRENT);
  }
  uint32_t end = micros();

  float dt = (float)(end - start) / 1000000.0f;
  if (dt > 0) {
    _sampleRate = (float)count / dt;
  }

  return count;
}

float MotorHal::getAdcSampleRate() const { return _sampleRate; }
