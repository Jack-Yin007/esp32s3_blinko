/*
 * Pet Robot Main Application - Simplified Version
 * 宠物机器人主应用程序 - 简化版本
 * 
 * 这是一个简化的主程序入口，展示模块化架构的基本结构
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 条件包含新架构模块
#include "app/pet_config.h"

#define TAG "PET_ROBOT_MAIN"

// 编译时配置开关
#ifndef USE_NEW_ARCHITECTURE
#define USE_NEW_ARCHITECTURE 0
#endif

/*=============================================================================
 * 简化的初始化函数声明
 *=============================================================================*/
static esp_err_t simple_nvs_init(void);

/*=============================================================================
 * ESP-IDF应用程序入口点
 *=============================================================================*/

void app_main(void)
{
#if USE_NEW_ARCHITECTURE
    ESP_LOGI(TAG, "=== Pet Robot System Starting (New Architecture) ===");
    ESP_LOGI(TAG, "Version: 1.0.0");
    
    // 基础系统初始化
    esp_err_t ret = simple_nvs_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }
    
    // 初始化配置系统
    ret = pet_config_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize configuration: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "=== Pet Robot System Started Successfully ===");
    ESP_LOGI(TAG, "New modular architecture is running");
    ESP_LOGI(TAG, "Ready to add HAL, middleware, and other modules");
    
    // 系统就绪，进入主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "System heartbeat - Free heap: %d bytes", esp_get_free_heap_size());
    }
#else
    ESP_LOGI(TAG, "New architecture disabled");
    ESP_LOGI(TAG, "Please use main.c for legacy functionality");
    ESP_LOGI(TAG, "To enable new architecture, set USE_NEW_ARCHITECTURE=1");
#endif
}

/*=============================================================================
 * 简化的辅助函数实现
 *=============================================================================*/

static esp_err_t simple_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "NVS Flash initialized successfully");
    }
    
    return ret;
}