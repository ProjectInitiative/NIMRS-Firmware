#pragma once

#include "Arduino.h"

inline void esp_restart() { ESP.restartCalled = true; }

#define ESP_ERR_OTA_VALIDATE_FAILED 0x104
