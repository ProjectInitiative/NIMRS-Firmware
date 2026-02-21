#include "MotorHal.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <driver/mcpwm.h>
#include <driver/adc.h>
#include <soc/mcpwm_periph.h>
#include <cmath>

// ADC Configuration (Legacy IDF 4.4 / Arduino 2.x)
#define ADC_READ_LEN 1024
#define ADC_UNIT ADC_UNIT_1
#define ADC_CHAN ADC_CHANNEL_4 // GPIO 5 (Check pinout for S3)

static uint8_t result_data[ADC_READ_LEN]; // Static buffer for DMA

MotorHal::MotorHal() : _sampleRate(20000.0f) {}

MotorHal& MotorHal::getInstance() {
    static MotorHal instance;
    return instance;
}

void MotorHal::init() {
    // --- 1. MCPWM Setup (Legacy) ---
    // Initialize GPIOs
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, Pinout::MOTOR_IN1);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, Pinout::MOTOR_IN2);

    // Configure Timer & Frequency
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 20000;    // frequency = 20kHz,
    pwm_config.cmpr_a = 0;           // duty cycle of PWMxA = 0
    pwm_config.cmpr_b = 0;           // duty cycle of PWMxB = 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);

    // Initial State: Brake (Both High for Slow Decay in legacy or Both Low for Fast Decay)
    // Legacy: mcpwm_set_signal_high/low
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
    mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);


    // --- 2. ADC Continuous Setup (Legacy) ---
    adc_digi_init_config_t adc_dma_config = {
        .max_store_buf_size = 4096,
        .conv_num_each_intr = ADC_READ_LEN,
        .adc1_chan_mask = BIT(ADC_CHAN),
        .adc2_chan_mask = 0,
    };

    adc_digi_configuration_t dig_cfg = {
        .conv_limit_en = 0,
        .conv_limit_num = 250,
        .pattern_num = 1,
        .adc_pattern = NULL, // Set below
        .sample_freq_hz = 20000,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2, // S3
    };

    adc_digi_pattern_config_t adc_pattern[1] = {0};
    adc_pattern[0].atten = ADC_ATTEN_DB_11;
    adc_pattern[0].channel = ADC_CHAN;
    adc_pattern[0].unit = ADC_UNIT;
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.adc_pattern = adc_pattern;

    ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));
    ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
    ESP_ERROR_CHECK(adc_digi_start());
}

void MotorHal::setDuty(float duty) {
    // Clamp duty
    if (duty > 1.0f) duty = 1.0f;
    if (duty < -1.0f) duty = -1.0f;

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
        // MCPWM_DUTY_MODE_0: Active high duty, i.e. duty cycle proportional to high time.
        // MCPWM_DUTY_MODE_1: Active low duty, i.e. duty cycle proportional to low time.

        // If we want Drive (Low) proportional to `duty`:
        // Set DUTY_MODE_1.
        // Set duty `dutyPercent`.
        // Result: Low for `dutyPercent`, High for `100-dutyPercent`.

        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_1);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, dutyPercent);
        mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0); // Ensure running? (Should be running)

    } else if (duty < 0) {
        // REV: IN1=PWM (Active Low), IN2=High
        // IN2 High
        mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);

        // IN1 Active Low PWM
        mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_1);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, dutyPercent);

    } else {
        // Brake: Both High
        mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A);
        mcpwm_set_signal_high(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B);
    }
}

size_t MotorHal::getAdcSamples(float* buffer, size_t maxLen) {
    uint32_t ret_num = 0;
    size_t totalSamples = 0;

    // Drain buffer loop
    while (totalSamples < maxLen) {
        esp_err_t ret = adc_digi_read_bytes(result_data, ADC_READ_LEN, &ret_num, 0); // timeout 0

        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            if (ret_num == 0) break;

            // Parse S3 Type 2 Data
            size_t count = ret_num / sizeof(adc_digi_output_data_t);
            for (size_t i = 0; i < count && totalSamples < maxLen; i++) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result_data[i * sizeof(adc_digi_output_data_t)];

                // Check if Type 2 valid
                if (p->type2.unit == ADC_UNIT-1 && p->type2.channel == ADC_CHAN) {
                     uint32_t val = p->type2.data;
                     buffer[totalSamples++] = (float)val;
                }
            }
            if (ret_num < ADC_READ_LEN) break;
        } else {
            break;
        }
    }
    return totalSamples;
}

float MotorHal::getAdcSampleRate() const {
    return _sampleRate;
}
