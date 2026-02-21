#ifndef ARDUINOJSON_MOCK_H
#define ARDUINOJSON_MOCK_H

#include "Arduino.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

struct JsonVariant {
  String val;
  bool isString;
  JsonVariant() : val(""), isString(true) {}
  JsonVariant(const char *v) : val(v), isString(true) {}
  JsonVariant(String v) : val(v), isString(true) {}
  JsonVariant(int v) : val(std::to_string(v)), isString(false) {}
  JsonVariant(unsigned int v) : val(std::to_string(v)), isString(false) {}
  JsonVariant(long unsigned int v) : val(std::to_string(v)), isString(false) {}
  JsonVariant(bool v) : val(v ? "true" : "false"), isString(false) {}

  template <typename T> T as() const { return (T) * this; }

  template <typename T> void operator=(T v) {
    JsonVariant temp(v);
    val = temp.val;
    isString = temp.isString;
  }

  template <typename T> T to() { return T(); }

  operator String() const { return val; }
  operator int() const { return val.toInt(); }
  operator uint8_t() const { return (uint8_t)val.toInt(); }
  operator bool() const { return val == "true" || val == "1"; }
};

class JsonDocument;

class JsonArray {
  JsonDocument *_doc;

public:
  std::vector<JsonVariant> _localData;

  JsonArray(JsonDocument *doc = nullptr) : _doc(doc) {}

  template <typename T> T add() { return T(); }
  void add(JsonVariant v);
  template <typename T> T to() { return T(); }
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

  typedef std::vector<Pair>::iterator iterator;
  iterator begin() { return _pairs.begin(); }
  iterator end() { return _pairs.end(); }

  template <typename T> T to() { return T(); }
  template <typename T> T add() { return T(); }
  template <typename T> T as() { return (T) * this; }
};

typedef JsonObject::Pair JsonPair;

class JsonDocument {
public:
  std::map<std::string, JsonVariant> _data;
  std::vector<JsonVariant> _arrData;
  bool _isArray = false;

  JsonVariant &operator[](String key) { return _data[(std::string)key]; }

  template <typename T> T to();

  template <typename T> T as() {
    JsonObject obj;
    for (auto const &[k, v] : _data) {
      obj._data[k] = v;
      obj._pairs.push_back({String(k.c_str()), v});
    }
    return obj;
  }
  void clear() {
    _data.clear();
    _arrData.clear();
    _isArray = false;
  }
};

inline void JsonArray::add(JsonVariant v) {
  if (_doc) {
    _doc->_arrData.push_back(v);
  } else {
    _localData.push_back(v);
  }
}

template <> inline JsonArray JsonDocument::to<JsonArray>() {
  _isArray = true;
  _data.clear();
  _arrData.clear();
  return JsonArray(this);
}

template <typename T> T JsonDocument::to() { return T(); }

inline void serializeJson(const JsonDocument &doc, String &out) {
  if (doc._isArray) {
    out = "[";
    for (size_t i = 0; i < doc._arrData.size(); ++i) {
      if (i > 0)
        out += ",";
      if (doc._arrData[i].isString)
        out += "\"" + doc._arrData[i].val + "\"";
      else
        out += doc._arrData[i].val;
    }
    out += "]";
  } else {
    out = "{\"mock\":true}";
  }
}

inline void serializeJson(const JsonDocument &doc, Print &out) {
  String s;
  serializeJson(doc, s);
  out.print(s);
}

inline void serializeJson(const JsonArray &arr, String &out) { out = "[]"; }
inline void serializeJson(const JsonObject &obj, String &out) { out = "{}"; }

struct DeserializationError {
  enum Code { Ok, InvalidInput };
  Code _code;
  DeserializationError(Code c) : _code(c) {}
  operator bool() const { return _code != Ok; }
  const char *c_str() const { return "error"; }
};

inline String trim(const String &s) {
  auto start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

inline DeserializationError deserializeJson(JsonDocument &doc,
                                            const String &in) {
  std::string s = (std::string)in;
  size_t pos = s.find('{');
  if (pos == std::string::npos)
    return DeserializationError::Ok;
  pos++;

  while (true) {
    pos = s.find('"', pos);
    if (pos == std::string::npos)
      break;
    size_t key_start = pos + 1;
    size_t key_end = s.find('"', key_start);
    if (key_end == std::string::npos)
      break;
    std::string key = s.substr(key_start, key_end - key_start);

    pos = s.find(':', key_end);
    if (pos == std::string::npos)
      break;
    pos++;

    pos = s.find_first_not_of(" \t\n\r", pos);
    if (pos == std::string::npos)
      break;

    std::string val;
    if (s[pos] == '"') {
      size_t val_start = pos + 1;
      size_t val_end = s.find('"', val_start);
      if (val_end == std::string::npos)
        break;
      val = s.substr(val_start, val_end - val_start);
      pos = val_end + 1;
    } else {
      size_t val_end = s.find_first_of(",}", pos);
      if (val_end == std::string::npos)
        val_end = s.length();
      val = trim(String(s.substr(pos, val_end - pos)));
      pos = val_end;
    }
    doc._data[key] = JsonVariant(val);

    pos = s.find_first_of(",}", pos);
    if (pos == std::string::npos || s[pos] == '}')
      break;
    pos++;
  }

  return DeserializationError::Ok;
}

#endif