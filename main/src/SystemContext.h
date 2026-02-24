#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum ControlSource { SOURCE_DCC, SOURCE_WEB };

struct SystemState {
  uint16_t dccAddress = 3;
  uint8_t speed = 0;
  bool direction = true; // true = forward
  bool functions[29] = {false};

  // Control Logic
  ControlSource speedSource = SOURCE_DCC;
  uint8_t lastDccSpeed = 0;
  bool lastDccDirection = true;

  // Status flags
  bool wifiConnected = false;
  uint32_t lastDccPacketTime = 0;

  // Motor Load Factor (0.0 = No Load, 1.0 = Heavy Load)
  // Used for Audio Chuff modification
  float loadFactor = 0.0f;
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
