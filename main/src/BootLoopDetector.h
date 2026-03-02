#ifndef BOOT_LOOP_DETECTOR_H
#define BOOT_LOOP_DETECTOR_H

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

struct RollbackInfo {
  bool rolledback;
  String runningVersion;
  String crashedVersion;
};

class BootLoopDetector {
public:
  static void check();
  static void startStabilityTimer();
  static bool performHealthCheck();
  static void performFactoryReset(); // Wipe settings and restore defaults
  static void markSuccessful();
  static bool didRollback();
  static RollbackInfo getRollbackInfo();
  static void clearRollback();

private:
  static void rollback();
  static void timerCallback(TimerHandle_t xTimer);
};

#endif
