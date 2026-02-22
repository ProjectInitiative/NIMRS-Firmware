#include "Logger.h"
#include <ArduinoJson.h>

Logger::Logger() {
  _historyMutex = xSemaphoreCreateMutex();
  _bufferMutex = xSemaphoreCreateMutex();
  _logQueue = NULL;
  _taskHandle = NULL;
}

void Logger::startTask() {
  if (_logQueue == NULL) {
    _logQueue = xQueueCreate(50, sizeof(LogMessage));
  }
  if (_taskHandle == NULL) {
    xTaskCreatePinnedToCore(_taskEntry, "LoggerTask", 4096, this,
                            1, // Priority 1 (Low)
                            &_taskHandle,
                            1 // Core 1
    );
  }
}

void Logger::_taskEntry(void *param) {
  Logger *self = (Logger *)param;
  self->_processQueue();
}

void Logger::_processQueue() {
  LogMessage msg;
  while (true) {
    if (xQueueReceive(_logQueue, &msg, portMAX_DELAY) == pdTRUE) {
      if (_serialEnabled) {
        Serial.println(msg.text);
      }
      _addToBuffer(msg.text);
    }
  }
}

size_t Logger::write(uint8_t c) {
  // Fallback if task not running
  if (_taskHandle == NULL) {
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

  // Async Mode
  if (xSemaphoreTake(_bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    if (c == '\n') {
      LogMessage msg;
      strncpy(msg.text, _currentLine.c_str(), sizeof(msg.text) - 1);
      msg.text[sizeof(msg.text) - 1] = '\0';
      xQueueSend(_logQueue, &msg, 0);
      _currentLine = "";
    } else if (c != '\r') {
      _currentLine += (char)c;
    }
    xSemaphoreGive(_bufferMutex);
  }
  return 1;
}

size_t Logger::write(const uint8_t *buffer, size_t size) {
  // Fallback if task not running
  if (_taskHandle == NULL) {
    if (_serialEnabled)
      Serial.write(buffer, size);

    const char *buf = (const char *)buffer;
    size_t start = 0;
    for (size_t i = 0; i < size; i++) {
      char c = buf[i];
      if (c == '\n') {
        if (i > start)
          _currentLine.concat(buf + start, i - start);
        _addToBuffer(_currentLine);
        _currentLine = "";
        start = i + 1;
      } else if (c == '\r') {
        if (i > start)
          _currentLine.concat(buf + start, i - start);
        start = i + 1;
      }
    }
    if (start < size) {
      _currentLine.concat(buf + start, size - start);
    }
    return size;
  }

  // Async Mode
  if (xSemaphoreTake(_bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    const char *buf = (const char *)buffer;
    size_t start = 0;

    for (size_t i = 0; i < size; i++) {
      char c = buf[i];
      if (c == '\n') {
        if (i > start) {
          _currentLine.concat(buf + start, i - start);
        }

        LogMessage msg;
        strncpy(msg.text, _currentLine.c_str(), sizeof(msg.text) - 1);
        msg.text[sizeof(msg.text) - 1] = '\0';
        xQueueSend(_logQueue, &msg, 0);

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
    xSemaphoreGive(_bufferMutex);
  }
  return size;
}

size_t Logger::printf(const char *format, ...) {
  char loc_buf[128]; // Increased buffer size to match LogMessage
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
  if ((size_t)len >= sizeof(loc_buf)) {
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

  char loc_buf[128];
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
  if ((size_t)len >= sizeof(loc_buf)) {
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
  if (xSemaphoreTake(_historyMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
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
    xSemaphoreGive(_historyMutex);
  }
}

String Logger::getLogsHTML() {
  String html = "";
  if (xSemaphoreTake(_historyMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    for (const auto &line : _lines) {
      html += line + "<br>\n";
    }
    xSemaphoreGive(_historyMutex);
  }
  return html;
}

String Logger::getLogsJSON(const String &filter) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  if (xSemaphoreTake(_historyMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
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
    xSemaphoreGive(_historyMutex);
  }

  String output;
  serializeJson(doc, output);
  return output;
}
