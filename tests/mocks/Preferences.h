#ifndef PREFERENCES_MOCK_H
#define PREFERENCES_MOCK_H

#include "Arduino.h"
#include <map>
#include <string>

class Preferences {
public:
  static std::map<std::string, std::string> _storage;

  void begin(const char *name, bool readOnly) {}
  void end() {}

  String getString(const char *key, const char *def) {
    if (_storage.count(key)) {
      return String(_storage[key].c_str());
    }
    return String(def);
  }

  void putString(const char *key, const String &val) {
    _storage[key] = std::string(val.c_str());
  }

  int getInt(const char *key, int def) {
    if (_storage.count(key)) {
      return std::stoi(_storage[key]);
    }
    return def;
  }

  void putInt(const char *key, int val) { _storage[key] = std::to_string(val); }

  void clear() { _storage.clear(); }

  static void reset() { _storage.clear(); }
};

#endif
