/*
 * Pet Robot Configuration Implementation
 * 宠物机器人配置实现
 */

#include "pet_config.h"
#include "esp_log.h"
#include <string.h>

/*=============================================================================
 * 私有变量 (Private Variables)
 *=============================================================================*/

static const char* TAG = "PET_CONFIG";

// 默认配置
static const pet_config_t s_default_config = {
    .gpio = {
        .touch_pin = PIN_TOUCH_SENSOR,
        .pir_sensor_pin = PIN_MOTION_SENSOR,
        .led_pin = PIN_LED_STATUS,
        .buzzer_pin = 18
    }
};

// 当前配置
static pet_config_t s_current_config;
static bool s_config_initialized = false;

/*=============================================================================
 * 公共函数实现 (Public Function Implementation)
 *=============================================================================*/

esp_err_t pet_config_init(void)
{
    if (s_config_initialized) {
        ESP_LOGW(TAG, "Pet configuration already initialized");
        return ESP_OK;
    }

    // 复制默认配置
    memcpy(&s_current_config, &s_default_config, sizeof(pet_config_t));
    
    s_config_initialized = true;
    ESP_LOGI(TAG, "Pet configuration initialized successfully");
    
    return ESP_OK;
}

const pet_config_t* pet_config_get(void)
{
    if (!s_config_initialized) {
        ESP_LOGW(TAG, "Pet configuration not initialized, returning default config");
        return &s_default_config;
    }
    
    return &s_current_config;
}