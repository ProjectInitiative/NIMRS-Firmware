#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SystemState {
  uint16_t dccAddress = 3;
  uint8_t speed = 0;
  bool direction = true; // true = forward
  bool functions[29] = {false};

  // Status flags
  bool wifiConnected = false;
  uint32_t lastDccPacketTime = 0;
};

class SystemContext {
public:
  static SystemContext &getInstance() {
    static SystemContext instance;
    return instance;
  }

  SystemState &getState() { return _state; }

  void lock() { xSemaphoreTake(_mutex, portMAX_DELAY); }
  void unlock() { xSemaphoreGive(_mutex); }

private:
  SystemContext() { _mutex = xSemaphoreCreateMutex(); }
  SystemState _state;
  SemaphoreHandle_t _mutex;
};

// Helper macro for scoped lock
class ScopedLock {
public:
  ScopedLock(SystemContext &context) : _ctx(context) { _ctx.lock(); }
  ~ScopedLock() { _ctx.unlock(); }

private:
  SystemContext &_ctx;
};

#endif