#ifndef BOOT_LOOP_DETECTOR_H
#define BOOT_LOOP_DETECTOR_H

#include <Arduino.h>

class BootLoopDetector {
public:
  static void check();
  static void markSuccessful();
  static bool didRollback();

private:
  static const int CRASH_THRESHOLD = 3;
  static void rollback();
};

#endif
