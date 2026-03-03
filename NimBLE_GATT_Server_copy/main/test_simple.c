/*
 * Simple Test for 3-Layer Architecture
 * 3层架构简单测试
 * 
 * 测试应用层 → 驱动层 → 系统层的基本功能
 */

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drivers/led_driver.h"
#include "drivers/heart_rate_sensor.h"

static const char* TAG = "TEST_3LAYER";

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Pet Robot - 3层架构测试");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  应用层 → 驱动层 → 系统层");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize LED driver
    esp_err_t ret = led_driver_init(GPIO_NUM_2);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LED driver initialized successfully");
        
        // Test LED functionality
        led_driver_set_state(true);
        vTaskDelay(pdMS_TO_TICKS(500));
        led_driver_set_state(false);
        ESP_LOGI(TAG, "LED test completed");
    } else {
        ESP_LOGE(TAG, "LED driver initialization failed");
    }
    
    // Initialize heart rate sensor
    ret = heart_rate_driver_init(true); // simulation mode
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Heart rate sensor initialized successfully");
        
        // Test heart rate functionality
        heart_rate_driver_start_measurement();
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint16_t bpm = heart_rate_driver_get_bpm();
        ESP_LOGI(TAG, "Current heart rate: %d BPM", bpm);
        heart_rate_driver_stop_measurement();
    } else {
        ESP_LOGE(TAG, "Heart rate sensor initialization failed");
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  3层架构测试完成!");
    ESP_LOGI(TAG, "========================================");
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}