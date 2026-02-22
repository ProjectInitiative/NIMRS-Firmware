#ifndef TASK_MOCK_H
#define TASK_MOCK_H

#include "FreeRTOS.h"

// Basic FreeRTOS Task Functions
inline void vTaskDelay(TickType_t xTicksToDelay) {}
inline void vTaskDelayUntil(TickType_t *pxPreviousWakeTime,
                            TickType_t xTimeIncrement) {}
inline TickType_t xTaskGetTickCount(void) { return 0; }

inline void xTaskCreatePinnedToCore(void (*task)(void *), const char *name,
                                    uint32_t stack, void *param, uint32_t prio,
                                    TaskHandle_t *handle, int core) {}

#endif
