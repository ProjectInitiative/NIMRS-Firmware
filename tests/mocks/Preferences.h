#ifndef PREFERENCES_MOCK_H
#define PREFERENCES_MOCK_H

#include "Arduino.h"

class Preferences {
public:
  void begin(const char *name, bool readOnly) {}
  void end() {}
  String getString(const char *key, const char *def) { return def; }
  void putString(const char *key, const String &val) {}
};

#endif
