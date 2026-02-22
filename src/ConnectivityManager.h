#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include "Logger.h"
#include "SystemContext.h"
#include <ArduinoJson.h>
#include <HTTPUpdateServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

class ConnectivityManager {
public:
  ConnectivityManager();
  void setup();
  void loop();

private:
  WebServer _server;
  HTTPUpdateServer _httpUpdater;

  // WiFi State
  enum WifiState {
    WIFI_INIT,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_AP_MODE,
    WIFI_FAIL
  };
  WifiState _wifiState = WIFI_INIT;
  uint32_t _connectStartTime = 0;
  String _hostname;

  // File Manager Handlers
  void handleFileList();
  void handleFileUpload();
  void handleFileDelete();
  void handleStaticFile(); // Catch-all for FS files

  // WiFi Management Handlers
  void handleWifiSave();
  void handleWifiReset();
  void handleWifiScan();

  // Control Handler
  void handleControl();
  void handleStatus();
  void handleCV();
  void handleCvAll();
  void handleAudioPlay();
  void sendJson(const JsonDocument &doc);

  // Authentication
  bool isAuthenticated();
  String _webUser;
  String _webPass;

  bool _shouldRestart = false;
  uint32_t _restartTimer = 0;
};

#endif