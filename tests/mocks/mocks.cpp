#include "Arduino.h"
#include "AudioController.h"
#include "DccController.h"
#include "LittleFS.h"
#include "Logger.h"
#include "MotorController.h"
#include "WiFi.h"
#include <stdarg.h>
#include <stdio.h>

MockSerial Serial;
WiFiClass WiFi;
LittleFSClass LittleFS;
ESPClass ESP;

std::map<uint8_t, uint8_t> pinStates;
std::map<uint8_t, uint8_t> pinModes;

void pinMode(uint8_t pin, uint8_t mode) {
  pinModes[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
  pinStates[pin] = val;
}

int digitalRead(uint8_t pin) {
  return pinStates[pin];
}

// Logger methods not inline in header
size_t Logger::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  // vprintf(format, arg);
  va_end(arg);
  return 0;
}
void Logger::debug(const char *format, ...) {}
size_t Logger::write(uint8_t c) { return 1; }
size_t Logger::write(const uint8_t *buffer, size_t size) { return size; }
String Logger::getLogsJSON(const String &filter) { return "[]"; }
String Logger::getLogsHTML() { return ""; }
void Logger::_addToBuffer(const String &line) {}

AudioController &AudioController::getInstance() {
  static AudioController instance;
  return instance;
}
AudioController::AudioController() {}
void AudioController::loadAssets() {}
void AudioController::playFile(const char *file) {}

// Implement missing members of DccController (since using real header)
DccController::DccController() {}
void DccController::setup() {}
void DccController::loop() {}
void DccController::updateSpeed(uint8_t speed, bool direction) {}
void DccController::updateFunction(uint8_t functionIndex, bool active) {}
bool DccController::isPacketValid() { return true; }

MotorController &MotorController::getInstance() {
  static MotorController instance;
  return instance;
}
MotorController::MotorController() {}
void MotorController::startTest() {}
String MotorController::getTestJSON() { return "{}"; }

File File::openNextFile() {
  if (_name == "/" && _nextIdx < LittleFS.mockFiles.size()) {
    return LittleFS.mockFiles[_nextIdx++];
  }
  return File();
}
