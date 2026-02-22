#ifndef ARDUINOJSON_MOCK_H
#define ARDUINOJSON_MOCK_H

#include "Arduino.h"
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
  JsonVariant(float v) : val(std::to_string(v)) {}
  JsonVariant(double v) : val(std::to_string(v)) {}
  JsonVariant(bool v) : val(v ? "true" : "false") {}

  // Basic conversions
  operator String() const { return val; }
  operator int() const { return val.toInt(); }
  operator uint8_t() const { return (uint8_t)val.toInt(); }
  operator bool() const { return val == "true" || val == "1"; }

  template <typename T> T as() const;
  template <typename T> bool is() const { return true; }
  template <typename T> T to() { return T(); }

  // Conversions to Array/Object
  operator JsonArray() const;
  operator JsonObject() const;

  template <typename T> void operator=(T v) { val = JsonVariant(v).val; }
};

// JsonPair
struct JsonPair {
  String _k;
  JsonVariant _v;
  String key() const { return _k; }
  JsonVariant value() const { return _v; }
};

// JsonArray Mock
class JsonArray {
public:
  std::vector<JsonVariant> *_dataPtr;

  JsonArray() : _dataPtr(nullptr) {}
  JsonArray(std::vector<JsonVariant> *ptr) : _dataPtr(ptr) {}

  typedef std::vector<JsonVariant>::iterator iterator;
  typedef std::vector<JsonVariant>::const_iterator const_iterator;

  iterator begin() { return _dataPtr ? _dataPtr->begin() : iterator(); }
  iterator end() { return _dataPtr ? _dataPtr->end() : iterator(); }
  const_iterator begin() const {
    return _dataPtr ? _dataPtr->begin() : const_iterator();
  }
  const_iterator end() const {
    return _dataPtr ? _dataPtr->end() : const_iterator();
  }

  template <typename T> void add(T v) {
    if (_dataPtr)
      _dataPtr->push_back(JsonVariant(v));
  }
  template <typename T> T add() { return T(); }

  template <typename T> T to() { return T(); }
};

// JsonObject Mock
class JsonObject {
public:
  std::map<std::string, JsonVariant> *_dataPtr;

  JsonObject() : _dataPtr(nullptr) {}
  JsonObject(std::map<std::string, JsonVariant> *ptr) : _dataPtr(ptr) {}

  JsonVariant &operator[](String key) {
    if (!_dataPtr) {
      static JsonVariant dummy;
      return dummy;
    }
    return (*_dataPtr)[(std::string)key];
  }
  JsonVariant &operator[](const char *key) { return (*this)[String(key)]; }

  // Iterator yielding JsonPair
  struct iterator {
    std::map<std::string, JsonVariant>::iterator _it;
    bool operator!=(const iterator &other) const { return _it != other._it; }
    iterator &operator++() {
      ++_it;
      return *this;
    }
    JsonPair operator*() const { return {_it->first.c_str(), _it->second}; }
  };
  struct const_iterator {
    std::map<std::string, JsonVariant>::const_iterator _it;
    bool operator!=(const const_iterator &other) const {
      return _it != other._it;
    }
    const_iterator &operator++() {
      ++_it;
      return *this;
    }
    JsonPair operator*() const { return {_it->first.c_str(), _it->second}; }
  };

  iterator begin() {
    if (!_dataPtr)
      return iterator();
    return {_dataPtr->begin()};
  }
  iterator end() {
    if (!_dataPtr)
      return iterator();
    return {_dataPtr->end()};
  }
  const_iterator begin() const {
    if (!_dataPtr)
      return const_iterator();
    return {_dataPtr->begin()};
  }
  const_iterator end() const {
    if (!_dataPtr)
      return const_iterator();
    return {_dataPtr->end()};
  }

  template <typename T> T to() { return T(); }
  template <typename T> T add() { return T(); }
  template <typename T> T as() { return (T) * this; }

  bool containsKey(const char* key) const {
    if (!_dataPtr) return false;
    return _dataPtr->count((std::string)key) > 0;
  }
};

// Implement conversions
inline JsonVariant::operator JsonArray() const { return JsonArray(); }
inline JsonVariant::operator JsonObject() const { return JsonObject(); }

// Implement as<T> specializations
template <> inline String JsonVariant::as<String>() const { return val; }
template <> inline int JsonVariant::as<int>() const { return val.toInt(); }
template <typename T> T JsonVariant::as() const { return T(); }

class JsonDocument {
public:
  std::map<std::string, JsonVariant> _data;
  std::vector<JsonVariant> _arrayData;
  bool _isArray = false;

  JsonVariant &operator[](String key) { return _data[(std::string)key]; }
  JsonVariant &operator[](const char *key) { return _data[(std::string)key]; }

  template <typename T> T to() { return T(); }
  template <typename T> T as() { return T(); }

  bool containsKey(const char* key) const {
    return _data.count((std::string)key) > 0;
  }

  void clear() {
    _data.clear();
    _arrayData.clear();
  }
};

template <> inline JsonArray JsonDocument::to<JsonArray>() {
  _isArray = true;
  _arrayData.clear();
  return JsonArray(&_arrayData);
}
template <> inline JsonObject JsonDocument::to<JsonObject>() {
  _isArray = false;
  _data.clear();
  return JsonObject(&_data);
}

inline void serializeJson(const JsonDocument &doc, String &out) {
  if (doc._isArray) {
    out = "[";
    for (size_t i = 0; i < doc._arrayData.size(); ++i) {
      if (i > 0)
        out += ",";
      String val = doc._arrayData[i].val;
      bool quote = true;
      if (val == "true" || val == "false")
        quote = false;
      else if (val.find_first_not_of("0123456789.-") == std::string::npos &&
               !val.empty())
        quote = false;
      else if (val.startsWith("{") || val.startsWith("["))
        quote = false;

      if (quote)
        out += "\"" + val + "\"";
      else
        out += val;
    }
    out += "]";
  } else {
    out = "{";
    bool first = true;
    for (auto const &pair : doc._data) {
      if (!first)
        out += ",";
      first = false;
      out += "\"" + String(pair.first.c_str()) + "\":";
      String val = pair.second.val;
      bool quote = true;
      if (val == "true" || val == "false")
        quote = false;
      else if (val.find_first_not_of("0123456789.-") == std::string::npos &&
               !val.empty())
        quote = false;
      else if (val.startsWith("{") || val.startsWith("["))
        quote = false;

      if (quote)
        out += "\"" + val + "\"";
      else
        out += val;
    }
    out += "}";
  }
}

inline void serializeJson(const JsonDocument &doc, Print &out) {
  String s;
  serializeJson(doc, s);
  out.print(s.c_str());
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
  doc.clear();
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

class File;
inline DeserializationError deserializeJson(JsonDocument &doc, File &file) {
  return DeserializationError::Ok;
}

#endif
