/**
 * @file vibrator.c
 * @brief 震动马达驱动实现 - GPIO控制方式
 * 
 * 震动模式（来自Blinko动作组合.xlsx J列）：
 * 1. 震动一下 - 单次短震 (120ms)
 * 2. 双震 - 两次短震 (100ms × 2，间隔150ms)
 * 3. 高频轻震 - 快速连续轻度震动 (50ms × 10，间隔50ms)
 * 
 * 硬件连接：MOTO3_RSTN -> ESP32S3 GPIO9
 */

#include "drivers/hardware_interface.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "vibrator.h"

static const char *TAG = "Vibrator";

/* ==================== 硬件配置 ==================== */
// 震动马达 MOTO3_RSTN -> ESP32S3 GPIO9
#define VIBRATOR_GPIO       GPIO_NUM_9

/* ==================== 震动模式定义 ==================== */
typedef struct {
    uint8_t pattern_id;
    const char *name;
    uint16_t duration_ms;   // 单次震动时长
    uint8_t repeat_count;   // 重复次数
    uint16_t interval_ms;   // 重复间隔
    vibrator_state_e state; // GPIO状态枚举
} vibration_pattern_config_t;

// 9种震动模式配置（匹配action_table.h中的vibration_pattern_id_t枚举）
static const vibration_pattern_config_t g_pattern_configs[] = {
    // pattern_id, name,              duration, repeat, interval, state
    {0x00, "关闭",                   0,        0,      0,        NONE_VIBRATION},           // VIB_PATTERN_OFF
    {0x01, "短脉冲(100ms)",          100,      1,      0,        SHORT_VIBRATION},          // VIB_PATTERN_SHORT_PULSE
    {0x02, "长脉冲(500ms)",          500,      1,      0,        SHORT_VIBRATION},          // VIB_PATTERN_LONG_PULSE
    {0x03, "双震",                   120,      2,      150,      DOUBLE_VIBRATION},         // VIB_PATTERN_DOUBLE_PULSE
    {0x04, "三连震",                 80,       3,      100,      HIGH_FRENQUENCY_VIBRATION}, // VIB_PATTERN_TRIPLE_PULSE
    {0x05, "持续弱震",               50,       20,     30,       HIGH_FRENQUENCY_VIBRATION}, // VIB_PATTERN_CONTINUOUS_WEAK
    {0x06, "持续强震",               150,      10,     50,       HIGH_FRENQUENCY_VIBRATION}, // VIB_PATTERN_CONTINUOUS_STRONG
    {0x07, "波浪震动",               60,       15,     40,       HIGH_FRENQUENCY_VIBRATION}, // VIB_PATTERN_WAVE
    {0x08, "心跳震动",               200,      2,      800,      DOUBLE_VIBRATION},         // VIB_PATTERN_HEARTBEAT (慢节拍)
};

#define PATTERN_COUNT (sizeof(g_pattern_configs) / sizeof(vibration_pattern_config_t))

/* ==================== 全局变量 ==================== */
static bool g_vibrator_initialized = false;
static TaskHandle_t g_vibrator_task = NULL;
static uint8_t g_current_pattern = 0;
static bool g_pattern_active = false;

/* ==================== GPIO控制函数 ==================== */

/**
 * @brief 初始化震动马达GPIO
 */
static void vibrator_gpio_init(void)
{
    gpio_config_t vibrator_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << VIBRATOR_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&vibrator_gpio_config);
    gpio_set_level(VIBRATOR_GPIO, 0);
    ESP_LOGI(TAG, "Vibrator GPIO initialized on GPIO%d", VIBRATOR_GPIO);
}

/**
 * @brief 启动震动马达
 */
static inline void vibrator_motor_on(void)
{
    gpio_set_level(VIBRATOR_GPIO, 1);
}

/**
 * @brief 停止震动马达
 */
static inline void vibrator_motor_off(void)
{
    gpio_set_level(VIBRATOR_GPIO, 0);
}

/* ==================== 模式控制 ==================== */

/**
 * @brief 获取模式配置
 */
static const vibration_pattern_config_t* get_pattern_config(uint8_t pattern_id)
{
    for (int i = 0; i < PATTERN_COUNT; i++) {
        if (g_pattern_configs[i].pattern_id == pattern_id) {
            return &g_pattern_configs[i];
        }
    }
    return NULL;
}

/**
 * @brief 执行震动模式（通用实现，支持所有9种模式）
 */
static void execute_vibration_pattern(const vibration_pattern_config_t *config)
{
    if (config == NULL) {
        ESP_LOGW(TAG, "Invalid vibration config");
        return;
    }
    
    if (config->repeat_count == 0 || config->duration_ms == 0) {
        ESP_LOGI(TAG, "Executing: %s (OFF)", config->name);
        vibrator_motor_off();
        return;
    }

    ESP_LOGI(TAG, "🔥 Executing: %s (%d × %dms, interval %dms) - GPIO9 HIGH", 
             config->name, config->repeat_count, config->duration_ms, config->interval_ms);
    
    // 通用震动模式实现：根据配置参数执行震动序列
    for (int i = 0; i < config->repeat_count; i++) {
        // 开始震动 - 确保GPIO9被拉高
        ESP_LOGI(TAG, "🟢 Vibration ON - GPIO9 = HIGH (cycle %d/%d)", i + 1, config->repeat_count);
        vibrator_motor_on();
        vTaskDelay(pdMS_TO_TICKS(config->duration_ms));
        
        // 停止震动
        ESP_LOGI(TAG, "🔴 Vibration OFF - GPIO9 = LOW");
        vibrator_motor_off();
        
        // 如果不是最后一次，添加间隔
        if (i < config->repeat_count - 1 && config->interval_ms > 0) {
            ESP_LOGI(TAG, "⏸️ Interval delay: %dms", config->interval_ms);
            vTaskDelay(pdMS_TO_TICKS(config->interval_ms));
        }
    }
    
    ESP_LOGI(TAG, "✅ Vibration pattern completed: %s", config->name);
}

/**
 * @brief 震动序列任务
 */
static void vibrator_pattern_task(void *arg)
{
    while (1) {
        if (!g_pattern_active || g_current_pattern == 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const vibration_pattern_config_t *config = get_pattern_config(g_current_pattern);
        if (config == NULL) {
            ESP_LOGW(TAG, "Unknown pattern: 0x%02X", g_current_pattern);
            g_pattern_active = false;
            continue;
        }

        ESP_LOGI(TAG, "Executing pattern: %s", config->name);
        execute_vibration_pattern(config);

        // 完成后停止
        g_pattern_active = false;
        g_current_pattern = 0;
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化震动马达
 */
esp_err_t hw_vibrator_init(void)
{
    if (g_vibrator_initialized) {
        ESP_LOGW(TAG, "Vibrator already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing vibrator driver (GPIO mode)...");

    // 初始化GPIO
    vibrator_gpio_init();

    // 创建震动控制任务
    BaseType_t ret = xTaskCreate(
        vibrator_pattern_task, 
        "vibrator", 
        4096,  // 增加栈大小：2048→4096 (防止日志输出栈溢出)
        NULL, 
        5, 
        &g_vibrator_task
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create vibrator task");
        return ESP_FAIL;
    }
    
    g_vibrator_initialized = true;
    
    ESP_LOGI(TAG, "🔥 Vibrator driver initialized - 9 patterns supported (GPIO9):");
    ESP_LOGI(TAG, "  [0x00] %s", g_pattern_configs[0].name);
    ESP_LOGI(TAG, "  [0x01] %s (%dms)", g_pattern_configs[1].name, g_pattern_configs[1].duration_ms);
    ESP_LOGI(TAG, "  [0x02] %s (%dms)", g_pattern_configs[2].name, g_pattern_configs[2].duration_ms);
    ESP_LOGI(TAG, "  [0x03] %s (%dms × %d)", g_pattern_configs[3].name, g_pattern_configs[3].duration_ms, g_pattern_configs[3].repeat_count);
    ESP_LOGI(TAG, "  [0x04] %s (%dms × %d)", g_pattern_configs[4].name, g_pattern_configs[4].duration_ms, g_pattern_configs[4].repeat_count);
    ESP_LOGI(TAG, "  [0x05] %s (%dms × %d)", g_pattern_configs[5].name, g_pattern_configs[5].duration_ms, g_pattern_configs[5].repeat_count);
    ESP_LOGI(TAG, "  [0x06] %s (%dms × %d)", g_pattern_configs[6].name, g_pattern_configs[6].duration_ms, g_pattern_configs[6].repeat_count);
    ESP_LOGI(TAG, "  [0x07] %s (%dms × %d)", g_pattern_configs[7].name, g_pattern_configs[7].duration_ms, g_pattern_configs[7].repeat_count);
    ESP_LOGI(TAG, "  [0x08] %s (%dms × %d)", g_pattern_configs[8].name, g_pattern_configs[8].duration_ms, g_pattern_configs[8].repeat_count);
    
    return ESP_OK;
}

/**
 * @brief 设置震动模式
 */
esp_err_t hw_vibrator_set_pattern(vibration_pattern_id_t pattern)
{
    if (!g_vibrator_initialized) {
        ESP_LOGW(TAG, "Vibrator not initialized");
        return ESP_FAIL;
    }
    
    const vibration_pattern_config_t *config = get_pattern_config(pattern);
    if (config == NULL) {
        ESP_LOGW(TAG, "Unknown pattern: 0x%02X", pattern);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Set pattern: %s (0x%02X)", config->name, pattern);
    g_current_pattern = pattern;
    g_pattern_active = true;
    
    return ESP_OK;
}

/**
 * @brief 启动震动模式（支持时长参数）
 */
esp_err_t hw_vibrator_start_pattern(vibration_pattern_id_t pattern, uint16_t duration_ms)
{
    // 当前实现使用配置表的默认时长
    // TODO: 如需要支持自定义时长，可在此处添加临时配置逻辑
    (void)duration_ms;
    return hw_vibrator_set_pattern(pattern);
}

/**
 * @brief 设置震动强度
 * @note GPIO模式不支持强度调节
 */
esp_err_t hw_vibrator_set_intensity(uint8_t intensity)
{
    (void)intensity;
    ESP_LOGW(TAG, "GPIO mode does not support intensity control");
    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @brief 停止震动
 */
esp_err_t hw_vibrator_stop(void)
{
    if (!g_vibrator_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping vibrator");
    g_pattern_active = false;
    g_current_pattern = 0;
    vibrator_motor_off();
    
    return ESP_OK;
}

/**
 * @brief 获取震动模式名称
 */
const char* hw_vibrator_get_pattern_name(vibration_pattern_id_t pattern)
{
    const vibration_pattern_config_t *config = get_pattern_config(pattern);
    return config ? config->name : "Unknown";
}