#ifndef WIFI_MOCK_H
#define WIFI_MOCK_H

#include "Arduino.h"

// Define constants
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3
#define WIFI_OFF 0

#define WL_NO_SHIELD 255
#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_SCAN_COMPLETED 2
#define WL_CONNECTED 3
#define WL_CONNECT_FAILED 4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED 6

#define WIFI_AUTH_OPEN 0

typedef uint8_t WiFiMode;
typedef uint8_t wl_status_t;

class WiFiClass {
public:
  IPAddress localIP() { return IPAddress(); }
  IPAddress softAPIP() { return IPAddress(); }

  void setHostname(const char *hostname) {}
  void persistent(bool p) {}

  void begin(const char *ssid, const char *pass) {}
  void begin() {}

  void disconnect(bool wifioff = false, bool eraseap = false) {}

  bool softAP(const char *ssid, const char *passphrase = NULL, int channel = 1,
              int ssid_hidden = 0, int max_connection = 4) {
    return true;
  }

  void mode(WiFiMode m) {}

  wl_status_t status() { return _mockStatus; }

  int scanNetworks() { return 0; }
  String SSID(int i) { return ""; }
  int RSSI(int i) { return 0; }
  int encryptionType(int i) { return 0; }

  // Test helper to simulate status
  void _setStatus(wl_status_t s) { _mockStatus = s; }

private:
  wl_status_t _mockStatus = WL_CONNECTED;
};

extern WiFiClass WiFi;

#endif
