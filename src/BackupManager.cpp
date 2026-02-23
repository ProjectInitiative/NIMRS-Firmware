#include "BackupManager.h"
#include "CvRegistry.h"
#include "DccController.h"
#include "SystemContext.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <algorithm>

BackupManager &BackupManager::getInstance() {
  static BackupManager instance;
  return instance;
}

BackupManager::BackupManager() : _state(IDLE) {}

// --- Backup Generation ---

void BackupManager::generateBackup(Print *output) {
  // 1. Create Config JSON
  String configJson = createConfigJson();

  // 2. Write Config JSON Entry
  writeTarHeader(output, "config.json", configJson.length());
  output->print(configJson);
  writePadding(output, configJson.length());

  // 3. Iterate LittleFS Files
  File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      String name = file.name();
      // Skip system files if needed, but backup everything is usually better
      // Ensure relative path (no leading slash for tar standard usually, but
      // LittleFS has them)
      if (name.startsWith("/"))
        name = name.substring(1);

      if (file.size() > 0) {
        writeTarHeader(output, name, file.size());

        // Stream content
        uint8_t buf[128];
        while (file.available()) {
          size_t read = file.read(buf, sizeof(buf));
          output->write(buf, read);
        }

        writePadding(output, file.size());
      }
      file = root.openNextFile();
    }
  }

  // 4. Write EOF (Two 512-byte blocks of zeros)
  for (int i = 0; i < 1024; i++)
    output->write(0);
}

void BackupManager::writeTarHeader(Print *out, const String &filename,
                                   size_t size) {
  uint8_t header[512];
  memset(header, 0, 512);

  // Name (0)
  strncpy((char *)header, filename.c_str(), 99);

  // Mode (100) - 0000644
  strncpy((char *)header + 100, "0000644", 7);

  // UID (108) - 0000000
  strncpy((char *)header + 108, "0000000", 7);

  // GID (116) - 0000000
  strncpy((char *)header + 116, "0000000", 7);

  // Size (124) - 11 chars + null
  snprintf((char *)header + 124, 12, "%011o", (unsigned int)size);

  // MTime (136)
  snprintf((char *)header + 136, 12, "%011o", 0);

  // Checksum (148) - Calculate as spaces first
  memset(header + 148, ' ', 8);

  // Typeflag (156) - '0'
  header[156] = '0';

  // Magic (257) - "ustar"
  strncpy((char *)header + 257, "ustar", 6);

  // Version (263) - "00"
  header[263] = '0';
  header[264] = '0';

  // Calculate Checksum
  unsigned int sum = 0;
  for (int i = 0; i < 512; i++)
    sum += header[i];

  // Write Checksum (6 digits + null + space)
  snprintf((char *)header + 148, 8, "%06o", sum);

  out->write(header, 512);
}

void BackupManager::writePadding(Print *out, size_t size) {
  size_t padding = (512 - (size % 512)) % 512;
  for (size_t i = 0; i < padding; i++)
    out->write(0);
}

String BackupManager::createConfigJson() {
  JsonDocument doc;

  // System Config
  Preferences prefs;
  prefs.begin("config", true);
  doc["hostname"] = prefs.getString("hostname", "NIMRS-Decoder");
  doc["web_user"] = prefs.getString("web_user", "admin");
  doc["web_pass"] = prefs.getString("web_pass", "admin");
  prefs.end();

  // WiFi Config (Current credentials)
  // Note: WiFi.SSID() returns current connected SSID, but we want stored ones.
  // ESP32 stores WiFi in NVS under "nvs.net80211" usually, but not easily
  // accessible via API. However, ConnectivityManager stores them? No,
  // ConnectivityManager just calls WiFi.begin(). Wait, check
  // ConnectivityManager.cpp again. It calls `WiFi.begin()`. If params are not
  // passed, it uses SDK storage. If `handleWifiSave` is called, it calls
  // `WiFi.begin(ssid, pass)`. We cannot retrieve the password from the SDK.
  // LIMITATION: We cannot backup WiFi password if it's only in SDK NVS.
  // But `ConnectivityManager` does not save them to "config" prefs?
  // Let's check `ConnectivityManager.cpp`.
  // It calls `WiFi.begin(ssid, pass)` directly. It does NOT save to Prefs.
  // So we can only backup the SSID if connected. The password is lost to us
  // (security). Unless we store them ourselves. Strategy: We will backup what
  // we can. If we can't get the password, we leave it blank. The user will have
  // to re-enter WiFi if they restore to a wiped chip. BUT, `doc["wifi_ssid"]`
  // is useful.
  doc["wifi_ssid"] = WiFi.SSID();

  // CVs
  JsonObject cvs = doc["cvs"].to<JsonObject>();
  NmraDcc &dcc = DccController::getInstance().getDcc();

  // Iterate all defined CVs
  for (size_t i = 0; i < CV_DEFS_COUNT; i++) {
    uint16_t id = CV_DEFS[i].id;
    char key[8];
    snprintf(key, sizeof(key), "%d", id);
    cvs[key] = dcc.getCV(id);
  }

  String output;
  serializeJson(doc, output);
  return output;
}

// --- Restore Logic ---

void BackupManager::beginRestore() {
  _state = READ_HEADER;
  _headerPos = 0;
  _bytesNeeded = 0;
  _currentFileSize = 0;
  _configJsonBuffer = "";
  _isConfigJson = false;
  _currentFileName = "";
}

void BackupManager::writeChunk(uint8_t *data, size_t len) {
  size_t pos = 0;

  while (pos < len) {
    if (_state == READ_HEADER) {
      size_t copyLen = std::min(len - pos, 512 - _headerPos);
      memcpy(_headerBuf + _headerPos, data + pos, copyLen);
      _headerPos += copyLen;
      pos += copyLen;

      if (_headerPos == 512) {
        // Verify Checksum? Simplified for embedded: check for empty block (EOF)
        bool isEmpty = true;
        for (int i = 0; i < 512; i++) {
          if (_headerBuf[i] != 0) {
            isEmpty = false;
            break;
          }
        }

        if (isEmpty) {
          _state = IDLE; // Done or padding
          return;
        }

        // Parse Name
        char nameBuf[101];
        memcpy(nameBuf, _headerBuf, 100);
        nameBuf[100] = 0;
        _currentFileName = String(nameBuf);

        // Parse Size (Octal)
        char sizeBuf[13];
        memcpy(sizeBuf, _headerBuf + 124, 12);
        sizeBuf[12] = 0;
        _currentFileSize = strtol(sizeBuf, NULL, 8);

        _bytesNeeded = _currentFileSize;
        _state = READ_CONTENT;

        // Check if Config
        _isConfigJson = (_currentFileName.endsWith("config.json"));
        if (_isConfigJson) {
          _configJsonBuffer = "";
        } else {
          // Open File
          String path = "/" + _currentFileName;
          if (LittleFS.exists(path))
            LittleFS.remove(path);
          _currentFile = LittleFS.open(path, "w");
        }

        if (_currentFileSize == 0) {
          // Empty file or directory
          _state = READ_HEADER; // No padding for 0 size? Wait, tar spec says
                                // data is 512 aligned.
          // 0 size means no data blocks.
          // But we still need to check if we need to reset header pos?
          // Yes.
          _headerPos = 0;
        }
      }
    } else if (_state == READ_CONTENT) {
      size_t writeLen = std::min(len - pos, _bytesNeeded);

      if (_isConfigJson) {
        // Append to string. Be careful of memory.
        // Assuming config.json is small (< few KB).
        for (size_t i = 0; i < writeLen; i++) {
          _configJsonBuffer += (char)data[pos + i];
        }
      } else if (_currentFile) {
        _currentFile.write(data + pos, writeLen);
      }

      pos += writeLen;
      _bytesNeeded -= writeLen;

      if (_bytesNeeded == 0) {
        if (_currentFile)
          _currentFile.close();

        // Calculate padding to skip
        size_t padding = (512 - (_currentFileSize % 512)) % 512;
        if (padding > 0) {
          _bytesNeeded = padding;
          _state = IGNORE_PADDING;
        } else {
          _state = READ_HEADER;
          _headerPos = 0;
        }
      }
    } else if (_state == IGNORE_PADDING) {
      size_t skipLen = std::min(len - pos, _bytesNeeded);
      pos += skipLen;
      _bytesNeeded -= skipLen;

      if (_bytesNeeded == 0) {
        _state = READ_HEADER;
        _headerPos = 0;
      }
    } else if (_state == IDLE) {
      // Consumed EOF or garbage
      pos++;
    }
  }
}

bool BackupManager::endRestore(String &errorMsg) {
  // If we have a config buffer, parse and apply it
  if (_configJsonBuffer.length() > 0) {
    return applyConfigJson(_configJsonBuffer, errorMsg);
  }
  return true;
}

bool BackupManager::applyConfigJson(const String &json, String &errorMsg) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    errorMsg = "JSON Parse Error";
    return false;
  }

  // Apply Prefs
  Preferences prefs;
  prefs.begin("config", false);
  if (doc.containsKey("hostname"))
    prefs.putString("hostname", doc["hostname"].as<String>());
  if (doc.containsKey("web_user"))
    prefs.putString("web_user", doc["web_user"].as<String>());
  if (doc.containsKey("web_pass"))
    prefs.putString("web_pass", doc["web_pass"].as<String>());
  prefs.end();

  // Apply WiFi (Best Effort)
  if (doc.containsKey("wifi_ssid")) {
    // We can't apply password if we didn't back it up.
    // But if the user provides it in the JSON manually, we could.
    // For now, we just restore the settings we can.
    // The user might need to re-configure WiFi via AP mode if password was
    // lost.
  }

  // Apply CVs
  if (doc.containsKey("cvs")) {
    JsonObject cvs = doc["cvs"];
    NmraDcc &dcc = DccController::getInstance().getDcc();

    for (JsonPair p : cvs) {
      int id = atoi(p.key().c_str());
      int val = p.value().as<int>();
      dcc.setCV(id, val);
    }
  }

  return true;
}
