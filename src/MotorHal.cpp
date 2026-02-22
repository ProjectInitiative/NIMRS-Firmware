#include "MotorHal.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <cmath>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <driver/mcpwm.h>
#include <esp_err.h>
#include <esp_log.h>
#include <string.h>

#ifndef SOC_ADC_DIGI_MAX_BITWIDTH
#define SOC_ADC_DIGI_MAX_BITWIDTH 12
#endif

// ADC Configuration (Legacy IDF 4.4 / Arduino 2.x)
#define ADC_READ_LEN 1024
#define ADC_UNIT 0 // 0 = ADC1, 1 = ADC2 (DMA unit index is 0-based)
#define ADC_CHAN 4 // ADC_CHANNEL_4 is GPIO 5 on S3 ADC1

static uint32_t result_data_aligned[ADC_READ_LEN / 4]; // Static buffer aligned
                                                       // for 32-bit access
static uint8_t *result_data = (uint8_t *)result_data_aligned;

MotorHal::MotorHal() : _sampleRate(20000.0f) {}

MotorHal &MotorHal::getInstance() {
  static MotorHal instance;
  return instance;
}

void MotorHal::init() {
  // --- 1. MCPWM Setup (Legacy) ---
  // Initialize GPIOs
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, Pinout::MOTOR_IN1);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, Pinout::MOTOR_IN2);

  // Initialize Gain Select and Current Sense (High Impedance / Input)
  gpio_reset_pin((gpio_num_t)Pinout::MOTOR_GAIN_SEL);
  pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);

  gpio_reset_pin((gpio_num_t)Pinout::MOTOR_CURRENT);
  pinMode(Pinout::MOTOR_CURRENT, INPUT);

  // Configure Timer & Frequency
  mcpwm_config_t pwm_config;
  pwm_config.frequency = 20000; // frequency = 20kHz,
  pwm_config.cmpr_a = 0;        // duty cycle of PWMxA = 0
  pwm_config.cmpr_b = 0;        // duty cycle of PWMxB = 0
  pwm_config.counter_mode = MCPWM_UP_COUNTER;
  pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

  mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

  // Initial State: Brake (Both High for Slow Decay in legacy or Both Low for
  // Fast Decay) Legacy: mcpwm_set_signal_high/low
  mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
  mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);

  // --- 2. ADC Continuous Setup (Legacy) ---
  adc_digi_init_config_t adc_dma_config = {0};
  adc_dma_config.max_store_buf_size = 4096;
  adc_dma_config.conv_num_each_intr = 256;
  adc_dma_config.adc1_chan_mask = BIT(ADC_CHAN);
  adc_dma_config.adc2_chan_mask = 0; // Explicitly 0

  adc_digi_pattern_config_t adc_pattern[1] = {0};
  adc_pattern[0].atten = ADC_ATTEN_DB_0;
  adc_pattern[0].channel = (uint8_t)ADC_CHAN;
  adc_pattern[0].unit = ADC_UNIT_1; // Explicitly ADC1 (S3 DMA requirement)
  adc_pattern[0].bit_width = (uint8_t)SOC_ADC_DIGI_MAX_BITWIDTH;

  adc_digi_configuration_t dig_cfg = {0};
  dig_cfg.conv_limit_en = 0;
  dig_cfg.conv_limit_num = 250;
  dig_cfg.pattern_num = 1;
  dig_cfg.adc_pattern = adc_pattern;
  dig_cfg.sample_freq_hz = 20000;
  dig_cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  dig_cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2; // S3

  ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));
  ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
  ESP_ERROR_CHECK(adc_digi_start());
}

void MotorHal::setDuty(float duty) {
  // Clamp duty
  if (duty > 1.0f)
    duty = 1.0f;
  if (duty < -1.0f)
    duty = -1.0f;

  float dutyPercent = fabs(duty) * 100.0f;

  if (duty > 0) {
    // FWD: IN1=High (Brake/Drive), IN2=PWM (Active Low)
    // IN1 High
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);

    // IN2 Active Low PWM
    // Duty Mode 1: Active High?
    // Let's check:
    // DUTY_MODE_0: Active high (High for duty%, Low for rest)
    // DUTY_MODE_1: Active low (Low for duty%, High for rest)

    // We want Low for `duty` duration (Drive), High for rest (Brake).
    // So Low for duty%. Active Low. -> DUTY_MODE_1?
    // Wait, legacy docs say:
    // MCPWM_DUTY_MODE_0: Active high duty, i.e. duty cycle proportional to high
    // time. MCPWM_DUTY_MODE_1: Active low duty, i.e. duty cycle proportional to
    // low time.

    // If we want Drive (Low) proportional to `duty`:
    // Set DUTY_MODE_1.
    // Set duty `dutyPercent`.
    // Result: Low for `dutyPercent`, High for `100-dutyPercent`.

    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B,
                        MCPWM_DUTY_MODE_1);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, dutyPercent);
    mcpwm_start(MCPWM_UNIT_0,
                MCPWM_TIMER_0); // Ensure running? (Should be running)

  } else if (duty < 0) {
    // REV: IN1=PWM (Active Low), IN2=High
    // IN2 High
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);

    // IN1 Active Low PWM
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A,
                        MCPWM_DUTY_MODE_1);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, dutyPercent);

  } else {
    // Brake: Both High
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
  }
}

size_t MotorHal::getAdcSamples(float *buffer, size_t maxLen) {
  uint32_t ret_num = 0;
  size_t totalSamples = 0;

  // Drain buffer loop
  while (totalSamples < maxLen) {
    esp_err_t ret = adc_digi_read_bytes(result_data, ADC_READ_LEN, &ret_num,
                                        0); // timeout 0

    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
      if (ret_num == 0)
        break;

      // Parse S3 Type 2 Data (4 bytes per sample)
      // Note: In IDF 4.4, adc_digi_output_data_t might be 2 bytes by default
      // but S3 uses 4. We manually cast to uint32_t to be safe if struct
      // definition varies.

      // S3 Type 2 format (32-bit):
      // [11:0] data, [15:12] channel, [16] unit, [31:17] reserved?
      // Check TRM:
      // 31:18 reserved
      // 17: unit
      // 16:13 channel
      // 12:0 data (13 bits? usually 12)

      // Actually, let's rely on standard parsing but be careful about size.
      // If result_data contains 4-byte samples, we iterate by 4 bytes.

      size_t count = ret_num / 4;
      uint32_t *dmaBuf = (uint32_t *)result_data;

      for (size_t i = 0; i < count && totalSamples < maxLen; i++) {
        uint32_t raw = dmaBuf[i];

        // Decode S3 DMA format (Type 2)
        uint32_t data = raw & 0xFFF; // 12 bits

        // Since we only configured one channel pattern, assume all data is for
        // that channel. This avoids issues with potential channel bit mismatch.
        buffer[totalSamples++] = (float)data;
      }
      if (ret_num < ADC_READ_LEN)
        break;
    } else {
      break;
    }
  }
  return totalSamples;
}

float MotorHal::getAdcSampleRate() const { return _sampleRate; }
