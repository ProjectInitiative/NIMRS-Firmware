#ifndef WIFI_MOCK_H
#define WIFI_MOCK_H

#include "Arduino.h"

class WiFiClass {
public:
  String lastSSID;
  String lastPass;

  IPAddress localIP() { return IPAddress(); }
  void setHostname(const char *hostname) {}
  void persistent(bool p) {}
  void begin(const char *ssid, const char *pass) {
    lastSSID = ssid;
    lastPass = pass;
  }
  int scanNetworks() { return 0; }
  String SSID(int i) { return ""; }
  int RSSI(int i) { return 0; }
  int encryptionType(int i) { return 0; }
};

#define WIFI_AUTH_OPEN 0
extern WiFiClass WiFi;

#endif
