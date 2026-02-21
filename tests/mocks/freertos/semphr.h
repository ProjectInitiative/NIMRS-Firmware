#ifndef SEMPHR_MOCK_H
#define SEMPHR_MOCK_H

#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
inline int xSemaphoreTake(SemaphoreHandle_t x, TickType_t t) { return 1; }
inline int xSemaphoreGive(SemaphoreHandle_t x) { return 1; }

#endif
