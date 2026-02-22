#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

#include <cctype>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <stdarg.h>
#include <stdint.h>
#include <string>
#include <vector>

class String : public std::string {
public:
  String() : std::string("") {}
  String(const char *s) : std::string(s ? s : "") {}
  String(const std::string &s) : std::string(s) {}
  String(int n) : std::string(std::to_string(n)) {}
  String(unsigned int n) : std::string(std::to_string(n)) {}
  String(long n) : std::string(std::to_string(n)) {}
  String(unsigned long n) : std::string(std::to_string(n)) {}
  String(long long n) : std::string(std::to_string(n)) {}

  bool startsWith(const String &s) const {
    if (s.length() > this->length())
      return false;
    return this->compare(0, s.length(), s) == 0;
  }
  bool endsWith(const String &s) const {
    if (this->length() < s.length())
      return false;
    return this->compare(this->length() - s.length(), s.length(), s) == 0;
  }
  int indexOf(const String &s) const {
    size_t found = this->find(s);
    if (found == std::string::npos)
      return -1;
    return (int)found;
  }
  int indexOf(char c) const {
    const char *p = strchr(this->c_str(), c);
    if (p)
      return p - this->c_str();
    return -1;
  }
  int lastIndexOf(char c) const {
    size_t found = this->rfind(c);
    if (found == std::string::npos)
      return -1;
    return (int)found;
  }
  String substring(unsigned int beginIndex,
                   unsigned int endIndex = (unsigned int)-1) const {
    if (beginIndex >= this->length())
      return String("");
    if (endIndex == (unsigned int)-1)
      endIndex = this->length();
    if (endIndex > this->length())
      endIndex = this->length();
    if (beginIndex > endIndex)
      return String("");
    return String(this->substr(beginIndex, endIndex - beginIndex));
  }
  int toInt() const {
    if (this->empty())
      return 0;
    try {
      return std::stoi(*this);
    } catch (...) {
      return 0;
    }
  }
  unsigned int length() const { return std::string::length(); }
  const char *c_str() const { return std::string::c_str(); }
  unsigned char concat(const char *cstr, unsigned int length) {
    this->append(cstr, length);
    return 1;
  }
  void toLowerCase() {
    for (size_t i = 0; i < this->length(); i++) {
      (*this)[i] = std::tolower((unsigned char)(*this)[i]);
    }
  }
  char charAt(unsigned int index) const {
    if (index >= this->length())
      return 0;
    return (*this)[index];
  }
  void remove(unsigned int index, unsigned int count = (unsigned int)-1) {
    if (index >= this->length())
      return;
    this->erase(index, count);
  }
};

#define PROGMEM
#define PSTR(s) s
#define F(s) s

extern unsigned long _mockMillis;
inline unsigned long millis() { return _mockMillis; }
inline void delay(unsigned long ms) {}

class IPAddress {
public:
  String toString() const { return "127.0.0.1"; }
  operator String() const { return toString(); }
};

class Print {
public:
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < size; i++)
      n += write(buffer[i]);
    return n;
  }
  size_t print(const String &s) {
    return write((const uint8_t *)s.c_str(), s.length());
  }
  size_t println(const String &s) { return print(s) + print("\n"); }
  size_t print(const char *s) { return print(String(s)); }
  size_t println(const char *s) { return println(String(s)); }
  size_t print(int n) { return print(String(n)); }
  size_t println(int n) { return println(String(n)); }
  size_t print(unsigned long n) { return print(String(n)); }
  size_t println(unsigned long n) { return println(String(n)); }
  virtual size_t printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
  }
};

class MockSerial : public Print {
public:
  void begin(unsigned long baud) {}
  virtual size_t write(uint8_t c) override { return putchar(c); }
  using Print::write;
};

extern MockSerial Serial;

class ESPClass {
public:
  void restart() {}
};
extern ESPClass ESP;

#define map(x, in_min, in_max, out_min, out_max)                               \
  ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

#define constrain(amt, low, high)                                              \
  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

#define OUTPUT 1
#define INPUT 0
#define HIGH 1
#define LOW 0
#define ADC_0db 0

extern std::string mockLogBuffer;

inline void pinMode(int pin, int mode) {}
inline void digitalWrite(int pin, int val) {}
inline void analogReadResolution(int bits) {}
inline void analogSetPinAttenuation(int pin, int attenuation) {}
inline void ledcSetup(int channel, int freq, int resolution) {}
inline void ledcAttachPin(int pin, int channel) {}
inline void ledcWrite(int channel, int duty) {}
inline int analogRead(int pin) { return 0; }

#endif