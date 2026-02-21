#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include "Logger.h"
#include "SystemContext.h"
#include <HTTPUpdateServer.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

class ConnectivityManager {
public:
  ConnectivityManager();
  void setup();
  void loop();

private:
  WebServer _server;
  HTTPUpdateServer _httpUpdater;
  WiFiManager _wifiManager;

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

  // Authentication
  bool isAuthenticated();
  String _webUser;
  String _webPass;

  bool _shouldRestart = false;
  uint32_t _restartTimer = 0;
};

#endif