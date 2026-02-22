#ifndef ESP_PARTITION_H
#define ESP_PARTITION_H

#include <stdint.h>
#include <string>

typedef enum {
  ESP_PARTITION_TYPE_APP,
  ESP_PARTITION_TYPE_DATA,
} esp_partition_type_t;

typedef enum {
  ESP_PARTITION_SUBTYPE_APP_OTA_0,
  ESP_PARTITION_SUBTYPE_APP_OTA_1,
  ESP_PARTITION_SUBTYPE_DATA_NVS,
  ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
} esp_partition_subtype_t;

typedef struct {
  esp_partition_type_t type;
  esp_partition_subtype_t subtype;
  uint32_t address;
  uint32_t size;
  const char *label;
  bool encrypted;
} esp_partition_t;

#endif
