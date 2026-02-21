#ifndef ARDUINO_MOCK_H
#define ARDUINO_MOCK_H

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
    if (s.length() < s.length())
      return false;
    return this->compare(this->length() - s.length(), s.length(), s) == 0;
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
};

#define PROGMEM
#define PSTR(s) s
#define F(s) s

extern unsigned long _mockMillis;
inline unsigned long millis() { return _mockMillis; }
inline void delay(unsigned long ms) { _mockMillis += ms; }

// --- GPIO & ADC Mocks ---
#define OUTPUT 1
#define INPUT 0
#define ADC_0db 0

extern std::function<int(uint8_t)> _mockAnalogRead;
inline int analogRead(uint8_t pin) {
  if (_mockAnalogRead) return _mockAnalogRead(pin);
  return 0;
}
inline void analogReadResolution(int res) {}
inline void analogSetPinAttenuation(int pin, int att) {}
inline void pinMode(uint8_t pin, uint8_t mode) {}

// --- LEDC (PWM) Mocks ---
extern std::map<int, int> _mockLedcValues;
inline void ledcSetup(uint8_t channel, double freq, uint8_t resolution_bits) {}
inline void ledcAttachPin(uint8_t pin, uint8_t channel) {}
inline void ledcWrite(uint8_t channel, uint32_t duty) {
    _mockLedcValues[channel] = duty;
}

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
};

extern MockSerial Serial;

class ESPClass {
public:
  void restart() {}
};
extern ESPClass ESP;

#define map(x, in_min, in_max, out_min, out_max)                               \
  ((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min)

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#endif
