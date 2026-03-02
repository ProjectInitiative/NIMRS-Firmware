#ifndef ESP_OTA_OPS_H
#define ESP_OTA_OPS_H

#include "esp_app_format.h"
#include "esp_err.h"
#include "esp_partition.h"

// OTA functions
typedef enum {
  ESP_OTA_IMG_NEW,
  ESP_OTA_IMG_PENDING_VERIFY,
  ESP_OTA_IMG_VALID,
  ESP_OTA_IMG_INVALID,
  ESP_OTA_IMG_ABORTED,
  ESP_OTA_IMG_UNDEFINED = -1,
} esp_ota_img_states_t;

const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *
esp_ota_get_next_update_partition(const esp_partition_t *start_from);
const esp_partition_t *esp_ota_get_last_invalid_partition(void);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition);
const esp_partition_t *esp_ota_get_boot_partition(void);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition,
                                      esp_ota_img_states_t *ota_state);

// New functions for version info
const esp_app_desc_t *esp_ota_get_app_description(void);
esp_err_t esp_ota_get_partition_description(const esp_partition_t *partition,
                                            esp_app_desc_t *app_desc);

esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void);

// Helper to control mock state from tests
void setMockRunningPartition(const esp_partition_t *p);
const esp_partition_t *getMockBootPartition();

// Exposed partition instances for tests
extern esp_partition_t mock_ota_0;
extern esp_partition_t mock_ota_1;

#endif
