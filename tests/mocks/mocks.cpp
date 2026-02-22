#include "Arduino.h"
#include "AudioController.h"
#ifndef SKIP_MOCK_DCC_CONTROLLER
#include "DccController.h"
#endif
#include "EEPROM.h"
#include "LittleFS.h"
#include "Logger.h"
#ifndef SKIP_MOCK_MOTOR_CONTROLLER
#include "MotorController.h"
#endif
#include "WiFi.h"
#include <stdarg.h>
#include <stdio.h>

MockSerial Serial;
WiFiClass WiFi;
LittleFSClass LittleFS;
ESPClass ESP;
EEPROMClass EEPROM;

std::string mockLogBuffer;

// Logger methods not inline in header
size_t Logger::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char buf[256];
  vsnprintf(buf, sizeof(buf), format, arg);
  va_end(arg);
  mockLogBuffer += buf;
  return 0;
}
void Logger::debug(const char *format, ...) {}
size_t Logger::write(uint8_t c) {
  mockLogBuffer += (char)c;
  return 1;
}
size_t Logger::write(const uint8_t *buffer, size_t size) {
  mockLogBuffer.append((const char *)buffer, size);
  return size;
}
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

#ifndef SKIP_MOCK_DCC_CONTROLLER
DccController &DccController::getInstance() {
  static DccController instance;
  return instance;
}
DccController::DccController() {}
NmraDcc &DccController::getDcc() {
  static NmraDcc dcc;
  return dcc;
}
bool DccController::isPacketValid() { return true; }
#endif

#ifndef SKIP_MOCK_MOTOR_CONTROLLER
MotorController &MotorController::getInstance() {
  static MotorController instance;
  return instance;
}
MotorController::MotorController() {}
void MotorController::startTest() {}
String MotorController::getTestJSON() { return "{}"; }
#endif

File File::openNextFile() {
  if (_name == "/" && _nextIdx < LittleFS.mockFiles.size()) {
    return LittleFS.mockFiles[_nextIdx++];
  }
  return File();
}
