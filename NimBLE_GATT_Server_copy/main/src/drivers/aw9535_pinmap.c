/**
 * @file aw9535_pinmap.c
 * @brief AW9535 GPIO扩展器引脚映射实现
 */

#include "drivers/aw9535_pinmap.h"
#include "drivers/aw9535.h"
#include "esp_log.h"

static const char *TAG = "AW9535_PINMAP";

/**
 * @brief 初始化所有AW9535控制引脚
 */
esp_err_t aw9535_pinmap_init(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Initializing AW9535 pin mappings...");
    
    // ==================== Port 0 配置 ====================
    
    // P00: 传感器电源 (输出)
    ret = aw9535_set_mode(AW9535_PIN_SENSOR_POWER, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P00 (SENSOR_POWER)");
        return ret;
    }
    aw9535_set_level(AW9535_PIN_SENSOR_POWER, AW9535_LEVEL_LOW); // 默认关闭
    
    // P01: 马达1中断 (输入)
    ret = aw9535_set_mode(AW9535_PIN_MOTOR1_INT, AW9535_MODE_INPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P01 (MOTOR1_INT)");
        return ret;
    }
    
    // P02-P04: RGB LED (输出) - 由 led_enhanced.c 初始化
    ESP_LOGI(TAG, "  P02-P04: RGB LED (configured by led_enhanced.c)");
    
    // P05: 充电使能 (输出)
    ret = aw9535_set_mode(AW9535_PIN_CHARGE_CE, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P05 (CHARGE_CE)");
        return ret;
    }
    aw9535_set_level(AW9535_PIN_CHARGE_CE, AW9535_LEVEL_HIGH); // 默认使能充电
    
    // P06: 充电中断 (输入)
    ret = aw9535_set_mode(AW9535_PIN_CHARGE_INT, AW9535_MODE_INPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P06 (CHARGE_INT)");
        return ret;
    }
    
    // P07: BUCK使能 (输出)
    ret = aw9535_set_mode(AW9535_PIN_BUCK_EN, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P07 (BUCK_EN)");
        return ret;
    }
    aw9535_set_level(AW9535_PIN_BUCK_EN, AW9535_LEVEL_HIGH); // 默认使能BUCK
    
    // ==================== Port 1 配置 ====================
    
    // P10-P12: 马达2中断 (输入)
    ret = aw9535_set_mode(AW9535_PIN_MOTOR2_INT1, AW9535_MODE_INPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P12 (MOTOR2_INT1)");
        return ret;
    }
    
    ret = aw9535_set_mode(AW9535_PIN_MOTOR2_INT2, AW9535_MODE_INPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P11 (MOTOR2_INT2)");
        return ret;
    }
    
    ret = aw9535_set_mode(AW9535_PIN_MOTOR2_INT3, AW9535_MODE_INPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P10 (MOTOR2_INT3)");
        return ret;
    }
    
    // P17: 天问使能 (输出)
    ret = aw9535_set_mode(AW9535_PIN_TW_EN, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P17 (TW_EN)");
        return ret;
    }
    aw9535_set_level(AW9535_PIN_TW_EN, AW9535_LEVEL_HIGH); // 默认使能天问
    
    // P16: 马达使能 (输出)
    ret = aw9535_set_mode(AW9535_PIN_MOTOR_EN, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure P16 (MOTOR_EN)");
        return ret;
    }
    aw9535_set_level(AW9535_PIN_MOTOR_EN, AW9535_LEVEL_LOW); // 默认禁用马达
    
    // P13-P15: MAX98357A控制 - 由 max98357a.c 初始化
    ESP_LOGI(TAG, "  P13-P15: MAX98357A (configured by max98357a.c)");
    
    ESP_LOGI(TAG, "✓ AW9535 pin mappings initialized successfully");
    ESP_LOGI(TAG, "  Port 0: SENSOR_PWR, MOTOR1_INT, RGB_LED, CHARGE_CE/INT, BUCK_EN");
    ESP_LOGI(TAG, "  Port 1: MOTOR2_INTs, MOTOR_EN, TW_EN, MAX98357A_CTRL");
    
    return ESP_OK;
}
