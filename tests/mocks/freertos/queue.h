#ifndef QUEUE_MOCK_H
#define QUEUE_MOCK_H

#include "FreeRTOS.h"

typedef void *QueueHandle_t;

inline QueueHandle_t xQueueCreate(uint32_t count, uint32_t size) {
  return (QueueHandle_t)1;
}

inline int xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue,
                      TickType_t xTicksToWait) {
  return pdTRUE;
}

inline int xQueueReceive(QueueHandle_t xQueue, void *pvBuffer,
                         TickType_t xTicksToWait) {
  return pdFALSE;
}

inline int uxQueueMessagesWaiting(QueueHandle_t xQueue) { return 0; }

#endif
