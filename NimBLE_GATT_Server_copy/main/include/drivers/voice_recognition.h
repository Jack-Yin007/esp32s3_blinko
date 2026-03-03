/**
 * @file voice_recognition.h
 * @brief Voice Recognition GPIO Interrupt Driver for ESP32-S3
 * 
 * This driver handles voice wakeup detection through GPIO7 hardware interrupt.
 * When the external voice recognition module detects keywords like "Blinko",
 * GPIO7 goes LOW (active low signal).
 * 
 * ⚠️ Hardware Change History:
 * - OLD: UART0 (GPIO43 TX, GPIO44 RX) with protocol parsing
 * - DEPRECATED: AW9535 P1.7 GPIO interrupt (rising edge trigger)
 * - V1: GPIO44 (U0RXD) polling mode (100ms interval, can miss <100ms pulses)
 * - V2: GPIO44 (U0RXD) interrupt mode (falling edge, <1ms response)
 * - CURRENT: GPIO7 interrupt mode (falling edge, <1ms response) - 2025-12-24 硬件变更
 * 
 * Hardware Configuration:
 * - Interface: Direct GPIO7
 * - Detection: Low level (0V) = keyword detected
 * - Mode: Hardware interrupt (falling edge trigger)
 * - Pull-up: Enabled (internal pull-up resistor)
 * - Debounce: 500ms software debounce
 * 
 * Benefits:
 * - Instant response (<1ms vs 0-100ms polling delay)
 * - No missed triggers (detects pulses as short as 1ms)
 * - Lower CPU usage (no continuous polling)
 * - Lower power consumption
 * - Simple hardware interface (no I2C expander needed)
 */

#ifndef VOICE_RECOGNITION_H
#define VOICE_RECOGNITION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize voice sensor driver (GPIO7 interrupt mode)
 * 
 * Configures GPIO7 as input with falling edge interrupt.
 * The voice module outputs LOW when keyword "Blinko" is detected.
 * 
 * This function:
 * - Configures GPIO7 with internal pull-up
 * - Installs GPIO ISR service
 * - Creates voice handling task
 * - Enables falling edge interrupt
 * - Implements 500ms software debounce
 * 
 * This function should be called once during system initialization.
 * 
 * @return ESP_OK if initialization successful, ESP_FAIL otherwise
 */
esp_err_t hw_aduio_sensor_init(void);

/**
 * @brief Check if voice keyword is currently detected (direct GPIO read)
 * 
 * Reads GPIO7 level to determine if voice keyword is currently detected.
 * Returns true when GPIO7 is LOW (keyword detected), false otherwise.
 * 
 * Note: With interrupt mode, this function is mainly for debugging.
 * Voice triggers are automatically handled by the interrupt system.
 * 
 * @return true if voice keyword detected (GPIO7 = LOW), false otherwise
 */
bool hw_voice_sensor_is_detected(void);

#ifdef __cplusplus
}
#endif

#endif // VOICE_RECOGNITION_H
