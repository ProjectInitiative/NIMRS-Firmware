#ifndef ESP_OTA_OPS_H
#define ESP_OTA_OPS_H

#include "esp_err.h"
#include "esp_partition.h"

// OTA functions
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *
esp_ota_get_next_update_partition(const esp_partition_t *start_from);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);

// Helper to control mock state from tests
void setMockRunningPartition(const esp_partition_t *p);
const esp_partition_t *getMockBootPartition();

// Exposed partition instances for tests
extern esp_partition_t mock_ota_0;
extern esp_partition_t mock_ota_1;

#endif
