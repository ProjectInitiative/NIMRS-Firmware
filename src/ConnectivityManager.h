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
};

#endif