#ifndef MOTOR_HAL_H
#define MOTOR_HAL_H

#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
#include <stddef.h>
#include <stdint.h>

// Forward declaration for friend
class MotorHal;
extern "C" bool motor_hal_mcpwm_cb(mcpwm_timer_handle_t timer,
                                   const mcpwm_timer_event_data_t *edata,
                                   void *user_ctx);

class MotorHal {
public:
  static MotorHal &getInstance();

  void init();
  void setDuty(float duty);
  void setHardwareGain(uint8_t mode);
  bool readFault();
  float getCurrentScalar() const;

  // Sensing (Synchronized)
  float getLatestCurrentAdc() const;

  // For compatibility with existing MotorTask logic
  size_t getAdcSamples(float *buffer, size_t maxLen);
  float getAdcSampleRate() const;

private:
  MotorHal();

  // New MCPWM V5 Handles
  mcpwm_timer_handle_t _timer;
  mcpwm_oper_handle_t _oper;
  mcpwm_gen_handle_t _genA;
  mcpwm_gen_handle_t _genB;
  mcpwm_cmpr_handle_t _cmprA;
  mcpwm_cmpr_handle_t _cmprB;

  uint8_t _lastGain;
  float _currentDuty;

  // ISR Communication
  StreamBufferHandle_t _adcStreamBuffer;
  volatile float _lastCurrentAdc;

  // Allow ISR to access private members
  friend bool motor_hal_mcpwm_cb(mcpwm_timer_handle_t timer,
                                 const mcpwm_timer_event_data_t *edata,
                                 void *user_ctx);
};

#endif
