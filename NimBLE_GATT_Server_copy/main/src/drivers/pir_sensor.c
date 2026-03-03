/**
 * @file pir_sensor.c
 * @brief PIR人体感应传感器驱动实现
 * 
 * 硬件连接：
 * - PIR传感器输出 -> ESP32-S3 GPIO6
 * 
 * 工作原理：
 * - 被动红外人体感应传感器
 * - 检测到人体移动时GPIO输出高电平
 * - 未检测到人体时GPIO输出低电平
 * - 典型感应距离：3-7米
 * - 感应角度：约120度
 * 
 * 注意事项：
 * - GPIO需要配置为输入模式
 * - 可选：配置内部下拉电阻确保未检测时稳定低电平
 * - PIR传感器上电后需要预热时间（约30-60秒）
 */

#include "drivers/hardware_interface.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "PIRSensor";

/* GPIO定义 */
#define PIR_GPIO_NUM            GPIO_NUM_6  // PIR传感器输出引脚

/* 初始化状态 */
static bool g_pir_initialized = false;

/**
 * @brief 初始化PIR人体感应传感器GPIO
 */
esp_err_t hw_pir_sensor_init(void)
{
    if (g_pir_initialized) {
        ESP_LOGW(TAG, "PIR sensor already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing PIR motion sensor...");
    
    // 配置GPIO6为输入模式，禁用上拉/下拉（PIR传感器为推挽输出）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIR_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,     // 禁用上拉（避免干扰PIR输出）
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", 
                 PIR_GPIO_NUM, esp_err_to_name(ret));
        return ret;
    }
    
    g_pir_initialized = true;
    
    ESP_LOGI(TAG, "PIR motion sensor initialized:");
    ESP_LOGI(TAG, "  GPIO%d: PIR output (High=Motion detected)", PIR_GPIO_NUM);
    ESP_LOGI(TAG, "  Input mode: No pull-up/down (push-pull output)");
    ESP_LOGI(TAG, "  Expected voltage: 0V idle, 3.3V triggered");
    ESP_LOGI(TAG, "  Note: PIR sensor needs 30-60s warm-up time");
    
    return ESP_OK;
}

/**
 * @brief 检测人体感应状态
 * @return true 检测到人体移动（GPIO高电平），false 未检测到（GPIO低电平）
 */
bool hw_pir_sensor_is_detected(void)
{
    if (!g_pir_initialized) {
        ESP_LOGW(TAG, "PIR sensor not initialized");
        return false;
    }
    
    // 读取GPIO6状态：高电平=检测到人体，低电平=未检测到
    int level = gpio_get_level(PIR_GPIO_NUM);
    return (level == 1);
}

/**
 * @brief 获取PIR传感器原始GPIO电平
 * @return 0=低电平（未检测到），1=高电平（检测到人体）
 * 
 * 用于调试或需要原始信号的场合
 */
int hw_pir_sensor_get_raw_level(void)
{
    if (!g_pir_initialized) {
        ESP_LOGW(TAG, "PIR sensor not initialized");
        return 0;
    }
    
    return gpio_get_level(PIR_GPIO_NUM);
}
