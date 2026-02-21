#ifndef LOGGER_MOCK_H
#define LOGGER_MOCK_H

#include "Arduino.h"

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DATA };

class Logger : public Print {
public:
    static Logger &getInstance();
    Logger();
    virtual ~Logger();
    void begin(unsigned long baud) {}
    void setLevel(LogLevel level) {}
    size_t printf(const char *format, ...) override;
    void debug(const char *format, ...);
    virtual size_t write(uint8_t c) override;
    String getLogsJSON(const String &filter = "");
};

#define Log Logger::getInstance()

#endif
