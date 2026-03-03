/**
 * @file touch_sensor.c
 * @brief TTP223触摸传感器驱动实现（支持GPIO中断）
 * 
 * 硬件连接：
 * - TTP223-1 (额头): TOUCH_INTN_1 -> ESP32-S3 GPIO4
 * - TTP223-2 (后背): TOUCH_INTN_2 -> ESP32-S3 GPIO5
 * 
 * 工作原理：
 * - TTP223是数字触摸传感器芯片
 * - 输出模式：触摸时GPIO输出高电平，未触摸时输出低电平
 * - 响应时间：约60ms
 * - 中断模式：上升沿=按下，下降沿=释放
 * 
 * 注意事项：
 * - GPIO需要配置为输入模式
 * - 配置内部下拉电阻确保未触摸时稳定低电平
 * - 中断回调在ISR上下文中执行，应尽快返回
 */

#include "drivers/touch_ttp223.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TTP223";

/* GPIO定义 */
#define TOUCH_GPIO_FOREHEAD     GPIO_NUM_4  // 额头触摸
#define TOUCH_GPIO_BACK         GPIO_NUM_5  // 后背触摸

/* 防抖时间（微秒） */
#define TOUCH_DEBOUNCE_TIME_US  60000  // 60ms

/* 初始化状态 */
static bool g_touch_initialized = false;
static bool g_isr_service_installed = false;

/* 触摸回调函数 */
static touch_ttp223_callback_t g_touch_callback = NULL;

/* 防抖定时器 */
static uint64_t g_last_touch_time[TOUCH_CHANNEL_MAX] = {0};

/* 中断使能标志 */
static bool g_interrupt_enabled[TOUCH_CHANNEL_MAX] = {false, false};

/* ==================== 私有辅助函数 ==================== */

/**
 * @brief 获取通道对应的GPIO编号
 */
static gpio_num_t get_gpio_num(touch_ttp223_channel_t channel)
{
    return (channel == TOUCH_CHANNEL_FOREHEAD) ? TOUCH_GPIO_FOREHEAD : TOUCH_GPIO_BACK;
}

/**
 * @brief GPIO中断处理函数（在ISR上下文中执行）
 * @param arg GPIO编号（作为void*传入）
 */
static void IRAM_ATTR touch_gpio_isr_handler(void* arg)
{
    gpio_num_t gpio_num = (gpio_num_t)(uint32_t)arg;
    
    // 确定触发的通道
    touch_ttp223_channel_t channel;
    if (gpio_num == TOUCH_GPIO_FOREHEAD) {
        channel = TOUCH_CHANNEL_FOREHEAD;
    } else if (gpio_num == TOUCH_GPIO_BACK) {
        channel = TOUCH_CHANNEL_BACK;
    } else {
        return;  // 未知GPIO
    }
    
    // 防抖处理
    uint64_t current_time = esp_timer_get_time();
    if ((current_time - g_last_touch_time[channel]) < TOUCH_DEBOUNCE_TIME_US) {
        return;  // 在防抖时间内，忽略
    }
    g_last_touch_time[channel] = current_time;
    
    // 读取当前GPIO电平，确定事件类型
    int level = gpio_get_level(gpio_num);
    touch_ttp223_event_t event_type = (level == 1) ? TOUCH_EVENT_PRESSED : TOUCH_EVENT_RELEASED;
    
    // 触发回调（如果已注册）
    if (g_touch_callback != NULL) {
        touch_ttp223_event_data_t event_data = {
            .channel = channel,
            .event_type = event_type,
            .timestamp_ms = (uint32_t)(current_time / 1000)
        };
        g_touch_callback(&event_data);
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化触摸传感器GPIO
 */
esp_err_t hw_touch_sensor_init(void)
{
    if (g_touch_initialized) {
        ESP_LOGW(TAG, "Touch sensor already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing TTP223 touch sensors...");
    
    // 配置GPIO4 (额头触摸)
    gpio_config_t io_conf_forehead = {
        .pin_bit_mask = (1ULL << TOUCH_GPIO_FOREHEAD),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // 下拉确保未触摸时为低电平
        .intr_type = GPIO_INTR_ANYEDGE         // 上升沿和下降沿都触发
    };
    esp_err_t ret = gpio_config(&io_conf_forehead);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", 
                 TOUCH_GPIO_FOREHEAD, esp_err_to_name(ret));
        return ret;
    }
    
    // 配置GPIO5 (后背触摸)
    gpio_config_t io_conf_back = {
        .pin_bit_mask = (1ULL << TOUCH_GPIO_BACK),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,  // 下拉确保未触摸时为低电平
        .intr_type = GPIO_INTR_ANYEDGE         // 上升沿和下降沿都触发
    };
    ret = gpio_config(&io_conf_back);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", 
                 TOUCH_GPIO_BACK, esp_err_to_name(ret));
        return ret;
    }
    
    // 安装GPIO ISR服务（如果还未安装）
    if (!g_isr_service_installed) {
        ret = gpio_install_isr_service(0);
        if (ret == ESP_OK) {
            g_isr_service_installed = true;
            ESP_LOGI(TAG, "GPIO ISR service installed");
        } else if (ret == ESP_ERR_INVALID_STATE) {
            // ISR服务已经安装，不是错误
            g_isr_service_installed = true;
            ESP_LOGI(TAG, "GPIO ISR service already installed");
        } else {
            ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // 添加GPIO中断处理函数
    ret = gpio_isr_handler_add(TOUCH_GPIO_FOREHEAD, touch_gpio_isr_handler, (void*)TOUCH_GPIO_FOREHEAD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO%d: %s", 
                 TOUCH_GPIO_FOREHEAD, esp_err_to_name(ret));
        return ret;
    }
    
    ret = gpio_isr_handler_add(TOUCH_GPIO_BACK, touch_gpio_isr_handler, (void*)TOUCH_GPIO_BACK);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO%d: %s", 
                 TOUCH_GPIO_BACK, esp_err_to_name(ret));
        gpio_isr_handler_remove(TOUCH_GPIO_FOREHEAD);  // 清理已添加的
        return ret;
    }
    
    g_interrupt_enabled[TOUCH_CHANNEL_FOREHEAD] = true;
    g_interrupt_enabled[TOUCH_CHANNEL_BACK] = true;
    g_touch_initialized = true;
    
    ESP_LOGI(TAG, "TTP223 touch sensors initialized:");
    ESP_LOGI(TAG, "  GPIO%d: Forehead touch (TOUCH_INTN_1) - Interrupt enabled", TOUCH_GPIO_FOREHEAD);
    ESP_LOGI(TAG, "  GPIO%d: Back touch (TOUCH_INTN_2) - Interrupt enabled", TOUCH_GPIO_BACK);
    
    return ESP_OK;
}

/**
 * @brief 反初始化触摸传感器
 */
esp_err_t hw_touch_sensor_deinit(void)
{
    if (!g_touch_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing TTP223 touch sensors...");
    
    // 移除ISR处理函数
    gpio_isr_handler_remove(TOUCH_GPIO_FOREHEAD);
    gpio_isr_handler_remove(TOUCH_GPIO_BACK);
    
    // 重置GPIO配置
    gpio_reset_pin(TOUCH_GPIO_FOREHEAD);
    gpio_reset_pin(TOUCH_GPIO_BACK);
    
    g_touch_callback = NULL;
    g_touch_initialized = false;
    g_interrupt_enabled[TOUCH_CHANNEL_FOREHEAD] = false;
    g_interrupt_enabled[TOUCH_CHANNEL_BACK] = false;
    
    ESP_LOGI(TAG, "TTP223 touch sensors deinitialized");
    return ESP_OK;
}

/**
 * @brief 检测额头触摸状态
 * @return true 已触摸（GPIO高电平），false 未触摸（GPIO低电平）
 */
bool hw_touch_sensor_is_forehead_touched(void)
{
    if (!g_touch_initialized) {
        ESP_LOGW(TAG, "Touch sensor not initialized");
        return false;
    }
    
    // 读取GPIO4状态：高电平=触摸，低电平=未触摸
    int level = gpio_get_level(TOUCH_GPIO_FOREHEAD);
    return (level == 1);
}

/**
 * @brief 检测后背触摸状态
 * @return true 已触摸（GPIO高电平），false 未触摸（GPIO低电平）
 */
bool hw_touch_sensor_is_back_touched(void)
{
    if (!g_touch_initialized) {
        ESP_LOGW(TAG, "Touch sensor not initialized");
        return false;
    }
    
    // 读取GPIO5状态：高电平=触摸，低电平=未触摸
    int level = gpio_get_level(TOUCH_GPIO_BACK);
    return (level == 1);
}

/**
 * @brief 检测任意触摸状态
 * @return true 任意一个通道被触摸，false 都未触摸
 */
bool hw_touch_sensor_is_any_touched(void)
{
    return hw_touch_sensor_is_forehead_touched() || 
           hw_touch_sensor_is_back_touched();
}

/**
 * @brief 注册触摸事件回调函数
 */
esp_err_t hw_touch_sensor_register_callback(touch_ttp223_callback_t callback)
{
    if (!g_touch_initialized) {
        ESP_LOGW(TAG, "Touch sensor not initialized");
        return ESP_FAIL;
    }
    
    if (callback == NULL) {
        ESP_LOGW(TAG, "Callback is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    g_touch_callback = callback;
    ESP_LOGI(TAG, "Touch event callback registered");
    return ESP_OK;
}

/**
 * @brief 注销触摸事件回调函数
 */
esp_err_t hw_touch_sensor_unregister_callback(void)
{
    g_touch_callback = NULL;
    ESP_LOGI(TAG, "Touch event callback unregistered");
    return ESP_OK;
}

/**
 * @brief 启用触摸中断
 */
esp_err_t hw_touch_sensor_enable_interrupt(touch_ttp223_channel_t channel)
{
    if (!g_touch_initialized) {
        ESP_LOGW(TAG, "Touch sensor not initialized");
        return ESP_FAIL;
    }
    
    if (channel >= TOUCH_CHANNEL_MAX) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }
    
    gpio_num_t gpio_num = get_gpio_num(channel);
    esp_err_t ret = gpio_intr_enable(gpio_num);
    if (ret == ESP_OK) {
        g_interrupt_enabled[channel] = true;
        ESP_LOGI(TAG, "Interrupt enabled for %s", hw_touch_sensor_get_channel_name(channel));
    }
    
    return ret;
}

/**
 * @brief 禁用触摸中断
 */
esp_err_t hw_touch_sensor_disable_interrupt(touch_ttp223_channel_t channel)
{
    if (!g_touch_initialized) {
        ESP_LOGW(TAG, "Touch sensor not initialized");
        return ESP_FAIL;
    }
    
    if (channel >= TOUCH_CHANNEL_MAX) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }
    
    gpio_num_t gpio_num = get_gpio_num(channel);
    esp_err_t ret = gpio_intr_disable(gpio_num);
    if (ret == ESP_OK) {
        g_interrupt_enabled[channel] = false;
        ESP_LOGI(TAG, "Interrupt disabled for %s", hw_touch_sensor_get_channel_name(channel));
    }
    
    return ret;
}

/**
 * @brief 获取触摸通道名称
 */
const char* hw_touch_sensor_get_channel_name(touch_ttp223_channel_t channel)
{
    switch (channel) {
        case TOUCH_CHANNEL_FOREHEAD:
            return "Forehead";
        case TOUCH_CHANNEL_BACK:
            return "Back";
        default:
            return "Unknown";
    }
}
