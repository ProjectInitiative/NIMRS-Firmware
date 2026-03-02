#pragma once

typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

inline TimerHandle_t xTimerCreate(const char *const pcTimerName,
                                  const uint32_t xTimerPeriodInTicks,
                                  const uint32_t uxAutoReload,
                                  void *const pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction) {
  return nullptr;
}
inline bool xTimerStart(TimerHandle_t xTimer, uint32_t xTicksToWait) {
  return true;
}
inline bool xTimerStop(TimerHandle_t xTimer, uint32_t xTicksToWait) {
  return true;
}
inline bool xTimerReset(TimerHandle_t xTimer, uint32_t xTicksToWait) {
  return true;
}
inline bool xTimerChangePeriod(TimerHandle_t xTimer, uint32_t xNewPeriod,
                               uint32_t xTicksToWait) {
  return true;
}
