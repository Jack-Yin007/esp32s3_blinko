/**
 * @file aw9535_example.c
 * @brief AW9535 GPIO扩展器使用示例
 * 
 * 本文件演示如何使用AW9535驱动程序
 */

#include "drivers/aw9535.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AW9535_EXAMPLE";

/**
 * @brief GPIO中断回调函数示例
 */
static void button_isr_handler(aw9535_pin_t pin, aw9535_level_t level, void *user_data)
{
    ESP_LOGI(TAG, "按钮中断: Pin=%d, Level=%d", pin, level);
}

/**
 * @brief 示例1: 基本GPIO输出控制
 */
void example_gpio_output(void)
{
    ESP_LOGI(TAG, "=== 示例1: GPIO输出控制 ===");
    
    // 配置P0_0为输出模式
    aw9535_set_mode(AW9535_PIN_P0_0, AW9535_MODE_OUTPUT);
    
    // 控制LED闪烁
    for (int i = 0; i < 10; i++) {
        aw9535_set_level(AW9535_PIN_P0_0, AW9535_LEVEL_HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        aw9535_set_level(AW9535_PIN_P0_0, AW9535_LEVEL_LOW);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief 示例2: GPIO输入读取
 */
void example_gpio_input(void)
{
    ESP_LOGI(TAG, "=== 示例2: GPIO输入读取 ===");
    
    // 配置P0_1为输入模式
    aw9535_set_mode(AW9535_PIN_P0_1, AW9535_MODE_INPUT);
    
    // 持续读取按钮状态
    for (int i = 0; i < 20; i++) {
        aw9535_level_t level;
        if (aw9535_get_level(AW9535_PIN_P0_1, &level) == ESP_OK) {
            ESP_LOGI(TAG, "按钮状态: %s", level ? "按下" : "释放");
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * @brief 示例3: 批量GPIO控制
 */
void example_port_control(void)
{
    ESP_LOGI(TAG, "=== 示例3: 批量GPIO控制 ===");
    
    // 配置端口0所有引脚为输出 (0x00 = 全输出)
    aw9535_set_mode_port0(0x00);
    
    // 流水灯效果
    for (int i = 0; i < 8; i++) {
        uint8_t pattern = (1 << i);
        aw9535_set_level_port0(pattern);
        ESP_LOGI(TAG, "流水灯模式: 0x%02X", pattern);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // 全部点亮
    aw9535_set_level_port0(0xFF);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 全部熄灭
    aw9535_set_level_port0(0x00);
}

/**
 * @brief 示例4: GPIO中断使用
 */
void example_gpio_interrupt(void)
{
    ESP_LOGI(TAG, "=== 示例4: GPIO中断 ===");
    
    // 配置P1_0为输入模式
    aw9535_set_mode(AW9535_PIN_P1_0, AW9535_MODE_INPUT);
    
    // 配置下降沿中断（按钮按下）
    aw9535_set_interrupt(AW9535_PIN_P1_0, AW9535_INT_FALLING, 
                         button_isr_handler, NULL);
    
    // 启用中断
    aw9535_enable_interrupt(AW9535_PIN_P1_0);
    
    ESP_LOGI(TAG, "中断已配置，请按下按钮...");
    
    // 等待中断
    vTaskDelay(pdMS_TO_TICKS(30000));  // 等待30秒
    
    // 禁用中断
    aw9535_disable_interrupt(AW9535_PIN_P1_0);
}

/**
 * @brief 示例5: 多引脚混合使用
 */
void example_mixed_usage(void)
{
    ESP_LOGI(TAG, "=== 示例5: 多引脚混合使用 ===");
    
    // 配置多个输出引脚 (LED)
    aw9535_set_mode(AW9535_PIN_P0_0, AW9535_MODE_OUTPUT);  // LED1
    aw9535_set_mode(AW9535_PIN_P0_1, AW9535_MODE_OUTPUT);  // LED2
    aw9535_set_mode(AW9535_PIN_P0_2, AW9535_MODE_OUTPUT);  // LED3
    
    // 配置输入引脚 (按钮)
    aw9535_set_mode(AW9535_PIN_P1_0, AW9535_MODE_INPUT);   // Button1
    aw9535_set_mode(AW9535_PIN_P1_1, AW9535_MODE_INPUT);   // Button2
    
    // 根据按钮状态控制LED
    for (int i = 0; i < 100; i++) {
        aw9535_level_t btn1_level, btn2_level;
        
        aw9535_get_level(AW9535_PIN_P1_0, &btn1_level);
        aw9535_get_level(AW9535_PIN_P1_1, &btn2_level);
        
        // Button1 控制 LED1
        aw9535_set_level(AW9535_PIN_P0_0, btn1_level);
        
        // Button2 控制 LED2
        aw9535_set_level(AW9535_PIN_P0_1, btn2_level);
        
        // LED3 闪烁
        aw9535_toggle_level(AW9535_PIN_P0_2);
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/**
 * @brief AW9535驱动使用示例主函数
 */
void aw9535_example_main(void)
{
    ESP_LOGI(TAG, "=== AW9535 GPIO扩展器驱动示例 ===");
    
    // 初始化配置
    aw9535_config_t config = {
        .i2c_cfg = {
            .i2c_port = I2C_NUM_0,
            .dev_addr = AW9535_I2C_ADDR_DEFAULT,  // 0x24
            .sda_io = GPIO_NUM_2,                  // ESP32-S3 GPIO2
            .scl_io = GPIO_NUM_1,                  // ESP32-S3 GPIO1
            .freq_hz = 400000,                     // 400kHz
        },
        .int_io = GPIO_NUM_45,                     // ESP32-S3 GPIO45 用于中断
    };
    
    // 初始化AW9535驱动
    esp_err_t ret = aw9535_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AW9535初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    
    // 复位设备到默认状态
    aw9535_reset();
    
    // 运行示例
    example_gpio_output();       // 示例1: 输出控制
    example_gpio_input();        // 示例2: 输入读取
    example_port_control();      // 示例3: 批量控制
    example_gpio_interrupt();    // 示例4: 中断使用
    example_mixed_usage();       // 示例5: 混合使用
    
    // 清理
    aw9535_deinit();
    
    ESP_LOGI(TAG, "示例运行完成");
}

/**
 * @brief 应用层API封装示例 - LED控制
 */

// LED引脚定义
#define LED_RED     AW9535_PIN_P0_0
#define LED_GREEN   AW9535_PIN_P0_1
#define LED_BLUE    AW9535_PIN_P0_2

// LED初始化
esp_err_t app_led_init(void)
{
    aw9535_set_mode(LED_RED, AW9535_MODE_OUTPUT);
    aw9535_set_mode(LED_GREEN, AW9535_MODE_OUTPUT);
    aw9535_set_mode(LED_BLUE, AW9535_MODE_OUTPUT);
    
    // 默认全部关闭
    aw9535_set_level(LED_RED, AW9535_LEVEL_LOW);
    aw9535_set_level(LED_GREEN, AW9535_LEVEL_LOW);
    aw9535_set_level(LED_BLUE, AW9535_LEVEL_LOW);
    
    return ESP_OK;
}

// LED控制
void app_led_set_color(bool red, bool green, bool blue)
{
    aw9535_set_level(LED_RED, red ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
    aw9535_set_level(LED_GREEN, green ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
    aw9535_set_level(LED_BLUE, blue ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
}

/**
 * @brief 应用层API封装示例 - 按钮控制
 */

// 按钮引脚定义
#define BUTTON_1    AW9535_PIN_P1_0
#define BUTTON_2    AW9535_PIN_P1_1

// 按钮初始化
esp_err_t app_button_init(void)
{
    aw9535_set_mode(BUTTON_1, AW9535_MODE_INPUT);
    aw9535_set_mode(BUTTON_2, AW9535_MODE_INPUT);
    
    return ESP_OK;
}

// 读取按钮状态
bool app_button_is_pressed(int button_num)
{
    aw9535_pin_t pin = (button_num == 1) ? BUTTON_1 : BUTTON_2;
    aw9535_level_t level;
    
    if (aw9535_get_level(pin, &level) == ESP_OK) {
        return (level == AW9535_LEVEL_LOW);  // 假设按下时为低电平
    }
    
    return false;
}

// 配置按钮中断
esp_err_t app_button_set_callback(int button_num, aw9535_isr_t callback, void *user_data)
{
    aw9535_pin_t pin = (button_num == 1) ? BUTTON_1 : BUTTON_2;
    
    return aw9535_set_interrupt(pin, AW9535_INT_FALLING, callback, user_data);
}
