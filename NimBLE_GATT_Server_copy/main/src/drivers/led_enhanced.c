/**
 * @file led_enhanced.c
 * @brief LED驱动增强实现 - 通过AW9535 GPIO扩展器控制RGB LED
 * 
 * 硬件连接：
 * - LED_R: AW9535 P0.2 (GPIO扩展器) - 2025-12-24 硬件变更
 * - LED_G: AW9535 P0.3 (GPIO扩展器) - 2025-12-24 硬件变更
 * - LED_B: AW9535 P0.4 (GPIO扩展器) - 2025-12-24 硬件变更
 * 
 * 支持的LED模式（8种）：
 * 0x00: LED_OFF               - 关闭
 * 0x01: LED_PINK_BLINK        - 粉色闪烁
 * 0x02: LED_ORANGE_BLINK      - 橙色闪烁
 * 0x03: LED_YELLOW_BLINK      - 黄色闪烁
 * 0x04: LED_BLUE_BLINK        - 蓝色闪烁
 * 0x05: LED_BLUE_STATIC       - 蓝色静止灯光
 * 0x06: LED_BLUE_WHITE_SLOW   - 蓝白交替低频闪烁
 * 0x07: LED_BLUE_WHITE_MIXED  - 蓝+白光低频闪烁
 */

#include "drivers/hardware_interface.h"
#include "drivers/aw9535.h"
#include "drivers/aw9535_pinmap.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "LED_Enhanced";

/* ==================== GPIO扩展器引脚定义 ==================== */
#define LED_R_PIN       AW9535_PIN_LED_R  // 红色LED (P02)
#define LED_G_PIN       AW9535_PIN_LED_G  // 绿色LED (P03)
#define LED_B_PIN       AW9535_PIN_LED_B  // 蓝色LED (P04)

/* ==================== 闪烁参数 ==================== */
#define BLINK_FAST_MS           200   // 快速闪烁周期
#define BLINK_SLOW_MS           1000  // 慢速闪烁周期

/* 注意：led_pattern_id_t 已在 app/action_table.h 中定义 */

/* ==================== RGB颜色定义（宏方式，避免unused警告）==================== */
// RGB状态直接使用数字表示：r, g, b
#define SET_RGB(r, g, b)  set_rgb_digital(r, g, b)

/* ==================== 全局变量 ==================== */
static bool g_led_initialized = false;
static TaskHandle_t g_led_animation_task = NULL;
static led_pattern_id_t g_current_pattern = LED_PATTERN_OFF;  // 使用action_table.h中定义的枚举
static volatile bool g_animation_running = false;

/* 自动关闭定时器相关 */
static TimerHandle_t g_led_auto_off_timer = NULL;
static const uint32_t LED_AUTO_OFF_DELAY_MS = 3000;  // 3秒后自动关闭

/* ==================== 私有函数声明 ==================== */
static void set_rgb_digital(uint8_t r, uint8_t g, uint8_t b);

/* ==================== 私有函数 ==================== */

/**
 * @brief 定时器回调函数 - 3秒后自动关闭LED
 */
static void led_auto_off_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "⏰ Auto-off timer triggered - turning off LED after 3 seconds");
    g_current_pattern = LED_PATTERN_OFF;
    set_rgb_digital(0, 0, 0);
}

/**
 * @brief 设置RGB LED状态（数字输出）
 * @param r 红色：0=关闭，1=开启
 * @param g 绿色：0=关闭，1=开启
 * @param b 蓝色：0=关闭，1=开启
 */
static void set_rgb_digital(uint8_t r, uint8_t g, uint8_t b)
{
    aw9535_set_level(LED_R_PIN, r ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);  // R
    aw9535_set_level(LED_G_PIN, g ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);  // G
    aw9535_set_level(LED_B_PIN, b ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);  // B
}

/**
 * @brief LED动画任务
 * 
 * 实现用户需求的8种LED模式（简化版，使用GPIO扩展器数字输出）：
 * 用户需求模式映射到现有枚举：
 * 1. 关闭           → LED_PATTERN_OFF
 * 2. 粉色闪烁(R+B)  → LED_PATTERN_SOLID_RED (临时用红色闪烁代替)
 * 3. 橙色闪烁(R+G)  → LED_PATTERN_SOLID_GREEN (临时用绿色闪烁代替)
 * 4. 黄色闪烁(R+G)  → LED_PATTERN_SOLID_YELLOW (黄色闪烁)
 * 5. 蓝色闪烁(B)    → LED_PATTERN_BLINK_FAST (快速闪烁蓝色)
 * 6. 蓝色静止(B)    → LED_PATTERN_SOLID_BLUE (纯蓝色)
 * 7. 蓝白交替慢速   → LED_PATTERN_BLINK_SLOW (慢速闪烁)
 * 8. 蓝白混合快速   → LED_PATTERN_WAVE (波浪效果用于快速切换)
 * 
 * 注意：由于现有枚举不完全匹配用户需求，这是临时映射方案
 */
static void led_animation_task(void *arg)
{
    uint32_t tick = 0;
    
    while (1) {
        switch (g_current_pattern) {
            case LED_PATTERN_OFF:  // 0. 关闭
                set_rgb_digital(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case LED_PATTERN_SOLID_RED:  // 1. 粉红闪烁（R+B）500ms周期
                set_rgb_digital(1, 0, 1);  // 粉红=R+B
                vTaskDelay(pdMS_TO_TICKS(250));
                set_rgb_digital(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
                
            case LED_PATTERN_SOLID_GREEN:  // 2. 橙色闪烁（R+G）500ms周期
                set_rgb_digital(1, 1, 0);  // 橙色≈R+G
                vTaskDelay(pdMS_TO_TICKS(250));
                set_rgb_digital(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
                
            case LED_PATTERN_SOLID_YELLOW:  // 3. 黄色闪烁（R+G）500ms周期
                set_rgb_digital(1, 1, 0);  // 黄色=R+G
                vTaskDelay(pdMS_TO_TICKS(250));
                set_rgb_digital(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(250));
                break;
                
            case LED_PATTERN_SOLID_BLUE:  // 5. 蓝色静止
                set_rgb_digital(0, 0, 1);  // 蓝色=B 常亮
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case LED_PATTERN_BLINK_FAST:  // 4. 蓝色快速闪烁（200ms周期）
                set_rgb_digital(0, 0, 1);  // 蓝色=B
                vTaskDelay(pdMS_TO_TICKS(100));
                set_rgb_digital(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            case LED_PATTERN_BLINK_SLOW:  // 6. 蓝白交替（慢速1s周期）
                set_rgb_digital(0, 0, 1);  // 蓝色
                vTaskDelay(pdMS_TO_TICKS(500));
                set_rgb_digital(1, 1, 1);  // 白色
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
                
            case LED_PATTERN_WAVE:  // 7. 蓝白混合快速切换（200ms周期）
                tick++;
                if (tick % 2 == 0) {
                    set_rgb_digital(0, 0, 1);  // 蓝色
                } else {
                    set_rgb_digital(1, 1, 1);  // 白色
                }
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
                
            default:
                ESP_LOGW(TAG, "Unsupported pattern: %d, turning off", g_current_pattern);
                set_rgb_digital(0, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化LED驱动（使用AW9535 GPIO扩展器）
 * 硬件连接：
 *   - P0.2: CTRL_LED_R (红色通道) - 2025-12-24 硬件变更
 *   - P0.3: CTRL_LED_G (绿色通道) - 2025-12-24 硬件变更
 *   - P0.4: CTRL_LED_B (蓝色通道) - 2025-12-24 硬件变更
 * 
 * 注意：GPIO扩展器仅支持数字输出（高/低），不支持PWM
 */
esp_err_t hw_led_init(void)
{
    if (g_led_initialized) {
        ESP_LOGW(TAG, "LED already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing LED driver (AW9535 GPIO expander)...");
    
    // 配置P02/P03/P04为输出模式
    esp_err_t ret;
    ret = aw9535_set_mode(LED_R_PIN, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED_R (P02) as output");
        return ret;
    }
    
    ret = aw9535_set_mode(LED_G_PIN, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED_G (P03) as output");
        return ret;
    }
    
    ret = aw9535_set_mode(LED_B_PIN, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LED_B (P04) as output");
        return ret;
    }
    
    // 初始化为关闭状态
    set_rgb_digital(0, 0, 0);
    
    // 创建LED自动关闭定时器
    g_led_auto_off_timer = xTimerCreate(
        "LED_AutoOff",                    // 定时器名称
        pdMS_TO_TICKS(LED_AUTO_OFF_DELAY_MS), // 定时器周期 (3秒)
        pdFALSE,                           // 单次触发 (不重复)
        NULL,                              // 定时器ID
        led_auto_off_timer_callback        // 回调函数
    );
    
    if (g_led_auto_off_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create LED auto-off timer");
        return ESP_FAIL;
    }
    
    // 创建LED动画任务
    BaseType_t task_ret = xTaskCreate(
        led_animation_task, 
        "led_animation", 
        2048, 
        NULL, 
        5, 
        &g_led_animation_task
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED animation task");
        return ESP_FAIL;
    }
    
    g_led_initialized = true;
    ESP_LOGI(TAG, "LED driver initialized successfully");
    ESP_LOGI(TAG, "Supported 8 LED patterns: OFF, PINK_BLINK, ORANGE_BLINK, YELLOW_BLINK, BLUE_BLINK, BLUE_STATIC, BLUE_WHITE_SLOW, BLUE_WHITE_MIXED");
    
    return ESP_OK;
}

/**
 * @brief 设置LED模式
 * @param pattern LED模式ID (0x00-0x07)
 * 
 * LED模式定义：
 * 0x00: LED_OFF           - 关闭
 * 0x01: LED_PINK_BLINK    - 粉红闪烁
 * 0x02: LED_ORANGE_BLINK  - 橙色闪烁
 * 0x03: LED_YELLOW_BLINK  - 黄色闪烁
 * 0x04: LED_BLUE_BLINK    - 蓝色闪烁
 * 0x05: LED_BLUE_STATIC   - 蓝色常亮
 * 0x06: LED_BLUE_WHITE_SLOW - 蓝白交替慢速
 * 0x07: LED_BLUE_WHITE_MIXED - 蓝白混合快速
 */
esp_err_t hw_led_set_pattern(led_pattern_id_t pattern)
{
    if (!g_led_initialized) {
        ESP_LOGW(TAG, "LED not initialized");
        return ESP_FAIL;
    }
    
    if (pattern > 0x07) {
        ESP_LOGW(TAG, "Invalid LED pattern: 0x%02X (valid range: 0x00-0x07)", pattern);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Set LED pattern: 0x%02X", pattern);
    g_current_pattern = pattern;
    
    // 如果不是关闭模式，启动3秒自动关闭定时器
    if (pattern != LED_PATTERN_OFF && g_led_auto_off_timer != NULL) {
        // 先停止之前的定时器（如果正在运行）
        xTimerStop(g_led_auto_off_timer, 0);
        
        // 启动新的定时器
        if (xTimerStart(g_led_auto_off_timer, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "🕐 LED auto-off timer started (3 seconds)");
        } else {
            ESP_LOGW(TAG, "Failed to start LED auto-off timer");
        }
    }
    
    return ESP_OK;
}

/**
 * @brief 设置LED颜色（数字输出，仅支持8种组合）
 * @param r 红色：0=关闭，非0=开启
 * @param g 绿色：0=关闭，非0=开启
 * @param b 蓝色：0=关闭，非0=开启
 * 
 * 可用颜色组合：
 * (0,0,0) = OFF 关闭
 * (1,0,0) = RED 红色
 * (0,1,0) = GREEN 绿色
 * (0,0,1) = BLUE 蓝色
 * (1,1,0) = YELLOW/ORANGE 黄色/橙色
 * (1,0,1) = MAGENTA/PINK 洋红/粉红
 * (0,1,1) = CYAN 青色
 * (1,1,1) = WHITE 白色
 */
esp_err_t hw_led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    if (!g_led_initialized) {
        ESP_LOGW(TAG, "LED not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Set LED color: R=%d G=%d B=%d", r?1:0, g?1:0, b?1:0);
    set_rgb_digital(r?1:0, g?1:0, b?1:0);
    
    return ESP_OK;
}

/**
 * @brief 停止LED效果
 */
esp_err_t hw_led_stop(void)
{
    if (!g_led_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping LED");
    g_current_pattern = 0x00;
    set_rgb_digital(0, 0, 0);
    
    // 停止自动关闭定时器
    if (g_led_auto_off_timer != NULL) {
        xTimerStop(g_led_auto_off_timer, 0);
        ESP_LOGI(TAG, "LED auto-off timer stopped");
    }
    
    return ESP_OK;
}
