#ifndef FREERTOS_MOCK_H
#define FREERTOS_MOCK_H

#include <stdint.h>

typedef void *TaskHandle_t;
typedef uint32_t TickType_t;
#define pdMS_TO_TICKS(ms) (ms)
#define portMAX_DELAY 0xFFFFFFFF

#endif
