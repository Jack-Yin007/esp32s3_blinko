/*
 * BLE OTA (Over-The-Air) Update Module
 * 
 * This module implements BLE-based firmware update functionality for ESP32-S3.
 * Compatible with host/ble_ota_tester.py testing tool.
 */

#ifndef BLE_OTA_H
#define BLE_OTA_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* OTA Command Definitions (matching ble_ota_tester.py protocol) */
#define BLE_OTA_CMD_START       0x01  // Start OTA upgrade
#define BLE_OTA_CMD_STOP        0x02  // Stop OTA upgrade  
#define BLE_OTA_CMD_ACK         0x03  // Acknowledgment
#define BLE_OTA_CMD_NACK        0x04  // Negative acknowledgment

/* OTA Status Codes */
typedef enum {
    BLE_OTA_STATUS_IDLE = 0,        // Ready for new OTA
    BLE_OTA_STATUS_RECEIVING,       // Receiving firmware data
    BLE_OTA_STATUS_COMPLETE,        // OTA completed successfully
    BLE_OTA_STATUS_ERROR,           // OTA failed
    BLE_OTA_STATUS_VERIFYING        // Verifying firmware
} ble_ota_status_t;

/* OTA Error Codes */
typedef enum {
    BLE_OTA_ERR_NONE = 0,
    BLE_OTA_ERR_INVALID_CMD,
    BLE_OTA_ERR_NO_SPACE,
    BLE_OTA_ERR_WRITE_FAIL,
    BLE_OTA_ERR_VERIFY_FAIL,
    BLE_OTA_ERR_ALREADY_RUNNING
} ble_ota_error_t;

/* OTA Progress Information */
typedef struct {
    uint32_t total_size;           // Total firmware size (bytes)
    uint32_t received_size;        // Received size (bytes)
    uint8_t  percentage;           // Progress percentage (0-100)
    ble_ota_status_t status;       // Current status
} ble_ota_progress_t;

/* OTA Callback Function Types */
typedef void (*ble_ota_progress_cb_t)(ble_ota_progress_t *progress);
typedef void (*ble_ota_complete_cb_t)(bool success, ble_ota_error_t error);

/**
 * @brief Initialize BLE OTA module
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_ota_init(void);

/**
 * @brief Start OTA upgrade
 * @param firmware_size Total firmware size in bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_ota_start(uint32_t firmware_size);

/**
 * @brief Write firmware data
 * @param data Data buffer
 * @param len Data length
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_ota_write_data(const uint8_t *data, size_t len);

/**
 * @brief End OTA upgrade and set boot partition
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_ota_end(void);

/**
 * @brief Abort OTA upgrade
 */
void ble_ota_abort(void);

/**
 * @brief Get current OTA progress
 * @param progress Output progress information
 */
void ble_ota_get_progress(ble_ota_progress_t *progress);

/**
 * @brief Get OTA status
 * @return Current OTA status
 */
ble_ota_status_t ble_ota_get_status(void);

/**
 * @brief Register progress callback
 * @param cb Callback function
 */
void ble_ota_register_progress_callback(ble_ota_progress_cb_t cb);

/**
 * @brief Register completion callback
 * @param cb Callback function
 */
void ble_ota_register_complete_callback(ble_ota_complete_cb_t cb);

/**
 * @brief Get total firmware length
 * @return Firmware size in bytes
 */
uint32_t ble_ota_get_fw_length(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_OTA_H
