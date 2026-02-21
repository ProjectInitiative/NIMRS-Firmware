#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>

typedef enum {
  GPIO_NUM_0 = 0,
  GPIO_NUM_1 = 1,
  GPIO_NUM_2 = 2,
  GPIO_NUM_3 = 3,
  // Add others as needed
} gpio_num_t;

void gpio_reset_pin(gpio_num_t gpio_num);

// Other GPIO related types
typedef enum { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db } adc_attenuation_t;

#endif
