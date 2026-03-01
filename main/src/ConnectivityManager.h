#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include "Logger.h"
#include "SystemContext.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>

class ConnectivityManager {
public:
  ConnectivityManager();
  void setup();
  void loop();

private:
  WebServer _server;
  String _uploadError;
  bool _uploadAuthPassed = false;

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
  void handleFirmwareUpdate();
  void handleFileDelete();
  void handleFileFormat();
  void handleStaticFile(); // Catch-all for FS files

  File _uploadFile;
  size_t _uploadBytesWritten = 0;

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
  const esp_partition_t *_ota_partition = nullptr;
};

#endif