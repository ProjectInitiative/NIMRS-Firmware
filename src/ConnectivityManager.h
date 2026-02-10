#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include "SystemContext.h"
#include "Logger.h"

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
};

#endif