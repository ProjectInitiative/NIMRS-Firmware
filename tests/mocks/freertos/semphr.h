#pragma once
#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (void*)1; }
inline void xSemaphoreTake(SemaphoreHandle_t xSemaphore, uint32_t xBlockTime) {}
inline void xSemaphoreGive(SemaphoreHandle_t xSemaphore) {}
