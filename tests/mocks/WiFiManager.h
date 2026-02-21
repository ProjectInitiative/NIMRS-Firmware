#ifndef WIFIMANAGER_MOCK_H
#define WIFIMANAGER_MOCK_H

#include "Arduino.h"

class WiFiManager {
public:
  void setConfigPortalTimeout(int t) {}
  void setAPCallback(std::function<void(WiFiManager *)> cb) {}
  void setDebugOutput(bool d) {}
  bool autoConnect(const char *name, const char *password = NULL) {
    return true;
  }
  void resetSettings() {}
};

#endif
