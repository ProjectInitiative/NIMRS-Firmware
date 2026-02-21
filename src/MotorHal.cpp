#include "MotorHal.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <driver/mcpwm_prelude.h>
#include <esp_adc/adc_continuous.h>
#include <soc/adc_channel.h>
#include <cmath>

// ADC Configuration for ESP32-S3
#define ADC_READ_LEN 256
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2 // S3 uses Type 2

static adc_continuous_handle_t adc_handle = NULL;
static mcpwm_timer_handle_t timer = NULL;
static mcpwm_oper_handle_t oper = NULL;
static mcpwm_cmpr_handle_t comparator = NULL;
static mcpwm_gen_handle_t generator1 = NULL;
static mcpwm_gen_handle_t generator2 = NULL;

MotorHal::MotorHal() : _sampleRate(20000.0f) {}

MotorHal& MotorHal::getInstance() {
    static MotorHal instance;
    return instance;
}

void MotorHal::init() {
    // --- 1. ADC Continuous Setup ---
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 4096, // Increased buffer to avoid overflow
        .conv_frame_size = ADC_READ_LEN,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20000,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[1] = {0};
    adc_pattern[0].atten = ADC_ATTEN_DB_11;
    adc_pattern[0].channel = ADC_CHANNEL_4; // GPIO 5 is ADC1_CH4
    adc_pattern[0].unit = ADC_UNIT_1;
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.pattern_num = 1;
    dig_cfg.adc_pattern = adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    // --- 2. MCPWM Setup ---
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, // 10MHz ticks
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = 500, // 10MHz / 500 = 20kHz
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_operator_config_t operator_config = {
        .group_id = 0,
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    mcpwm_comparator_config_t comparator_config = {
        .flags = {
            .update_cmp_on_tez = true,
        },
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = Pinout::MOTOR_IN1,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator1));

    generator_config.gen_gpio_num = Pinout::MOTOR_IN2;
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator2));

    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
}

void MotorHal::setDuty(float duty) {
    // Clamp duty
    if (duty > 1.0f) duty = 1.0f;
    if (duty < -1.0f) duty = -1.0f;

    // Period is 500 ticks
    uint32_t period = 500;
    uint32_t compareVal = (uint32_t)(fabs(duty) * period);

    // Set comparator
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, compareVal));

    if (duty > 0) {
        // FWD: IN1=High (Brake/Drive), IN2=PWM
        // We want Active Low PWM for Slow Decay drive?
        // Logic:
        // Drive: IN1=1, IN2=0
        // Brake: IN1=1, IN2=1 (Slow Decay)
        // Coast: IN1=0, IN2=0 (Fast Decay)

        // If we want slow decay (Brake/Drive switching):
        // PWM ON (Drive): IN1=1, IN2=0.
        // PWM OFF (Brake): IN1=1, IN2=1.

        // Implementation:
        // IN1: Force High.
        // IN2:
        // If duty 0% -> Drive 0% -> Brake 100% -> IN2=1 always?
        // If duty 100% -> Drive 100% -> Brake 0% -> IN2=0 always?

        // Let's use Active Low PWM on IN2.
        // 0 to CMP: LOW (Drive)
        // CMP to Period: HIGH (Brake)
        // Duty cycle = compareVal / period.
        // So compareVal is "Drive Time".

        mcpwm_generator_set_force_level(generator1, 1, true); // IN1 High

        mcpwm_generator_set_force_level(generator2, -1, true); // Release force
        // Action:
        // Timer Zero: Go LOW (Start Drive)
        mcpwm_generator_set_action_on_timer_event(generator2,
                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW));
        // Timer Compare: Go HIGH (Start Brake)
        mcpwm_generator_set_action_on_compare_event(generator2,
                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_HIGH));

    } else if (duty < 0) {
        // REV: IN1=PWM, IN2=High
        // Active Low PWM on IN1.

        mcpwm_generator_set_force_level(generator2, 1, true); // IN2 High

        mcpwm_generator_set_force_level(generator1, -1, true); // Release force

        mcpwm_generator_set_action_on_timer_event(generator1,
                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_LOW));
        mcpwm_generator_set_action_on_compare_event(generator1,
                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_HIGH));

    } else {
        // Stop/Brake: Both High
        mcpwm_generator_set_force_level(generator1, 1, true);
        mcpwm_generator_set_force_level(generator2, 1, true);
    }
}

size_t MotorHal::getAdcSamples(float* buffer, size_t maxLen) {
    if (!adc_handle) return 0;

    uint8_t rawBuf[ADC_READ_LEN];
    uint32_t ret_num = 0;
    size_t totalSamples = 0;

    // Drain the buffer
    while (totalSamples < maxLen) {
        esp_err_t ret = adc_continuous_read(adc_handle, rawBuf, ADC_READ_LEN, &ret_num, 0);

        if (ret == ESP_OK || ret == ESP_ERR_TIMEOUT) {
            if (ret_num == 0) break;

            size_t count = ret_num / sizeof(adc_digi_output_data_t);
            for (size_t i = 0; i < count && totalSamples < maxLen; i++) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t*)&rawBuf[i * sizeof(adc_digi_output_data_t)];

                if (p->type2.channel == ADC_CHANNEL_4) {
                    uint32_t val = p->type2.data;
                    buffer[totalSamples++] = (float)val;
                }
            }
            if (ret_num < ADC_READ_LEN) break; // No more full chunks
        } else {
            break;
        }
    }
    return totalSamples;
}

float MotorHal::getAdcSampleRate() const {
    return _sampleRate;
}
