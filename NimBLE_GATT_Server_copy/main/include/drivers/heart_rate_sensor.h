/*
 * Heart Rate Sensor Driver Header
 * 心率传感器驱动头文件
 * 
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef HEART_RATE_SENSOR_H
#define HEART_RATE_SENSOR_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Heart Rate Sensor Configuration */
#define HR_SENSOR_MIN_BPM           60    // 最小心率值
#define HR_SENSOR_MAX_BPM           200   // 最大心率值
#define HR_SENSOR_DEFAULT_BPM       75    // 默认心率值

/* Heart Rate Sensor Data Callback Type */
typedef void (*heart_rate_callback_t)(uint16_t bpm);

/* Heart Rate Sensor Driver Functions */

/**
 * @brief Initialize heart rate sensor driver
 * @param simulation_mode Enable simulation mode for testing
 * @return ESP_OK on success, error code on failure
 */
esp_err_t heart_rate_driver_init(bool simulation_mode);

/**
 * @brief Deinitialize heart rate sensor driver
 * @return ESP_OK on success, error code on failure
 */
esp_err_t heart_rate_driver_deinit(void);

/**
 * @brief Start heart rate measurement
 * @return ESP_OK on success, error code on failure
 */
esp_err_t heart_rate_driver_start_measurement(void);

/**
 * @brief Stop heart rate measurement
 * @return ESP_OK on success, error code on failure
 */
esp_err_t heart_rate_driver_stop_measurement(void);

/**
 * @brief Get current heart rate (BPM)
 * @return Current heart rate in beats per minute, 0 if not initialized
 */
uint16_t heart_rate_driver_get_bpm(void);

/**
 * @brief Set callback function for heart rate updates
 * @param callback Callback function to be called when heart rate updates
 * @return ESP_OK on success, error code on failure
 */
esp_err_t heart_rate_driver_set_callback(heart_rate_callback_t callback);

/**
 * @brief Update simulation data (for testing)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t heart_rate_driver_update_simulation(void);

/**
 * @brief Check if heart rate sensor is initialized
 * @return true if initialized, false otherwise
 */
bool heart_rate_driver_is_initialized(void);

/**
 * @brief Check if measurement is active
 * @return true if measuring, false otherwise
 */
bool heart_rate_driver_is_measuring(void);

#ifdef __cplusplus
}
#endif

#endif // HEART_RATE_SENSOR_H