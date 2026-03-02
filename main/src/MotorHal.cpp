#include "MotorHal.h"
#include "Logger.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <cmath>
#include <driver/mcpwm.h>
#include <esp_adc/adc_continuous.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

// ADC Configuration
#define ADC_READ_LEN 1024
#define ADC_CHAN ADC_CHANNEL_4 // GPIO 5 on S3 ADC1

// DMA requires word-aligned buffers for the S3
static uint32_t result_data_aligned[ADC_READ_LEN / 4];
static uint8_t *result_data = (uint8_t *)result_data_aligned;

MotorHal::MotorHal() : _sampleRate(20000.0f), _adcHandle(NULL) {}

MotorHal &MotorHal::getInstance() {
  static MotorHal instance;
  return instance;
}

void MotorHal::init() {
  // --- 0. Pin Modes ---
  pinMode(Pinout::MOTOR_CURRENT, INPUT);
  gpio_set_pull_mode((gpio_num_t)Pinout::MOTOR_CURRENT, GPIO_FLOATING);

  // Match old firmware: Truly High-Z (Floating)
  pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
  gpio_set_pull_mode((gpio_num_t)Pinout::MOTOR_GAIN_SEL, GPIO_FLOATING);

  // --- 1. MCPWM Setup (Legacy) ---
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, Pinout::MOTOR_IN1);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, Pinout::MOTOR_IN2);

  mcpwm_config_t pwm_config;
  pwm_config.frequency = 20000;
  pwm_config.cmpr_a = 0;
  pwm_config.cmpr_b = 0;
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

  // Initial State: Brake (Both High)
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 100.0f);
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
                      MCPWM_DUTY_MODE_0);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 100.0f);
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B,
                      MCPWM_DUTY_MODE_0);

  // --- 2. ADC Continuous Setup (Modern) ---
  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = 8192,
      .conv_frame_size = ADC_READ_LEN,
  };
  ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &_adcHandle));

  adc_digi_pattern_config_t adc_pattern[1] = {0};
  adc_pattern[0].atten = ADC_ATTEN_DB_0;
  adc_pattern[0].channel = (uint8_t)ADC_CHAN;
  adc_pattern[0].unit = ADC_UNIT_1;
  adc_pattern[0].bit_width = ADC_BITWIDTH_12;

  adc_continuous_config_t dig_cfg = {0};
  dig_cfg.sample_freq_hz = 20000;
  dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2; // S3
  dig_cfg.pattern_num = 1;
  dig_cfg.adc_pattern = adc_pattern;

  ESP_ERROR_CHECK(adc_continuous_config(_adcHandle, &dig_cfg));
  ESP_ERROR_CHECK(adc_continuous_start(_adcHandle));
  Log.println("MotorHal: ADC Continuous started (Aligned/InputGain)");
}

void MotorHal::setDuty(float duty) {
  if (duty > 1.0f)
    duty = 1.0f;
  if (duty < -1.0f)
    duty = -1.0f;

  float dutyPercent = fabs(duty) * 100.0f;

  if (duty > 0) {
    // FWD: IN1 High, IN2 PWM Active Low
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 100.0f);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
                        MCPWM_DUTY_MODE_0);

    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, dutyPercent);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B,
                        MCPWM_DUTY_MODE_1);
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
  } else if (duty < 0) {
    // REV: IN1 PWM Active Low, IN2 High
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 100.0f);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B,
                        MCPWM_DUTY_MODE_0);

    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, dutyPercent);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
                        MCPWM_DUTY_MODE_1);
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
  } else {
    // BRAKE: Both High
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 100.0f);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
                        MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 100.0f);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B,
                        MCPWM_DUTY_MODE_0);
  }
}

size_t MotorHal::getAdcSamples(float *buffer, size_t maxLen) {
  if (!_adcHandle)
    return 0;

  uint32_t ret_num = 0;
  size_t totalSamples = 0;

  while (totalSamples < maxLen) {
    esp_err_t ret =
        adc_continuous_read(_adcHandle, result_data, ADC_READ_LEN, &ret_num, 0);

    if (ret == ESP_OK) {
      if (ret_num == 0)
        break;

      uint32_t count = ret_num / SOC_ADC_DIGI_RESULT_BYTES;
      uint32_t *raw_results = (uint32_t *)result_data;

      for (uint32_t i = 0; i < count && totalSamples < maxLen; i++) {
        // S3 Type 2 Data is in bits 0-11
        uint32_t data = raw_results[i] & 0xFFF;
        buffer[totalSamples++] = (float)data;
      }

      if (ret_num < ADC_READ_LEN)
        break;
    } else if (ret == ESP_ERR_TIMEOUT) {
      break;
    } else {
      static unsigned long lastAdcErr = 0;
      if (millis() - lastAdcErr > 1000) {
        Log.printf("MotorHal: ADC DMA Error 0x%x\n", ret);
        lastAdcErr = millis();
      }
      break;
    }
  }

  return totalSamples;
}

float MotorHal::getAdcSampleRate() const { return _sampleRate; }
