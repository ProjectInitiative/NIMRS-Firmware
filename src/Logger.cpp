#include "Logger.h"
#include <ArduinoJson.h>

size_t Logger::write(uint8_t c) {
  if (_serialEnabled)
    Serial.write(c);

  if (c == '\n') {
    _addToBuffer(_currentLine);
    _currentLine = "";
  } else if (c != '\r') {
    _currentLine += (char)c;
  }
  return 1;
}

size_t Logger::write(const uint8_t *buffer, size_t size) {
  if (_serialEnabled)
    Serial.write(buffer, size);

  const char *buf = (const char *)buffer;
  size_t start = 0;

  for (size_t i = 0; i < size; i++) {
    char c = buf[i];
    if (c == '\n') {
      if (i > start) {
        _currentLine.concat(buf + start, i - start);
      }
      _addToBuffer(_currentLine);
      _currentLine = "";
      start = i + 1;
    } else if (c == '\r') {
      if (i > start) {
        _currentLine.concat(buf + start, i - start);
      }
      start = i + 1;
    }
  }

  if (start < size) {
    _currentLine.concat(buf + start, size - start);
  }
  return size;
}

size_t Logger::printf(const char *format, ...) {
  char loc_buf[64];
  char *temp = loc_buf;
  va_list arg;
  va_list copy;
  va_start(arg, format);
  va_copy(copy, arg);
  int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
  va_end(copy);
  if (len < 0) {
    va_end(arg);
    return 0;
  };
  if (len >= sizeof(loc_buf)) {
    temp = (char *)malloc(len + 1);
    if (temp == NULL) {
      va_end(arg);
      return 0;
    }
    len = vsnprintf(temp, len + 1, format, arg);
  }
  va_end(arg);

  // Write to our generic write()
  write((uint8_t *)temp, len);

  if (temp != loc_buf) {
    free(temp);
  }
  return len;
}

void Logger::debug(const char *format, ...) {
  if (_minLevel > LOG_DEBUG)
    return;

  char loc_buf[64];
  char *temp = loc_buf;
  va_list arg;
  va_list copy;
  va_start(arg, format);
  va_copy(copy, arg);
  int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
  va_end(copy);
  if (len < 0) {
    va_end(arg);
    return;
  };
  if (len >= sizeof(loc_buf)) {
    temp = (char *)malloc(len + 1);
    if (temp == NULL) {
      va_end(arg);
      return;
    }
    len = vsnprintf(temp, len + 1, format, arg);
  }
  va_end(arg);

  // Write
  write((uint8_t *)temp, len);

  if (temp != loc_buf) {
    free(temp);
  }
}

void Logger::_addToBuffer(const String &line) {
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    // Add timestamp
    String entry = "[" + String(millis()) + "] " + line;

    if (line.indexOf("[NIMRS_DATA]") != -1) {
      // High-frequency telemetry goes to the data buffer
      _dataLines.push_back(entry);
      if (_dataLines.size() > MAX_DATA_LINES) {
        _dataLines.pop_front();
      }
    } else {
      // System logs go to the main buffer
      _lines.push_back(entry);
      if (_lines.size() > MAX_LOG_LINES) {
        _lines.pop_front();
      }
    }
    xSemaphoreGive(_mutex);
  }
}

String Logger::getLogsHTML() {
  String html = "";
  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (const auto &line : _lines) {
      html += line + "<br>\n";
    }
    xSemaphoreGive(_mutex);
  }
  return html;
}

String Logger::getLogsJSON(const String &filter) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    if (filter == "[NIMRS_DATA]") {
      // Specifically requested telemetry
      for (const auto &line : _dataLines) {
        arr.add(line);
      }
    } else if (filter.length() > 0) {
      // Generic search filter in the system logs
      for (const auto &line : _lines) {
        if (line.indexOf(filter) != -1) {
          arr.add(line);
        }
      }
    } else {
      // No filter: return all system logs
      for (const auto &line : _lines) {
        arr.add(line);
      }
    }
    xSemaphoreGive(_mutex);
  }

  String output;
  serializeJson(doc, output);
  return output;
}
