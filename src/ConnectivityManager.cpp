#include "ConnectivityManager.h"
#include "AudioController.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "LameJs.h"
#include "MotorController.h"
#include "WebAssets.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

#ifndef BUILD_VERSION
#define BUILD_VERSION "dev"
#endif

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

#define AUTH_CHECK()                                                           \
  if (!isAuthenticated())                                                      \
  return

ConnectivityManager::ConnectivityManager() : _server(80) {}

void ConnectivityManager::setup() {
  Log.println("ConnectivityManager: Initializing...");

  // 1. Initialize File System
  if (!LittleFS.begin(true)) {
    Log.println("ConnectivityManager: LittleFS Mount Failed");
  } else {
    Log.printf("ConnectivityManager: LittleFS Total: %lu, Used: %lu\n",
               (unsigned long)LittleFS.totalBytes(),
               (unsigned long)LittleFS.usedBytes());
  }

  // 2. WiFi Setup
  Preferences prefs;
  prefs.begin("config", true); // Read-only mode
  _hostname = prefs.getString("hostname", "NIMRS-Decoder");
  _webUser = prefs.getString("web_user", "admin");
  _webPass = prefs.getString("web_pass", "admin");
  prefs.end();

  Log.printf("ConnectivityManager: Hostname: %s\n", _hostname.c_str());
  WiFi.setHostname(_hostname.c_str());

  // Try to connect using stored credentials
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  _connectStartTime = millis();
  _wifiState = WIFI_CONNECTING;

  Log.println("ConnectivityManager: Attempting connection...");

  // 3. Web Server Handlers

  // Embedded UI
  _server.on("/", HTTP_GET, [this]() {
    AUTH_CHECK();
    _server.send(200, "text/html", INDEX_HTML);
  });
  _server.on("/index.html", HTTP_GET, [this]() {
    AUTH_CHECK();
    _server.send(200, "text/html", INDEX_HTML);
  });
  _server.on("/style.css", HTTP_GET, [this]() {
    AUTH_CHECK();
    _server.send(200, "text/css", STYLE_CSS);
  });
  _server.on("/app.js", HTTP_GET, [this]() {
    AUTH_CHECK();
    _server.send(200, "application/javascript", APP_JS);
  });
  _server.on("/lame.min.js", HTTP_GET, [this]() {
    AUTH_CHECK();
    _server.send_P(200, "application/javascript", LAME_MIN_JS);
  });

  // API: System Status
  _server.on("/api/status", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleStatus();
  });

  // API: Hostname Config
  _server.on("/api/config/hostname", HTTP_POST, [this]() {
    AUTH_CHECK();
    if (!_server.hasArg("name")) {
      _server.send(400, "text/plain", "Missing name");
      return;
    }
    String newName = _server.arg("name");
    if (newName.length() > 0 && newName.length() < 32) {
      Preferences prefs;
      prefs.begin("config", false); // Read-write
      prefs.putString("hostname", newName);
      prefs.end();
      _server.send(200, "text/plain", "Hostname saved. Restart required.");
      _shouldRestart = true;
      _restartTimer = millis();
    } else {
      _server.send(400, "text/plain", "Invalid name length");
    }
  });

  // API: Web Authentication Config
  _server.on("/api/config/webauth", HTTP_POST, [this]() {
    AUTH_CHECK();
    if (!_server.hasArg("user") || !_server.hasArg("pass")) {
      _server.send(400, "text/plain", "Missing user or pass");
      return;
    }
    String newUser = _server.arg("user");
    String newPass = _server.arg("pass");

    // Allow empty user/pass to disable authentication
    if (newUser.length() < 32 && newPass.length() < 32) {
      _webUser = newUser;
      _webPass = newPass;
      Preferences prefs;
      prefs.begin("config", false);
      prefs.putString("web_user", _webUser);
      prefs.putString("web_pass", _webPass);
      prefs.end();
      _server.send(200, "text/plain",
                   "Web credentials saved. Restart required.");
      _shouldRestart = true;
      _restartTimer = millis();
    } else {
      _server.send(400, "text/plain", "Invalid length");
    }
  });

  // API: Logs
  _server.on("/api/logs", HTTP_GET, [this]() {
    AUTH_CHECK();
    String filter = "";
    if (_server.hasArg("type")) {
      String type = _server.arg("type");
      if (type == "data")
        filter = "[NIMRS_DATA]";
      else if (type == "debug")
        filter = "DCC:"; // Example: filter for DCC debug
    }
    _server.send(200, "application/json", Log.getLogsJSON(filter));
  });

  // API: File List
  _server.on("/api/files/list", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleFileList();
  });

  // API: File Delete
  _server.on("/api/files/delete", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleFileDelete();
  });
  _server.on("/api/files/delete", HTTP_DELETE, [this]() {
    AUTH_CHECK();
    handleFileDelete();
  });

  // API: File Upload
  _server.on(
      "/api/files/upload", HTTP_POST,
      [this]() {
        AUTH_CHECK();
        _server.send(200, "text/plain", "Upload OK");
      },
      [this]() {
        AUTH_CHECK();
        handleFileUpload();
      });

  // API: WiFi Management
  _server.on("/api/wifi/save", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleWifiSave();
  });
  _server.on("/api/wifi/reset", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleWifiReset();
  });
  _server.on("/api/wifi/scan", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleWifiScan();
  });

  // API: Control
  _server.on("/api/control", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleControl();
  });
  _server.on("/api/cv", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleCV();
  });
  _server.on("/api/cv/all", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleCvAll();
  });
  _server.on("/api/cv/all", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleCvAll();
  });
  _server.on("/api/audio/play", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleAudioPlay();
  });

  // API: Motor Test
  _server.on("/api/motor/test", [this]() {
    AUTH_CHECK();
    if (_server.method() == HTTP_POST) {
      MotorController::getInstance().startTest();
      _server.send(200, "application/json", "{\"status\":\"started\"}");
    } else if (_server.method() == HTTP_GET) {
      String json = MotorController::getInstance().getTestJSON();
      _server.send(200, "application/json", json);
    } else {
      _server.send(405, "text/plain", "Method Not Allowed");
    }
  });

  // API: Motor Calibration
  _server.on("/api/motor/calibrate", [this]() {
    AUTH_CHECK();
    if (_server.method() == HTTP_POST) {
      MotorController::getInstance().measureResistance();
      _server.send(200, "application/json", "{\"status\":\"started\"}");
    } else if (_server.method() == HTTP_GET) {
      JsonDocument doc;
      auto state = MotorController::getInstance().getResistanceState();
      switch (state) {
      case MotorController::ResistanceState::IDLE:
        doc["state"] = "IDLE";
        break;
      case MotorController::ResistanceState::MEASURING:
        doc["state"] = "MEASURING";
        break;
      case MotorController::ResistanceState::DONE:
        doc["state"] = "DONE";
        break;
      case MotorController::ResistanceState::ERROR:
        doc["state"] = "ERROR";
        break;
      }
      doc["resistance"] =
          MotorController::getInstance().getMeasuredResistance();
      String output;
      serializeJson(doc, output);
      _server.send(200, "application/json", output);
    } else {
      _server.send(405, "text/plain", "Method Not Allowed");
    }
  });

  // API: CV Definitions
  _server.on("/api/cv/defs", HTTP_GET, [this]() {
    AUTH_CHECK();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (size_t i = 0; i < CV_DEFS_COUNT; i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["cv"] = CV_DEFS[i].id;
      obj["name"] = CV_DEFS[i].name;
      obj["desc"] = CV_DEFS[i].desc;
    }
    sendJson(doc);
  });

  // OTA Updater
  _httpUpdater.setup(&_server, "/update", _webUser.c_str(), _webPass.c_str());

  // Static File Catch-All (For serving audio files or other assets from FS)
  _server.onNotFound([this]() {
    AUTH_CHECK();
    handleStaticFile();
  });

  _server.begin();
  Log.println("ConnectivityManager: Web Server started on port 80");
}

void ConnectivityManager::loop() {
  _server.handleClient();

  if (_wifiState == WIFI_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) {
      Log.print("ConnectivityManager: Connected. IP: ");
      Log.println(WiFi.localIP());
      SystemContext::getInstance().getState().wifiConnected = true;
      _wifiState = WIFI_CONNECTED;
    } else if (millis() - _connectStartTime > 10000 ||
               WiFi.status() == WL_CONNECT_FAILED) {
      // Timeout after 10 seconds or immediate failure
      Log.println(
          "ConnectivityManager: Connection failed/timeout. Starting AP...");
      _wifiState = WIFI_AP_MODE;
      WiFi.softAP(_hostname.c_str());

      Log.print("ConnectivityManager: AP Started. IP: ");
      Log.println(WiFi.softAPIP());
    }
  }

  if (_shouldRestart && millis() - _restartTimer > 1000) {
    Log.println("Rebooting...");
    ESP.restart();
  }
}

// --- File Management Implementation ---

/**
 * Helper to bridge ArduinoJson serialization and WebServer chunked streaming.
 */
class ChunkedPrint : public Print {
public:
  ChunkedPrint(WebServer &server) : _server(server), _pos(0) {}
  ~ChunkedPrint() { flush(); }

  size_t write(uint8_t c) override {
    if (_pos >= sizeof(_buffer)) {
      flush();
    }
    _buffer[_pos++] = c;
    return 1;
  }

  size_t write(const uint8_t *buffer, size_t size) override {
    flush();
    _server.sendContent((const char *)buffer, size);
    return size;
  }

  void flush() {
    if (_pos > 0) {
      _server.sendContent((const char *)_buffer, _pos);
      _pos = 0;
    }
  }

private:
  WebServer &_server;
  uint8_t _buffer[128];
  size_t _pos;
};

void ConnectivityManager::handleFileList() {
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    _server.send(500, "application/json",
                 "{\"error\":\"Failed to open root\"}");
    return;
  }

  _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  _server.send(200, "application/json", "");
  _server.sendContent("[");

  {
    ChunkedPrint writer(_server);
    JsonDocument doc;
    bool first = true;

    File file = root.openNextFile();
    while (file) {
      if (!first) {
        _server.sendContent(",");
      }
      first = false;

      doc.clear();
      JsonObject obj = doc.to<JsonObject>();
      const char *fileName = file.name();
      if (fileName[0] != '/') {
        String path = "/" + String(fileName);
        obj["name"] = path;
      } else {
        obj["name"] = fileName;
      }
      obj["size"] = file.size();

      serializeJson(doc, writer);
      file = root.openNextFile();
    }
  }

  _server.sendContent("]");
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
  HTTPUpload &upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
      filename = "/" + filename;

    // Workaround: Some clients/browsers may include a null terminator in the
    // filename which triggers our security check. Strip trailing null bytes.
    while (filename.length() > 0 &&
           filename.charAt(filename.length() - 1) == 0) {
      filename.remove(filename.length() - 1);
    }

    // Security Check: Path Traversal
    if (filename.indexOf("..") >= 0) {
      Log.printf("Upload Blocked: Path traversal detected in %s\n",
                 filename.c_str());
      fsUploadFile = File(); // Ensure invalid
      return;
    }

    // Security Check: Null Byte Injection
    // Note: indexOf('\0') returns length() on some Arduino cores if no embedded
    // null is found. Check if index is within valid range.
    if (filename.indexOf('\0') >= 0 &&
        (unsigned int)filename.indexOf('\0') < filename.length()) {
      Log.printf("Upload Blocked: Null byte detected in %s\n",
                 filename.c_str());
      fsUploadFile = File(); // Ensure invalid
      return;
    }

    // Security Check: Whitelist Extensions
    // Only allow specific asset types to prevent uploading malicious scripts
    // (HTML/JS) or overwriting system files.
    bool allowed = false;
    String lowerName = filename;
    lowerName.toLowerCase();
    if (lowerName.endsWith(".json") || lowerName.endsWith(".wav") ||
        lowerName.endsWith(".mp3")) {
      allowed = true;
    }

    if (!allowed) {
      Log.printf("Upload Blocked: Invalid extension for %s\n",
                 filename.c_str());
      fsUploadFile = File(); // Ensure invalid
      return;
    }

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
      Log.printf("Upload End: %lu bytes\n", (unsigned long)upload.totalSize);

      // Hot-reload sound assets if the config file was just uploaded
      if (upload.filename.endsWith("sound_assets.json")) {
        Log.println("Audio: Hot-reloading assets...");
        AudioController::getInstance().loadAssets();
      }
    }
  }
}

void ConnectivityManager::handleStaticFile() {
  String path = _server.uri();

  // Mime Types
  String contentType = "text/plain";
  if (path.endsWith(".html"))
    contentType = "text/html";
  else if (path.endsWith(".css"))
    contentType = "text/css";
  else if (path.endsWith(".js"))
    contentType = "application/javascript";
  else if (path.endsWith(".png"))
    contentType = "image/png";
  else if (path.endsWith(".ico"))
    contentType = "image/x-icon";
  else if (path.endsWith(".json"))
    contentType = "application/json";
  else if (path.endsWith(".wav"))
    contentType = "audio/wav";
  else if (path.endsWith(".mp3"))
    contentType = "audio/mpeg";

  File file = LittleFS.open(path, "r");
  if (file) {
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

  _shouldRestart = true;
  _restartTimer = millis();
}

void ConnectivityManager::handleWifiReset() {
  Log.println("Resetting WiFi Settings...");
  // Clear WiFi credentials from NVS
  WiFi.disconnect(true, true);
  _server.send(200, "text/plain", "WiFi settings reset. Restarting...");

  _shouldRestart = true;
  _restartTimer = millis();
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

  sendJson(doc);
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
  SystemContext &ctx = SystemContext::getInstance();
  ScopedLock lock(ctx);
  SystemState &state = ctx.getState();

  if (action == "stop") {
    state.speed = 0;
    state.speedSource = SOURCE_WEB;
    Log.println("Web: STOP");
  } else if (action == "toggle_lights") {
    state.functions[0] = !state.functions[0];
    Log.printf("Web: Lights %s\n", state.functions[0] ? "ON" : "OFF");
  } else if (action == "set_function") {
    int idx = doc["index"];
    bool val = doc["value"];
    if (idx >= 0 && idx < 29) {
      state.functions[idx] = val;
      Log.printf("Web: F%d %s\n", idx, val ? "ON" : "OFF");
    }
  } else if (action == "set_speed") {
    int val = doc["value"];
    // Map 0-126 (DCC steps) to 0-255 (PWM)
    state.speed = map(val, 0, 126, 0, 255);
    state.speedSource = SOURCE_WEB;
    Log.printf("Web: Speed Step %d -> PWM %d\n", val, state.speed);
  } else if (action == "set_direction") {
    state.direction = doc["value"];
    state.speedSource = SOURCE_WEB;
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
    snprintf(buf, sizeof(buf), "{\"cv\":%d,\"value\":%d}", cv, val);
    _server.send(200, "application/json", buf);
  } else if (cmd == "write") {
    int cv = doc["cv"];
    int val = doc["value"];
    DccController::getInstance().getDcc().setCV(cv, val);
    _server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    _server.send(200, "text/plain", "Unknown cmd");
  }
}

void ConnectivityManager::handleCvAll() {
  if (_server.method() == HTTP_GET) {
    JsonDocument doc;
    NmraDcc &dcc = DccController::getInstance().getDcc();

    // Loop through ALL defined CVs in our Registry
    for (size_t i = 0; i < CV_DEFS_COUNT; i++) {
      uint16_t id = CV_DEFS[i].id;
      char buf[12];
      snprintf(buf, sizeof(buf), "%d", id);
      doc[buf] = dcc.getCV(id);
    }

    sendJson(doc);
  } else if (_server.method() == HTTP_POST) {
    if (!_server.hasArg("plain")) {
      _server.send(400, "text/plain", "Body missing");
      return;
    }

    JsonDocument doc;
    deserializeJson(doc, _server.arg("plain"));
    JsonObject obj = doc.as<JsonObject>();
    NmraDcc &dcc = DccController::getInstance().getDcc();

    for (JsonPair p : obj) {
      uint16_t cv = String(p.key().c_str()).toInt();
      uint8_t val = p.value().as<uint8_t>();
      if (cv > 0) {
        dcc.setCV(cv, val);
        Log.printf("Web Bulk: Write CV%d = %d\n", cv, val);
      }
    }
    _server.send(200, "application/json", "{\"status\":\"ok\"}");
  }
}

void ConnectivityManager::handleAudioPlay() {
  if (!_server.hasArg("file")) {
    _server.send(400, "text/plain", "Missing file arg");
    return;
  }
  String file = _server.arg("file");
  // Ensure leading slash
  if (!file.startsWith("/"))
    file = "/" + file;

  AudioController::getInstance().playFile(file.c_str());
  _server.send(200, "text/plain", "Playing");
}

void ConnectivityManager::handleStatus() {
  SystemContext &ctx = SystemContext::getInstance();
  ScopedLock lock(ctx);
  SystemState &state = ctx.getState();
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

  // Retrieve hostname dynamically (in case it changed)
  Preferences prefs;
  prefs.begin("config", true);
  doc["hostname"] = prefs.getString("hostname", "NIMRS-Decoder");
  prefs.end();

  doc["fs_total"] = LittleFS.totalBytes();
  doc["fs_used"] = LittleFS.usedBytes();

  JsonArray funcs = doc["functions"].to<JsonArray>();
  for (int i = 0; i < 29; i++)
    funcs.add(state.functions[i]);

  sendJson(doc);
}

void ConnectivityManager::sendJson(const JsonDocument &doc) {
  String output;
  serializeJson(doc, output);
  _server.send(200, "application/json", output);
}

bool ConnectivityManager::isAuthenticated() {
  if (_webUser.length() == 0 ||
      _server.authenticate(_webUser.c_str(), _webPass.c_str())) {
    return true;
  }
  _server.requestAuthentication();
  return false;
}