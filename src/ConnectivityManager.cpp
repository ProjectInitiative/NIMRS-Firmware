#include "ConnectivityManager.h"
#include "WebAssets.h"
#include "DccController.h"
#include <ArduinoJson.h>

#ifndef BUILD_VERSION
#define BUILD_VERSION "dev"
#endif

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

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
    _wifiManager.setDebugOutput(true);

    if (!_wifiManager.autoConnect("NIMRS-Decoder")) {
        Log.println("ConnectivityManager: Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    Log.print("ConnectivityManager: Connected. IP: ");
    Log.println(WiFi.localIP());
    SystemContext::getInstance().getState().wifiConnected = true;

    // 3. Web Server Handlers

    // Embedded UI
    _server.on("/", HTTP_GET, [this]() {
        _server.send(200, "text/html", INDEX_HTML);
    });
    _server.on("/index.html", HTTP_GET, [this]() {
        _server.send(200, "text/html", INDEX_HTML);
    });
    _server.on("/style.css", HTTP_GET, [this]() {
        _server.send(200, "text/css", STYLE_CSS);
    });
    _server.on("/app.js", HTTP_GET, [this]() {
        _server.send(200, "application/javascript", APP_JS);
    });

    // API: System Status
    _server.on("/api/status", HTTP_GET, [this]() {
        SystemState& state = SystemContext::getInstance().getState();
        JsonDocument doc;
        
        // Use the configured address from NmraDcc, not just the last packet address
        doc["address"] = DccController::getInstance().getDcc().getAddr();
        
        // Map internal 0-255 speed to DCC 0-126 steps for UI display
        uint8_t dccSpeed = map(state.speed, 0, 255, 0, 126);
        doc["speed"] = dccSpeed;
        
        doc["direction"] = state.direction ? "forward" : "reverse";
        doc["wifi"] = state.wifiConnected;
        doc["uptime"] = millis() / 1000;
        doc["version"] = BUILD_VERSION;
        doc["hash"] = GIT_HASH;

        JsonArray funcs = doc["functions"].to<JsonArray>();
        for(int i=0; i<29; i++) funcs.add(state.functions[i]);

        String output;
        serializeJson(doc, output);
        _server.send(200, "application/json", output);
    });

    // API: Logs
    _server.on("/api/logs", HTTP_GET, [this]() {
        _server.send(200, "application/json", Log.getLogsJSON());
    });

    // API: File List
    _server.on("/api/files/list", HTTP_GET, [this]() { handleFileList(); });

    // API: File Delete
    _server.on("/api/files/delete", HTTP_POST, [this]() { handleFileDelete(); });
    _server.on("/api/files/delete", HTTP_DELETE, [this]() { handleFileDelete(); });

    // API: File Upload
    _server.on("/api/files/upload", HTTP_POST, 
        [this]() { _server.send(200, "text/plain", "Upload OK"); }, 
        [this]() { handleFileUpload(); }
    );

    // API: WiFi Management
    _server.on("/api/wifi/save", HTTP_POST, [this]() { handleWifiSave(); });
    _server.on("/api/wifi/reset", HTTP_POST, [this]() { handleWifiReset(); });
    _server.on("/api/wifi/scan", HTTP_GET, [this]() { handleWifiScan(); });

    // API: Control
    _server.on("/api/control", HTTP_POST, [this]() { handleControl(); });
    _server.on("/api/cv", HTTP_POST, [this]() { handleCV(); });

    // OTA Updater
    _httpUpdater.setup(&_server, "/update");

    // Static File Catch-All (For serving audio files or other assets from FS)
    _server.onNotFound([this]() { handleStaticFile(); });

    _server.begin();
    Log.println("ConnectivityManager: Web Server started on port 80");
}

void ConnectivityManager::loop() {
    _server.handleClient();
}

// --- File Management Implementation ---

void ConnectivityManager::handleFileList() {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        _server.send(500, "application/json", "{\"error\":\"Failed to open root\"}");
        return;
    }

    File file = root.openNextFile();
    while (file) {
        JsonObject obj = array.add<JsonObject>();
        String name = String(file.name());
        if (!name.startsWith("/")) name = "/" + name;
        obj["name"] = name;
        obj["size"] = file.size();
        file = root.openNextFile();
    }
    
    String output;
    serializeJson(doc, output);
    _server.send(200, "application/json", output);
}

void ConnectivityManager::handleFileDelete() {
    if (!_server.hasArg("path")) {
        _server.send(400, "text/plain", "Missing path argument");
        return;
    }
    String path = _server.arg("path");
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
        _server.send(200, "text/plain", "Deleted");
    } else {
        _server.send(404, "text/plain", "File not found");
    }
}

static File fsUploadFile;

void ConnectivityManager::handleFileUpload() {
    HTTPUpload& upload = _server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) filename = "/" + filename;
        
        Log.printf("Upload Start: %s\n", filename.c_str());
        fsUploadFile = LittleFS.open(filename, "w");
        filename = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
            Log.printf("Upload End: %u bytes\n", upload.totalSize);
        }
    }
}

void ConnectivityManager::handleStaticFile() {
    String path = _server.uri();
    
    // Mime Types
    String contentType = "text/plain";
    if (path.endsWith(".html")) contentType = "text/html";
    else if (path.endsWith(".css")) contentType = "text/css";
    else if (path.endsWith(".js")) contentType = "application/javascript";
    else if (path.endsWith(".png")) contentType = "image/png";
    else if (path.endsWith(".ico")) contentType = "image/x-icon";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".wav")) contentType = "audio/wav";
    else if (path.endsWith(".mp3")) contentType = "audio/mpeg";

    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        _server.streamFile(file, contentType);
        file.close();
    } else {
        _server.send(404, "text/plain", "404: Not Found");
    }
}

void ConnectivityManager::handleWifiSave() {
    if (!_server.hasArg("ssid") || !_server.hasArg("pass")) {
        _server.send(400, "text/plain", "Missing ssid or pass");
        return;
    }

    String ssid = _server.arg("ssid");
    String pass = _server.arg("pass");

    Log.printf("WiFi Config Update: SSID=%s\n", ssid.c_str());
    
    WiFi.persistent(true);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    _server.send(200, "text/plain", "WiFi credentials saved. Restarting...");
    
    delay(1000);
    ESP.restart();
}

void ConnectivityManager::handleWifiReset() {
    Log.println("Resetting WiFi Settings...");
    _wifiManager.resetSettings();
    _server.send(200, "text/plain", "WiFi settings reset. Restarting into AP mode...");
    
    delay(1000);
    ESP.restart();
}

void ConnectivityManager::handleWifiScan() {
    Log.println("Scanning WiFi Networks...");
    int n = WiFi.scanNetworks();
    
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < n; ++i) {
        JsonObject obj = array.add<JsonObject>();
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["enc"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
    
    String output;
    serializeJson(doc, output);
    _server.send(200, "application/json", output);
}

void ConnectivityManager::handleControl() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "text/plain", "Body missing");
        return;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, _server.arg("plain"));
    
    if (error) {
        _server.send(400, "text/plain", "Invalid JSON");
        return;
    }

    String action = doc["action"];
    SystemState& state = SystemContext::getInstance().getState();
    
    if (action == "stop") {
        state.speed = 0;
        Log.println("Web: STOP");
    } else if (action == "toggle_lights") {
        state.functions[0] = !state.functions[0];
        Log.printf("Web: Lights %s\n", state.functions[0] ? "ON" : "OFF");
    } else if (action == "set_function") {
        int idx = doc["index"];
        bool val = doc["value"];
        if(idx >= 0 && idx < 29) {
            state.functions[idx] = val;
            Log.printf("Web: F%d %s\n", idx, val ? "ON" : "OFF");
        }
    } else if (action == "set_speed") {
        int val = doc["value"];
        // Map 0-126 (DCC steps) to 0-255 (PWM)
        state.speed = map(val, 0, 126, 0, 255);
        Log.printf("Web: Speed Step %d -> PWM %d\n", val, state.speed);
    } else if (action == "set_direction") {
        state.direction = doc["value"];
        Log.printf("Web: Dir %s\n", state.direction ? "FWD" : "REV");
    } else if (action == "set_log_level") {
        int level = doc["value"]; // 0=Debug, 1=Info
        Log.setLevel((LogLevel)level);
        Log.printf("Web: Log Level %d\n", level);
    } else {
        _server.send(400, "text/plain", "Unknown action");
        return;
    }
    
    _server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void ConnectivityManager::handleCV() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "text/plain", "Body missing");
        return;
    }
    
    JsonDocument doc;
    deserializeJson(doc, _server.arg("plain"));
    String cmd = doc["cmd"];
    
    if (cmd == "read") {
        int cv = doc["cv"];
        int val = DccController::getInstance().getDcc().getCV(cv);
        char buf[32];
        sprintf(buf, "{\"cv\":%d,\"value\":%d}", cv, val);
        _server.send(200, "application/json", buf);
    } else if (cmd == "write") {
        int cv = doc["cv"];
        int val = doc["value"];
        DccController::getInstance().getDcc().setCV(cv, val);
        _server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        _server.send(400, "text/plain", "Unknown cmd");
    }
}
