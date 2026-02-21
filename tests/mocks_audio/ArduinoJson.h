#ifndef ARDUINOJSON_MOCK_H
#define ARDUINOJSON_MOCK_H

#include "Arduino.h"
#include "WebServer.h" // For File
#include <algorithm>
#include <map>
#include <string>
#include <vector>

// Forward declarations
class JsonArray;
class JsonObject;

struct JsonVariant {
  String val;
  JsonVariant() : val("") {}
  JsonVariant(const char *v) : val(v) {}
  JsonVariant(String v) : val(v) {}
  JsonVariant(int v) : val(std::to_string(v)) {}
  JsonVariant(unsigned int v) : val(std::to_string(v)) {}
  JsonVariant(long unsigned int v) : val(std::to_string(v)) {}
  JsonVariant(bool v) : val(v ? "true" : "false") {}

  template <typename T> T as() const;
  template <typename T> bool is() const { return true; }

  template <typename T> void operator=(T v) { val = JsonVariant(v).val; }

  operator String() const { return val; }
  operator int() const { return val.toInt(); }
  operator uint8_t() const { return (uint8_t)val.toInt(); }
  operator bool() const { return val == "true" || val == "1"; }

  // Forward declared conversions
  operator JsonObject() const;
  operator JsonArray() const;
};

class JsonArray {
public:
  std::vector<JsonVariant> _data;
  void add(JsonVariant v) { _data.push_back(v); }

  typedef std::vector<JsonVariant>::iterator iterator;
  iterator begin() { return _data.begin(); }
  iterator end() { return _data.end(); }
};

class JsonObject {
public:
  struct Pair {
    String _k;
    JsonVariant _v;
    String key() const { return _k; }
    JsonVariant value() const { return _v; }
  };

  std::map<std::string, JsonVariant> _data;
  std::vector<Pair> _pairs;

  JsonVariant &operator[](String key) { return _data[(std::string)key]; }
  JsonVariant &operator[](const char* key) { return _data[(std::string)key]; }

  typedef std::vector<Pair>::iterator iterator;
  iterator begin() { return _pairs.begin(); }
  iterator end() { return _pairs.end(); }

  template <typename T> T as() { return T(); } // Dummy
};

// Implementations
template<> inline JsonObject JsonVariant::as<JsonObject>() const { return JsonObject(); }
template<> inline JsonArray JsonVariant::as<JsonArray>() const { return JsonArray(); }
template<> inline String JsonVariant::as<String>() const { return val; }

inline JsonVariant::operator JsonObject() const { return JsonObject(); }
inline JsonVariant::operator JsonArray() const { return JsonArray(); }

class JsonDocument {
public:
  std::map<std::string, JsonVariant> _data;
  JsonVariant &operator[](String key) { return _data[(std::string)key]; }
  JsonVariant &operator[](const char* key) { return _data[(std::string)key]; } // Needed for const char*

  template <typename T> T as() { return T(); }
  void clear() { _data.clear(); }
};

struct DeserializationError {
  enum Code { Ok, InvalidInput };
  Code _code;
  DeserializationError(Code c) : _code(c) {}
  operator bool() const { return _code != Ok; }
  const char *c_str() const { return "error"; }
};

inline DeserializationError deserializeJson(JsonDocument &doc, const String &in) {
  return DeserializationError::Ok;
}

inline DeserializationError deserializeJson(JsonDocument &doc, File &file) {
  return DeserializationError::Ok;
}

#endif
