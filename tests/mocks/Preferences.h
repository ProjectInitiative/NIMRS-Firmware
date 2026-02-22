#ifndef PREFERENCES_MOCK_H
#define PREFERENCES_MOCK_H

#include "Arduino.h"
#include <map>
#include <string>

class Preferences {
public:
  static std::map<std::string, String> storage;

  void begin(const char *name, bool readOnly) {}
  void end() {}

  String getString(const char *key, const char *def) {
    if (storage.count(key))
      return storage[key];
    return String(def);
  }

  void putString(const char *key, const String &val) { storage[key] = val; }
};

#endif
