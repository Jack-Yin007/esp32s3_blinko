/*
 * LED Driver Implementation
 */

#include "drivers/led_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "LED_DRIVER";

typedef struct {
    bool initialized;
    gpio_num_t pin;
    bool state;
} led_driver_t;

static led_driver_t g_led_driver = {0};

esp_err_t led_driver_init(gpio_num_t pin) {
    if (g_led_driver.initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing LED driver on pin %d", pin);
    
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED pin");
        return ret;
    }
    
    g_led_driver.pin = pin;
    g_led_driver.state = false;
    g_led_driver.initialized = true;
    
    led_driver_set_state(false);
    
    ESP_LOGI(TAG, "LED driver initialized successfully");
    return ESP_OK;
}

esp_err_t led_driver_set_state(bool on) {
    if (!g_led_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = gpio_set_level(g_led_driver.pin, on ? 1 : 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED state");
        return ret;
    }
    
    g_led_driver.state = on;
    return ESP_OK;
}

bool led_driver_get_state(void) {
    return g_led_driver.state;
}

bool led_driver_is_initialized(void) {
    return g_led_driver.initialized;
}
