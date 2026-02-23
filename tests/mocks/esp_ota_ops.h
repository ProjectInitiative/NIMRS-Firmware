#ifndef ESP_OTA_OPS_H
#define ESP_OTA_OPS_H

#include "esp_err.h"
#include "esp_partition.h"

// Simplified esp_app_desc_t for mock
typedef struct {
  uint32_t magic_word;
  uint32_t secure_version;
  uint32_t reserv1[2];
  char version[32];
  char project_name[32];
  char time[16];
  char date[16];
  char idf_ver[32];
  uint8_t app_elf_sha256[32];
  uint32_t reserv2[20];
} esp_app_desc_t;

// OTA functions
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *
esp_ota_get_next_update_partition(const esp_partition_t *start_from);
const esp_partition_t *esp_ota_get_last_invalid_partition(void);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);

// New functions for version info
const esp_app_desc_t *esp_ota_get_app_description(void);
esp_err_t esp_ota_get_partition_description(const esp_partition_t *partition,
                                            esp_app_desc_t *app_desc);

// Helper to control mock state from tests
void setMockRunningPartition(const esp_partition_t *p);
const esp_partition_t *getMockBootPartition();

// Exposed partition instances for tests
extern esp_partition_t mock_ota_0;
extern esp_partition_t mock_ota_1;

#endif
