/*
 * BLE OTA Implementation
 * 
 * Handles firmware upgrade via BLE using ESP-IDF OTA APIs.
 */

#include "ble/ble_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BLE_OTA";

/* OTA Context Structure */
typedef struct {
    esp_ota_handle_t ota_handle;               // OTA handle
    const esp_partition_t *update_partition;   // Update partition pointer
    uint32_t firmware_size;                    // Total firmware size
    uint32_t received_size;                    // Received size
    ble_ota_status_t status;                   // Current status
    ble_ota_error_t last_error;                // Last error code
    ble_ota_progress_cb_t progress_cb;         // Progress callback
    ble_ota_complete_cb_t complete_cb;         // Completion callback
    bool initialized;                           // Initialization flag
    uint8_t last_reported_percent;             // Last reported percentage
} ble_ota_context_t;

static ble_ota_context_t s_ota_ctx = {0};

/* Internal: Notify progress */
static void notify_progress(void)
{
    if (s_ota_ctx.progress_cb && s_ota_ctx.firmware_size > 0) {
        ble_ota_progress_t progress = {
            .total_size = s_ota_ctx.firmware_size,
            .received_size = s_ota_ctx.received_size,
            .percentage = (uint8_t)((s_ota_ctx.received_size * 100) / s_ota_ctx.firmware_size),
            .status = s_ota_ctx.status
        };
        s_ota_ctx.progress_cb(&progress);
    }
}

/* Internal: Notify completion */
static void notify_complete(bool success, ble_ota_error_t error)
{
    if (s_ota_ctx.complete_cb) {
        s_ota_ctx.complete_cb(success, error);
    }
}

esp_err_t ble_ota_init(void)
{
    if (s_ota_ctx.initialized) {
        ESP_LOGW(TAG, "BLE OTA already initialized");
        return ESP_OK;
    }

    memset(&s_ota_ctx, 0, sizeof(s_ota_ctx));
    s_ota_ctx.status = BLE_OTA_STATUS_IDLE;
    s_ota_ctx.initialized = true;

    ESP_LOGI(TAG, "BLE OTA module initialized");
    return ESP_OK;
}

esp_err_t ble_ota_start(uint32_t firmware_size)
{
    if (!s_ota_ctx.initialized) {
        ESP_LOGE(TAG, "BLE OTA not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ota_ctx.status == BLE_OTA_STATUS_RECEIVING) {
        ESP_LOGW(TAG, "OTA already in progress - aborting previous session");
        // Abort previous session
        if (s_ota_ctx.ota_handle != 0) {
            esp_ota_abort(s_ota_ctx.ota_handle);
            s_ota_ctx.ota_handle = 0;
        }
        s_ota_ctx.status = BLE_OTA_STATUS_IDLE;
        s_ota_ctx.received_size = 0;
        s_ota_ctx.last_reported_percent = 0;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Starting BLE OTA upgrade");
    ESP_LOGI(TAG, "Firmware size: %lu bytes (%.2f KB)", 
             firmware_size, firmware_size / 1024.0);
    ESP_LOGI(TAG, "========================================");

    // Get next OTA partition
    s_ota_ctx.update_partition = esp_ota_get_next_update_partition(NULL);
    if (s_ota_ctx.update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get OTA partition");
        s_ota_ctx.last_error = BLE_OTA_ERR_NO_SPACE;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA partition: %s", s_ota_ctx.update_partition->label);
    ESP_LOGI(TAG, "  Subtype: %d", s_ota_ctx.update_partition->subtype);
    ESP_LOGI(TAG, "  Offset: 0x%lX", s_ota_ctx.update_partition->address);
    ESP_LOGI(TAG, "  Size: %lu KB", s_ota_ctx.update_partition->size / 1024);

    // Check if partition has enough space
    if (firmware_size > s_ota_ctx.update_partition->size) {
        ESP_LOGE(TAG, "Firmware too large: %lu > %lu", 
                 firmware_size, s_ota_ctx.update_partition->size);
        s_ota_ctx.last_error = BLE_OTA_ERR_NO_SPACE;
        return ESP_ERR_NO_MEM;
    }

    // Begin OTA
    esp_err_t err = esp_ota_begin(s_ota_ctx.update_partition, 
                                   firmware_size, 
                                   &s_ota_ctx.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        s_ota_ctx.last_error = BLE_OTA_ERR_WRITE_FAIL;
        return err;
    }

    s_ota_ctx.firmware_size = firmware_size;
    s_ota_ctx.received_size = 0;
    s_ota_ctx.status = BLE_OTA_STATUS_RECEIVING;
    s_ota_ctx.last_error = BLE_OTA_ERR_NONE;
    s_ota_ctx.last_reported_percent = 0;

    ESP_LOGI(TAG, "✓ OTA started successfully");
    notify_progress();

    return ESP_OK;
}

esp_err_t ble_ota_write_data(const uint8_t *data, size_t len)
{
    if (!s_ota_ctx.initialized || s_ota_ctx.status != BLE_OTA_STATUS_RECEIVING) {
        ESP_LOGE(TAG, "OTA not started");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid data");
        return ESP_ERR_INVALID_ARG;
    }

    // Check for overflow
    if (s_ota_ctx.received_size + len > s_ota_ctx.firmware_size) {
        ESP_LOGE(TAG, "Data overflow: received=%lu + len=%u > total=%lu",
                 s_ota_ctx.received_size, len, s_ota_ctx.firmware_size);
        s_ota_ctx.last_error = BLE_OTA_ERR_WRITE_FAIL;
        return ESP_ERR_INVALID_SIZE;
    }

    // Write data to OTA partition
    esp_err_t err = esp_ota_write(s_ota_ctx.ota_handle, data, len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        s_ota_ctx.last_error = BLE_OTA_ERR_WRITE_FAIL;
        return err;
    }

    s_ota_ctx.received_size += len;

    // Report progress every 5%
    uint8_t current_percent = (s_ota_ctx.received_size * 100) / s_ota_ctx.firmware_size;
    if (current_percent >= s_ota_ctx.last_reported_percent + 5 || 
        s_ota_ctx.received_size == s_ota_ctx.firmware_size) {
        ESP_LOGI(TAG, "Progress: %u%% (%lu/%lu bytes)",
                 current_percent, s_ota_ctx.received_size, s_ota_ctx.firmware_size);
        s_ota_ctx.last_reported_percent = current_percent;
        notify_progress();
    }

    return ESP_OK;
}

esp_err_t ble_ota_end(void)
{
    if (!s_ota_ctx.initialized || s_ota_ctx.status != BLE_OTA_STATUS_RECEIVING) {
        ESP_LOGE(TAG, "OTA not in progress");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Finalizing BLE OTA upgrade");
    ESP_LOGI(TAG, "Received: %lu / %lu bytes", 
             s_ota_ctx.received_size, s_ota_ctx.firmware_size);

    // Check if firmware is complete
    if (s_ota_ctx.received_size != s_ota_ctx.firmware_size) {
        ESP_LOGE(TAG, "Incomplete firmware: %lu != %lu",
                 s_ota_ctx.received_size, s_ota_ctx.firmware_size);
        s_ota_ctx.last_error = BLE_OTA_ERR_WRITE_FAIL;
        s_ota_ctx.status = BLE_OTA_STATUS_ERROR;
        notify_complete(false, BLE_OTA_ERR_WRITE_FAIL);
        return ESP_FAIL;
    }

    // End OTA write
    esp_err_t err = esp_ota_end(s_ota_ctx.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        s_ota_ctx.last_error = BLE_OTA_ERR_VERIFY_FAIL;
        s_ota_ctx.status = BLE_OTA_STATUS_ERROR;
        notify_complete(false, BLE_OTA_ERR_VERIFY_FAIL);
        return err;
    }

    // Set boot partition
    err = esp_ota_set_boot_partition(s_ota_ctx.update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        s_ota_ctx.last_error = BLE_OTA_ERR_VERIFY_FAIL;
        s_ota_ctx.status = BLE_OTA_STATUS_ERROR;
        notify_complete(false, BLE_OTA_ERR_VERIFY_FAIL);
        return err;
    }

    s_ota_ctx.status = BLE_OTA_STATUS_COMPLETE;
    ESP_LOGI(TAG, "✓ BLE OTA upgrade completed successfully!");
    ESP_LOGI(TAG, "New boot partition: %s", s_ota_ctx.update_partition->label);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Device will restart in 3 seconds...");
    ESP_LOGI(TAG, "========================================");

    notify_complete(true, BLE_OTA_ERR_NONE);

    // Restart after delay
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();

    return ESP_OK;
}

void ble_ota_abort(void)
{
    if (s_ota_ctx.status == BLE_OTA_STATUS_RECEIVING) {
        ESP_LOGW(TAG, "Aborting BLE OTA upgrade");
        esp_ota_abort(s_ota_ctx.ota_handle);
    }
    
    s_ota_ctx.status = BLE_OTA_STATUS_IDLE;
    s_ota_ctx.received_size = 0;
    s_ota_ctx.firmware_size = 0;
    s_ota_ctx.last_reported_percent = 0;
}

void ble_ota_get_progress(ble_ota_progress_t *progress)
{
    if (progress == NULL) return;

    progress->total_size = s_ota_ctx.firmware_size;
    progress->received_size = s_ota_ctx.received_size;
    progress->percentage = (s_ota_ctx.firmware_size > 0) ?
                          (uint8_t)((s_ota_ctx.received_size * 100) / s_ota_ctx.firmware_size) : 0;
    progress->status = s_ota_ctx.status;
}

ble_ota_status_t ble_ota_get_status(void)
{
    return s_ota_ctx.status;
}

void ble_ota_register_progress_callback(ble_ota_progress_cb_t cb)
{
    s_ota_ctx.progress_cb = cb;
}

void ble_ota_register_complete_callback(ble_ota_complete_cb_t cb)
{
    s_ota_ctx.complete_cb = cb;
}

uint32_t ble_ota_get_fw_length(void)
{
    return s_ota_ctx.firmware_size;
}
