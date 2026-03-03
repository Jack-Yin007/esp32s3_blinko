/*
 * LED Driver Header
 * LED驱动头文件
 * 
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include "esp_err.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LED Driver Functions */

/**
 * @brief Initialize LED driver
 * @param pin GPIO pin number for LED
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_driver_init(gpio_num_t pin);

/**
 * @brief Deinitialize LED driver
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_driver_deinit(void);

/**
 * @brief Set LED state
 * @param on true to turn on LED, false to turn off
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_driver_set_state(bool on);

/**
 * @brief Toggle LED state
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_driver_toggle(void);

/**
 * @brief Get current LED state
 * @return true if LED is on, false if off
 */
bool led_driver_get_state(void);

/**
 * @brief Blink LED for specified duration
 * @param duration_ms Duration to keep LED on in milliseconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t led_driver_blink(uint32_t duration_ms);

/**
 * @brief Check if LED driver is initialized
 * @return true if initialized, false otherwise
 */
bool led_driver_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // LED_DRIVER_H