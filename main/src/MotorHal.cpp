#include "MotorHal.h"
#include "Logger.h"
#include "driver/adc.h"
#include "nimrs-pinout.h"
#include <Arduino.h>
#include <cmath>

MotorHal::MotorHal()
    : _timer(NULL), _oper(NULL), _genA(NULL), _genB(NULL), _cmprA(NULL),
      _cmprB(NULL), _lastGain(255), _currentDuty(0.0f), _adcStreamBuffer(NULL),
      _lastCurrentAdc(0.0f) {}

MotorHal &MotorHal::getInstance() {
  static MotorHal instance;
  return instance;
}

// ISR Callback: Pushes high-frequency samples to stream buffer
extern "C" bool IRAM_ATTR
motor_hal_mcpwm_cb(mcpwm_timer_handle_t timer,
                   const mcpwm_timer_event_data_t *edata, void *user_ctx) {
  MotorHal *self = (MotorHal *)user_ctx;

  if (edata->count_value == 0) { // Center of ON
    int raw = adc1_get_raw(ADC1_CHANNEL_5);
    float val = (float)raw;
    self->_lastCurrentAdc = val;

    if (self->_adcStreamBuffer) {
      BaseType_t high_task_wakeup = pdFALSE;
      xStreamBufferSendFromISR(self->_adcStreamBuffer, &val, sizeof(float),
                               &high_task_wakeup);
      return high_task_wakeup == pdTRUE;
    }
  }
  return false;
}

void MotorHal::init() {
  // 1. Setup Stream Buffer (4096 bytes / sizeof(float) = 1024 samples capacity)
  _adcStreamBuffer = xStreamBufferCreate(4096, sizeof(float));

  // 2. Configure ADC1
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_5, ADC_ATTEN_DB_12);

  // 3. Initialize MCPWM Timer (Center Aligned)
  mcpwm_timer_config_t timer_config = {};
  timer_config.group_id = 0;
  timer_config.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
  timer_config.resolution_hz = 1000000;
  timer_config.count_mode = MCPWM_TIMER_COUNT_MODE_UP_DOWN;
  timer_config.period_ticks = 25; // 20kHz PWM (1MHz / (25 * 2))
  ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &_timer));

  // 4. Initialize Operator
  mcpwm_operator_config_t oper_config = {};
  oper_config.group_id = 0;
  ESP_ERROR_CHECK(mcpwm_new_operator(&oper_config, &_oper));
  ESP_ERROR_CHECK(mcpwm_operator_connect_timer(_oper, _timer));

  // 5. Initialize Comparators
  mcpwm_comparator_config_t cmpr_config = {};
  cmpr_config.flags.update_cmp_on_tez = true;
  ESP_ERROR_CHECK(mcpwm_new_comparator(_oper, &cmpr_config, &_cmprA));
  ESP_ERROR_CHECK(mcpwm_new_comparator(_oper, &cmpr_config, &_cmprB));

  // 6. Initialize Generators
  mcpwm_generator_config_t gen_config = {};
  gen_config.gen_gpio_num = Pinout::MOTOR_IN1;
  ESP_ERROR_CHECK(mcpwm_new_generator(_oper, &gen_config, &_genA));
  gen_config.gen_gpio_num = Pinout::MOTOR_IN2;
  ESP_ERROR_CHECK(mcpwm_new_generator(_oper, &gen_config, &_genB));

  // 7. Register Callback (TEZ only for Current)
  mcpwm_timer_event_callbacks_t cbs = {};
  cbs.on_empty = motor_hal_mcpwm_cb;
  ESP_ERROR_CHECK(mcpwm_timer_register_event_callbacks(_timer, &cbs, this));

  ESP_ERROR_CHECK(mcpwm_timer_enable(_timer));
  ESP_ERROR_CHECK(mcpwm_timer_start_stop(_timer, MCPWM_TIMER_START_NO_STOP));

  pinMode(Pinout::MOTOR_FAULT, INPUT_PULLUP);
  setHardwareGain(1);
  setDuty(0.0f);

  Log.println("MotorHal: V5 MCPWM Ready (20kHz Synchronized Sampling)");
}

void MotorHal::setDuty(float duty) {
  _currentDuty = duty;
  float dutyPercent = std::min(1.0f, std::max(-1.0f, duty));
  // 25 ticks is max period
  uint32_t compare_val = (uint32_t)(fabs(dutyPercent) * 25.0f);

  if (fabs(dutyPercent) < 0.01f) {
    // BRAKE
    mcpwm_generator_set_action_on_timer_event(
        _genA, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_timer_event(
        _genA, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_timer_event(
        _genB, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_timer_event(
        _genB, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_HIGH));
  } else if (dutyPercent > 0) {
    // FWD: Drive/Coast
    mcpwm_comparator_set_compare_value(_cmprA, compare_val);
    mcpwm_generator_set_action_on_timer_event(
        _genB, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_LOW));
    mcpwm_generator_set_action_on_timer_event(
        _genB, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_LOW));

    mcpwm_generator_set_action_on_timer_event(
        _genA, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(
        _genA, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, _cmprA,
                                              MCPWM_GEN_ACTION_LOW));
    mcpwm_generator_set_action_on_compare_event(
        _genA, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                              _cmprA, MCPWM_GEN_ACTION_HIGH));
  } else {
    // REV: Drive/Coast
    mcpwm_comparator_set_compare_value(_cmprB, compare_val);
    mcpwm_generator_set_action_on_timer_event(
        _genA, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_LOW));
    mcpwm_generator_set_action_on_timer_event(
        _genA, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_LOW));

    mcpwm_generator_set_action_on_timer_event(
        _genB, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            MCPWM_TIMER_EVENT_EMPTY,
                                            MCPWM_GEN_ACTION_HIGH));
    mcpwm_generator_set_action_on_compare_event(
        _genB, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, _cmprB,
                                              MCPWM_GEN_ACTION_LOW));
    mcpwm_generator_set_action_on_compare_event(
        _genB, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_DOWN,
                                              _cmprB, MCPWM_GEN_ACTION_HIGH));
  }
}

float MotorHal::getLatestCurrentAdc() const { return _lastCurrentAdc; }

void MotorHal::setHardwareGain(uint8_t mode) {
  if (_lastGain == mode)
    return;
  _lastGain = mode;
  switch (mode) {
  case 0:
    pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
    digitalWrite(Pinout::MOTOR_GAIN_SEL, LOW);
    break;
  case 1:
    pinMode(Pinout::MOTOR_GAIN_SEL, INPUT);
    gpio_set_pull_mode((gpio_num_t)Pinout::MOTOR_GAIN_SEL, GPIO_FLOATING);
    break;
  case 2:
    pinMode(Pinout::MOTOR_GAIN_SEL, OUTPUT);
    digitalWrite(Pinout::MOTOR_GAIN_SEL, HIGH);
    break;
  }
}

bool MotorHal::readFault() { return digitalRead(Pinout::MOTOR_FAULT) == LOW; }

float MotorHal::getCurrentScalar() const {
  const float V_PER_STEP = 3.3f / 4095.0f;
  switch (_lastGain) {
  case 0:
    return V_PER_STEP / 0.492f;
  case 1:
    return V_PER_STEP / 2.520f;
  case 2:
    return V_PER_STEP / 11.760f;
  default:
    return V_PER_STEP / 2.520f;
  }
}

size_t MotorHal::getAdcSamples(float *buffer, size_t maxLen) {
  if (!_adcStreamBuffer)
    return 0;
  // Receive all available samples up to maxLen
  size_t bytes =
      xStreamBufferReceive(_adcStreamBuffer, buffer, maxLen * sizeof(float), 0);
  return bytes / sizeof(float);
}

float MotorHal::getAdcSampleRate() const { return 20000.0f; }
