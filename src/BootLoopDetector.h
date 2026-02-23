#ifndef BOOT_LOOP_DETECTOR_H
#define BOOT_LOOP_DETECTOR_H

#include <Arduino.h>

struct RollbackInfo {
  bool rolledback;
  String runningVersion;
  String crashedVersion;
};

class BootLoopDetector {
public:
  static void check();
  static void markSuccessful();
  static bool didRollback();
  static RollbackInfo getRollbackInfo();
  static void clearRollback();

private:
  static const int CRASH_THRESHOLD = 3;
  static void rollback();
};

#endif
