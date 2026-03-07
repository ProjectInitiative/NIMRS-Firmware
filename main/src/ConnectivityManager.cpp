#include "ConnectivityManager.h"
#include "AudioController.h"
#include "BootLoopDetector.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "LameJs.h"
#include "MotorController.h"
#include "WebAssets.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_app_format.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

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
  // We attempt to mount WITHOUT formatting first.
  // If it fails (usually due to stale data from a different partition layout),
  // we explicitly format it. This avoids the internal LFS assert during a
  // "format-on-failure" mount.
  if (!LittleFS.begin(false)) {
    Log.println("ConnectivityManager: LittleFS Mount Failed. Formatting...");
    if (LittleFS.format()) {
      Log.println("ConnectivityManager: LittleFS Format Successful.");
      if (!LittleFS.begin(true)) {
        Log.println("ConnectivityManager: LittleFS Mount Failed AFTER format!");
      }
    } else {
      Log.println("ConnectivityManager: LittleFS Format FAILED!");
    }
  }

  if (LittleFS.usedBytes() >= 0) { // Check if mounted correctly
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
  /**
   * @api {GET} / Root Index
   * @apiGroup System
   * @apiDescription Serves the main web interface.
   */
  _server.on("/", HTTP_GET, [this]() {
    AUTH_CHECK();
    _server.send(200, "text/html", INDEX_HTML);
  });
  /**
   * @api {GET} /index.html Index HTML
   * @apiGroup System
   * @apiDescription Serves the main web interface (alias).
   */
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
  /**
   * @api {GET} /api/status System Status
   * @apiGroup Status
   * @apiDescription Retrieves the current system status.
   * @apiSuccess {Number} address Current DCC address.
   * @apiSuccess {Number} speed Current speed (0-126).
   * @apiSuccess {String} direction "forward" or "reverse".
   * @apiSuccess {Boolean} wifi WiFi connection status.
   * @apiSuccess {Number} uptime System uptime in seconds.
   * @apiSuccess {String} version Firmware build version.
   * @apiSuccess {String} hash Git commit hash.
   * @apiSuccess {String} hostname Device hostname.
   * @apiSuccess {Number} fs_total Total filesystem size.
   * @apiSuccess {Number} fs_used Used filesystem size.
   * @apiSuccess {Array} functions Array of 29 booleans (F0-F28).
   */
  _server.on("/api/status", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleStatus();
  });

  // API: Hostname Config
  /**
   * @api {POST} /api/config/hostname Set Hostname
   * @apiGroup Config
   * @apiDescription Updates the device hostname. Requires a restart.
   * @apiParam {String} name New hostname (max 31 chars).
   * @apiSuccess {String} text "Hostname saved. Restart required."
   * @apiError {String} text "Missing name" or "Invalid name length".
   */
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
  /**
   * @api {POST} /api/config/webauth Set Web Credentials
   * @apiGroup Config
   * @apiDescription Updates web interface credentials. Empty strings disable
   * auth.
   * @apiParam {String} user Username (max 31 chars).
   * @apiParam {String} pass Password (max 31 chars).
   * @apiSuccess {String} text "Web credentials saved. Restart required."
   * @apiError {String} text "Missing user or pass" or "Invalid length".
   */
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
  /**
   * @api {GET} /api/logs Get Logs
   * @apiGroup Logs
   * @apiDescription Retrieves system logs.
   * @apiParam {String} [type] Filter by type: "data" or "debug".
   * @apiSuccess {Array} logs Array of log strings.
   */
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
  /**
   * @api {DELETE} /api/logs Clear Logs
   * @apiGroup Logs
   * @apiDescription Clears all system and telemetry logs.
   * @apiSuccess {JSON} status {"status": "cleared"}
   */
  _server.on("/api/logs", HTTP_DELETE, [this]() {
    AUTH_CHECK();
    Log.clear();
    _server.send(200, "application/json", "{\"status\":\"cleared\"}");
  });

  // API: File List
  /**
   * @api {GET} /api/files/list List Files
   * @apiGroup Files
   * @apiDescription Lists all files in LittleFS.
   * @apiSuccess {Array} files Array of objects {name, size}.
   */
  _server.on("/api/files/list", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleFileList();
  });

  // API: File Delete
  /**
   * @api {POST} /api/files/delete Delete File
   * @apiGroup Files
   * @apiDescription Deletes a file.
   * @apiParam {String} path Path to the file.
   * @apiSuccess {String} text "Deleted"
   * @apiError {String} text "File not found" or "Missing path argument"
   */
  _server.on("/api/files/delete", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleFileDelete();
  });
  /**
   * @api {DELETE} /api/files/delete Delete File (Method)
   * @apiGroup Files
   * @apiDescription Deletes a file.
   * @apiParam {String} path Path to the file.
   * @apiSuccess {String} text "Deleted"
   * @apiError {String} text "File not found" or "Missing path argument"
   */
  _server.on("/api/files/delete", HTTP_DELETE, [this]() {
    AUTH_CHECK();
    handleFileDelete();
  });

  // API: File Format (DANGER)
  /**
   * @api {POST} /api/files/format Format Filesystem
   * @apiGroup Files
   * @apiDescription Formats the LittleFS partition. DANGER: Deletes all files.
   * @apiSuccess {String} text "Formatting started..."
   */
  _server.on("/api/files/format", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleFileFormat();
  });

  // API: File Upload
  /**
   * @api {POST} /api/files/upload Upload File
   * @apiGroup Files
   * @apiDescription Uploads a file (multipart/form-data).
   * @apiParam {File} file The file to upload.
   * @apiSuccess {String} text "Upload OK"
   */
  _server.on(
      "/api/files/upload", HTTP_POST,
      [this]() {
        // We check auth in the response phase too, but rely on
        // _uploadAuthPassed/Error for status
        if (_uploadError == "Unauthorized") {
          // Auth failed during upload start
          return; // requestAuthentication() was likely called inside
                  // handleFileUpload
        }
        AUTH_CHECK();
        if (_uploadError.length() > 0) {
          _server.send(500, "text/plain", _uploadError);
        } else {
          _server.send(200, "text/plain", "Upload OK");
        }
      },
      [this]() {
        // AUTH_CHECK removed here to prevent repeated auth calls during
        // streaming
        handleFileUpload();
      });

  // API: WiFi Management
  /**
   * @api {POST} /api/wifi/save Save WiFi Config
   * @apiGroup WiFi
   * @apiDescription Saves WiFi credentials and restarts.
   * @apiParam {String} ssid SSID.
   * @apiParam {String} pass Password.
   * @apiSuccess {String} text "WiFi credentials saved. Restarting..."
   */
  _server.on("/api/wifi/save", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleWifiSave();
  });
  /**
   * @api {POST} /api/wifi/reset Reset WiFi
   * @apiGroup WiFi
   * @apiDescription Clears WiFi credentials and restarts.
   * @apiSuccess {String} text "WiFi settings reset. Restarting..."
   */
  _server.on("/api/wifi/reset", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleWifiReset();
  });
  /**
   * @api {GET} /api/wifi/scan Scan WiFi
   * @apiGroup WiFi
   * @apiDescription Scans for available networks.
   * @apiSuccess {Array} networks Array of {ssid, rssi, enc}.
   */
  _server.on("/api/wifi/scan", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleWifiScan();
  });

  // API: Control
  /**
   * @api {POST} /api/control Control Decoder
   * @apiGroup Control
   * @apiDescription Controls speed, direction, functions, etc.
   * @apiParam {JSON} plain JSON payload.
   * @apiParam (Payload) {String} action Action to perform ("stop",
   * "toggle_lights", "set_function", "set_speed", "set_direction",
   * "set_log_level").
   * @apiParam (Payload) {Mixed} [value] Value for the action.
   * @apiParam (Payload) {Number} [index] Function index for "set_function".
   * @apiSuccess {JSON} status {"status": "ok"}
   */
  _server.on("/api/control", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleControl();
  });
  /**
   * @api {POST} /api/cv Read/Write CV
   * @apiGroup Control
   * @apiDescription Reads or writes a single CV.
   * @apiParam {JSON} plain JSON payload.
   * @apiParam (Payload) {String} cmd "read" or "write".
   * @apiParam (Payload) {Number} cv CV number.
   * @apiParam (Payload) {Number} [value] Value to write (for "write").
   * @apiSuccess (Read) {JSON} cv {"cv": number, "value": number}
   * @apiSuccess (Write) {JSON} status {"status": "ok"}
   */
  _server.on("/api/cv", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleCV();
  });
  /**
   * @api {GET} /api/cv/all Read All CVs
   * @apiGroup Control
   * @apiDescription Reads all known CVs.
   * @apiSuccess {JSON} cvs Key-value map of CV ID to value.
   */
  _server.on("/api/cv/all", HTTP_GET, [this]() {
    AUTH_CHECK();
    handleCvAll();
  });
  /**
   * @api {POST} /api/cv/all Bulk Write CVs
   * @apiGroup Control
   * @apiDescription Writes multiple CVs.
   * @apiParam {JSON} plain JSON object where keys are CV IDs and values are
   * values.
   * @apiSuccess {JSON} status {"status": "ok"}
   */
  _server.on("/api/cv/all", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleCvAll();
  });
  /**
   * @api {POST} /api/audio/play Play Audio
   * @apiGroup Control
   * @apiDescription Plays a specific audio file.
   * @apiParam {String} file Filename (path).
   * @apiSuccess {String} text "Playing"
   */
  _server.on("/api/audio/play", HTTP_POST, [this]() {
    AUTH_CHECK();
    handleAudioPlay();
  });

  // API: Motor Test
  /**
   * @api {POST} /api/motor/test Start Motor Test
   * @apiGroup Motor
   * @apiDescription Starts the motor test profile.
   * @apiSuccess {JSON} status {"status": "started"}
   */
  /**
   * @api {GET} /api/motor/test Get Motor Test Data
   * @apiGroup Motor
   * @apiDescription Retrieves motor test telemetry data.
   * @apiSuccess {JSON} data Telemetry JSON.
   */
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
  /**
   * @api {POST} /api/motor/calibrate Start Calibration
   * @apiGroup Motor
   * @apiDescription Starts motor resistance measurement.
   * @apiSuccess {JSON} status {"status": "started"}
   */
  /**
   * @api {GET} /api/motor/calibrate Get Calibration Status
   * @apiGroup Motor
   * @apiDescription Gets current calibration state.
   * @apiSuccess {String} state "IDLE", "MEASURING", "DONE", or "ERROR".
   * @apiSuccess {Number} resistance Measured resistance in 10mOhm units.
   */
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

  // API: Telemetry
  /**
   * @api {GET} /api/telemetry Get Live Telemetry
   * @apiGroup Status
   * @apiDescription Retrieves live motor and system telemetry as JSON.
   */
  _server.on("/api/telemetry", HTTP_GET, [this]() {
    AUTH_CHECK();
    MotorTask::Status status = MotorTask::getInstance().getStatus();
    SystemState &state = SystemContext::getInstance().getState();

    JsonDocument doc;
    doc["target_speed"] = state.speed;
    doc["duty"] = status.duty;
    doc["current"] = status.current;
    doc["voltage"] = status.appliedVoltage;
    doc["rpm"] = status.estimatedRpm;
    doc["ripple_freq"] = status.rippleFreq;
    doc["stalled"] = status.stalled;

    String output;
    serializeJson(doc, output);
    _server.send(200, "application/json", output);
  });

  // API: CV Definitions
  /**
   * @api {GET} /api/cv/defs Get CV Definitions
   * @apiGroup Control
   * @apiDescription Retrieves definition of all supported CVs.
   * @apiSuccess {Array} cvs Array of {cv, name, desc}.
   */
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
  // Use manual handler to ensure proper logging and control
  _server.on(
      "/update", HTTP_POST,
      [this]() {
        // Send success
        AUTH_CHECK();
        _server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        if (!Update.hasError()) {
          _shouldRestart = true;
          _restartTimer = millis();
        }
      },
      [this]() {
        // Handle upload
        handleFirmwareUpdate();
      });

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
    // Do NOT call markSuccessful() here. We want the next boot to be
    // verified by the 30-second timer, not force-validated now.
    ESP.restart();
  }
}

// --- File Management Implementation ---

/**
 * Helper to bridge ArduinoJson serialization and WebServer chunked streaming.
 */
void ConnectivityManager::handleFileList() {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) {
    _server.send(500, "application/json",
                 "{\"error\":\"Failed to open root\"}");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    JsonObject obj = array.add<JsonObject>();
    String name = String(file.name());
    if (!name.startsWith("/"))
      name = "/" + name;
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

void ConnectivityManager::handleFileFormat() {
  Log.println("Files: Formatting LittleFS...");
  _server.send(200, "text/plain", "Formatting started...");

  // Format is blocking and can take time
  if (LittleFS.format()) {
    Log.println("Files: Format Success");
  } else {
    Log.println("Files: Format Failed");
  }

  // Re-mount to be safe
  LittleFS.end();
  LittleFS.begin(true);
}

void ConnectivityManager::handleFileUpload() {
  HTTPUpload &upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    _uploadError = "";
    _uploadAuthPassed = isAuthenticated();
    if (!_uploadAuthPassed) {
      _uploadError = "Unauthorized";
      return;
    }

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
      _uploadFile = File(); // Ensure invalid
      _uploadError = "Invalid filename (path traversal)";
      return;
    }

    // Security Check: Null Byte Injection
    // Note: indexOf('\0') returns length() on some Arduino cores if no embedded
    // null is found. Check if index is within valid range.
    if (filename.indexOf('\0') >= 0 &&
        (unsigned int)filename.indexOf('\0') < filename.length()) {
      Log.printf("Upload Blocked: Null byte detected in %s\n",
                 filename.c_str());
      _uploadFile = File(); // Ensure invalid
      _uploadError = "Invalid filename (null byte)";
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
      _uploadFile = File(); // Ensure invalid
      _uploadError = "Invalid file extension";
      return;
    }

    // Cleanup: Remove file if it already exists to ensure fresh write
    if (LittleFS.exists(filename)) {
      LittleFS.remove(filename);
    }

    Log.printf("Upload Start: %s\n", filename.c_str());
    _uploadFile = LittleFS.open(filename, "w");
    _uploadBytesWritten = 0;

    if (!_uploadFile) {
      Log.printf("Upload Error: Failed to open %s for writing. (FS Total: %lu, "
                 "Used: %lu)\n",
                 filename.c_str(), (unsigned long)LittleFS.totalBytes(),
                 (unsigned long)LittleFS.usedBytes());
      _uploadError = "File open failed";
    }
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_uploadFile) {
      size_t written = _uploadFile.write(upload.buf, upload.currentSize);
      _uploadBytesWritten += written;

      if (written != upload.currentSize) {
        Log.printf("Upload Error: Write mismatch! Expected %lu, wrote %lu (FS "
                   "Full?)\n",
                   (unsigned long)upload.currentSize, (unsigned long)written);
        _uploadFile.close();  // Stop writing to avoid further errors
        _uploadFile = File(); // Invalidate
        _uploadError = "Write failed (FS Full?)";
      }
    } else {
      // If we get here, it means file opening failed or authentication failed
      // earlier, but the client is still sending data. We ignore it. Uncomment
      // to debug why writes are skipped: Log.println("Upload Warning: Skipping
      // write (File invalid or Auth failed)");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile) {
      _uploadFile.close();
      Log.printf("Upload End: %lu bytes. Written: %lu bytes.\n",
                 (unsigned long)upload.totalSize,
                 (unsigned long)_uploadBytesWritten);

      if (_uploadBytesWritten != upload.totalSize) {
        Log.printf("Upload Error: Size mismatch! Expected %lu, wrote %lu. "
                   "Deleting %s\n",
                   (unsigned long)upload.totalSize,
                   (unsigned long)_uploadBytesWritten, upload.filename.c_str());
        LittleFS.remove(upload.filename.c_str()); // Cleanup broken file
      }

      // Hot-reload sound assets if the config file was just uploaded
      if (upload.filename.endsWith("sound_assets.json")) {
        Log.println("Audio: Hot-reloading assets...");
        AudioController::getInstance().loadAssets();
      }
    }
  }
}

void ConnectivityManager::handleFirmwareUpdate() {
  HTTPUpload &upload = _server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!isAuthenticated()) {
      return;
    }
    Log.printf("Update: Receiving %s\n", upload.filename.c_str());

    // Clear rollback acknowledgment so a future crash can be reported
    Preferences prefs;
    if (prefs.begin("bootloop", false)) {
      prefs.putBool("acknowledged", false);
      prefs.end();
    }

    // Track which partition we are updating    _ota_partition =
    // esp_ota_get_next_update_partition(NULL);

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Log.printf("Update: Success! Size: %u\n", upload.totalSize);

      // EXPLICITLY set boot partition to the one we just wrote.
      // This sets state to NEW (0) in otadata.
      if (_ota_partition) {
        esp_err_t err = esp_ota_set_boot_partition(_ota_partition);
        if (err == ESP_OK) {
          Log.printf("Update: Boot partition set to %s. State is NEW.\n",
                     _ota_partition->label);
        } else {
          Log.printf("Update: ERROR setting boot partition: 0x%x\n", err);
        }
      }

      if (!Update.hasError()) {
        _shouldRestart = true;
        _restartTimer = millis();
      }
    } else {
      Update.printError(Serial);
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
  } else if (action == "clear_rollback") {
    BootLoopDetector::clearRollback();
    Log.println("Web: Rollback Flag Cleared");
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

  RollbackInfo rb = BootLoopDetector::getRollbackInfo();
  doc["rolled_back"] = rb.rolledback;
  doc["running_version"] = rb.runningVersion;
  doc["crashed_version"] = rb.crashedVersion;

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