#include "ConnectivityManager.h"
#include <ArduinoJson.h>

ConnectivityManager::ConnectivityManager() : _server(80) {}

void ConnectivityManager::setup() {
    Log.println("ConnectivityManager: Initializing...");

    // 1. Initialize File System
    if (!LittleFS.begin(true)) {
        Log.println("ConnectivityManager: LittleFS Mount Failed");
    } else {
        Log.printf("ConnectivityManager: LittleFS Total: %lu, Used: %lu\n", (unsigned long)LittleFS.totalBytes(), (unsigned long)LittleFS.usedBytes());
    }

    // 2. WiFi Setup
    _wifiManager.setConfigPortalTimeout(180);
    _wifiManager.setAPCallback([](WiFiManager* wm) {
        Log.println("ConnectivityManager: Entered Config Portal");
    });

    // WiFiManager writes to Serial by default. We can't easily capture its internal logs 
    // without subclassing or setting a custom debug port (which expects Print*).
    // We can redirect WiFiManager debug:
    _wifiManager.setDebugOutput(true);
    // _wifiManager.setDebugPort(Log); // This works because Logger inherits Print!

    if (!_wifiManager.autoConnect("NIMRS-Decoder")) {
        Log.println("ConnectivityManager: Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    Log.print("ConnectivityManager: Connected. IP: ");
    Log.println(WiFi.localIP());
    SystemContext::getInstance().getState().wifiConnected = true;

    // 3. Web Server & OTA
    _server.on("/", HTTP_GET, [this]() {
        String html = "<html><head><title>NIMRS</title></head><body>";
        html += "<h1>NIMRS Decoder</h1>";
        html += "<p>Status: OK</p>";
        html += "<p><a href='/update'>Firmware Update</a></p>";
        html += "<p><a href='/logs'>Live Logs</a></p>";
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

    // Log Viewer Page
    _server.on("/logs", HTTP_GET, [this]() {
        String html = "<html><head><title>NIMRS Logs</title>";
        html += "<script>";
        html += "setInterval(function(){";
        html += "  fetch('/api/logs').then(r => r.json()).then(data => {";
        html += "    document.getElementById('logbox').innerHTML = data.join('<br>');";
        html += "    window.scrollTo(0,document.body.scrollHeight);";
        html += "  });";
        html += "}, 1000);";
        html += "</script></head>";
        html += "<body style='background:#222;color:#0f0;font-family:monospace;'>";
        html += "<h2>System Logs</h2>";
        html += "<div id='logbox'>Loading...</div>";
        html += "</body></html>";
        _server.send(200, "text/html", html);
    });

    // Log JSON API
    _server.on("/api/logs", HTTP_GET, [this]() {
        _server.send(200, "application/json", Log.getLogsJSON());
    });

    _httpUpdater.setup(&_server, "/update");
    _server.begin();
    Log.println("ConnectivityManager: Web Server started on port 80");
}

void ConnectivityManager::loop() {
    _server.handleClient();
}