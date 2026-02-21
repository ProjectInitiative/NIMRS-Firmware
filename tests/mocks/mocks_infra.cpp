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

AudioController &AudioController::getInstance() {
  static AudioController instance;
  return instance;
}
AudioController::AudioController() {}
void AudioController::loadAssets() {}
void AudioController::playFile(const char *file) {}

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
