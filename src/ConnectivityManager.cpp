#include "ConnectivityManager.h"
#include <ArduinoJson.h>

ConnectivityManager::ConnectivityManager() : _server(80) {}

void ConnectivityManager::setup() {
    Serial.println("ConnectivityManager: Initializing...");

    // 1. Initialize File System
    if (!LittleFS.begin(true)) {
        Serial.println("ConnectivityManager: LittleFS Mount Failed");
    } else {
        Serial.printf("ConnectivityManager: LittleFS Total: %lu, Used: %lu\n", (unsigned long)LittleFS.totalBytes(), (unsigned long)LittleFS.usedBytes());
    }

    // 2. WiFi Setup
    _wifiManager.setConfigPortalTimeout(180);
    _wifiManager.setAPCallback([](WiFiManager* wm) {
        Serial.println("ConnectivityManager: Entered Config Portal");
    });

    if (!_wifiManager.autoConnect("NIMRS-Decoder")) {
        Serial.println("ConnectivityManager: Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    Serial.print("ConnectivityManager: Connected. IP: ");
    Serial.println(WiFi.localIP());
    SystemContext::getInstance().getState().wifiConnected = true;

    // 3. Web Server & OTA
    _server.on("/", HTTP_GET, [this]() {
        String html = "<html><body><h1>NIMRS Decoder</h1>";
        html += "<p>Status: OK</p>";
        html += "<p><a href='/update'>Firmware Update</a></p>";
        html += "</body></html>";
        _server.send(200, "text/html", html);
    });

    _server.on("/api/status", HTTP_GET, [this]() {
        SystemState& state = SystemContext::getInstance().getState();
        JsonDocument doc;
        
        doc["address"] = state.dccAddress;
        doc["speed"] = state.speed;
        doc["direction"] = state.direction ? "forward" : "reverse";
        doc["wifi"] = state.wifiConnected;
        doc["uptime"] = millis() / 1000;

        String output;
        serializeJson(doc, output);
        _server.send(200, "application/json", output);
    });

    _httpUpdater.setup(&_server, "/update");
    _server.begin();
    Serial.println("ConnectivityManager: Web Server started on port 80");
}

void ConnectivityManager::loop() {
    _server.handleClient();
}
