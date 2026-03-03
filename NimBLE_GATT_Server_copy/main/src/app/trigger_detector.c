/*
 * Trigger Detector Implementation - 触发检测器实现
 */

#include "app/trigger_detector.h"
#include "drivers/hardware_interface.h"
#include "drivers/touch_ttp223.h"  // TTP223触摸传感器中断接口
#include "drivers/voice_recognition.h"  // 语音识别驱动
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "TriggerDetector";

/* ==================== 外部变量声明 ==================== */
// 来自audio_player.c - 用于检查是否正在播放音频
extern bool g_is_playing;

/* ==================== 默认配置 ==================== */
#define DEFAULT_DEBOUNCE_MS         200     // 200ms去抖动
#define DEFAULT_MIN_INTERVAL_MS     1000    // 1秒最小间隔
#define DEFAULT_AUTO_INTERVAL_MS    30000   // 30秒自动触发
#define DEFAULT_TOUCH_THRESHOLD     50      // 触摸阈值50%
#define DEFAULT_VOICE_THRESHOLD     60      // 语音阈值60%

#define DETECTOR_TASK_STACK_SIZE    4096
#define DETECTOR_TASK_PRIORITY      5
#define DETECTOR_CHECK_INTERVAL_MS  100     // 检测任务循环间隔
#define EVENT_QUEUE_SIZE            10      // 事件队列大小

/* ==================== 全局变量 ==================== */
static bool g_initialized = false;
static bool g_running = false;
static TaskHandle_t g_detector_task_handle = NULL;
static QueueHandle_t g_event_queue = NULL;  // 事件队列
static trigger_callback_t g_callback = NULL;
static void *g_callback_user_data = NULL;

static trigger_detector_config_t g_config = {
    .touch_enabled = true,
    .approach_enabled = true,
    .voice_enabled = true,
    .auto_enabled = false,
    .debounce_ms = DEFAULT_DEBOUNCE_MS,
    .min_interval_ms = DEFAULT_MIN_INTERVAL_MS,
    .auto_interval_ms = DEFAULT_AUTO_INTERVAL_MS,
    .touch_threshold = DEFAULT_TOUCH_THRESHOLD,
    .voice_threshold = DEFAULT_VOICE_THRESHOLD
};

static trigger_statistics_t g_statistics = {0};

/* 去抖动状态 */
static struct {
    bool touch_state;
    bool approach_state;
    bool voice_state;       // 语音检测状态（用于边沿检测）
    uint32_t touch_last_time;
    uint32_t approach_last_time;
    uint32_t voice_last_time;
    uint32_t last_trigger_time;
    uint32_t auto_last_time;
} g_debounce_state = {0};

/* 触发类型名称表 */
static const char *trigger_type_names[] = {
    "Unknown",
    "Touch",
    "Approach",
    "Voice",
    "Auto"
};

/* ==================== 私有函数声明 ==================== */
static void detector_task(void *arg);
static bool check_voice_trigger(void);
static bool check_approach_trigger(void);
static bool check_auto_trigger(void);
static void fire_trigger_event(trigger_type_t type, uint8_t intensity);
static void fire_trigger_event_task(trigger_type_t type, uint8_t intensity);
static bool should_trigger(trigger_type_t type);
static uint32_t get_current_time_ms(void);
static void on_touch_event(const touch_ttp223_event_data_t* event_data);

/* ==================== 私有函数实现 ==================== */

/**
 * @brief TTP223触摸传感器中断回调函数
 * 
 * 注意：在ISR上下文中执行，应尽快返回
 */
static void on_touch_event(const touch_ttp223_event_data_t* event_data) {
    // 只处理按下事件（释放事件忽略）
    if (event_data->event_type != TOUCH_EVENT_PRESSED) {
        return;
    }
    
    // 检查触摸触发是否启用
    if (!g_config.touch_enabled) {
        return;
    }
    
    // 检查是否应该触发（去抖动和间隔检查）
    if (!should_trigger(TRIGGER_TYPE_TOUCH)) {
        return;
    }
    
    // 触发事件（TTP223是数字传感器，使用固定强度100）
    fire_trigger_event(TRIGGER_TYPE_TOUCH, 100);
    
    // 注意：ISR中不能使用ESP_LOGI，会导致崩溃
    // 日志输出已移到 fire_trigger_event 的任务上下文中
}

/**
 * @brief 获取当前时间（毫秒）
 */
static uint32_t get_current_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief 检查是否应该触发（考虑去抖动和最小间隔）
 */
static bool should_trigger(trigger_type_t type) {
    uint32_t current_time = get_current_time_ms();
    uint32_t *last_time_ptr = NULL;
    
    switch (type) {
        case TRIGGER_TYPE_TOUCH:
            last_time_ptr = &g_debounce_state.touch_last_time;
            break;
        case TRIGGER_TYPE_APPROACH:
            last_time_ptr = &g_debounce_state.approach_last_time;
            break;
        case TRIGGER_TYPE_VOICE:
            last_time_ptr = &g_debounce_state.voice_last_time;
            break;
        case TRIGGER_TYPE_AUTO:
            last_time_ptr = &g_debounce_state.auto_last_time;
            break;
        default:
            return false;
    }
    
    // 检查去抖动时间
    if ((current_time - *last_time_ptr) < g_config.debounce_ms) {
        return false;
    }
    
    // 🔒 播放期间完全阻止新触发（防止重复播放）
    // ⚠️ 重要: 更新去抖动时间,防止播放后立即重复触发
    if (g_is_playing) {
        *last_time_ptr = current_time;  // 更新去抖动时间
        g_debounce_state.last_trigger_time = current_time;  // 更新全局触发时间
        // 注意: 不能在此处使用ESP_LOGI (可能在ISR中调用)
        return false;
    }
    
    // 检查最小触发间隔（增加到5秒，避免重复触发）
    uint32_t min_interval = 5000;  // 5秒强制间隔
    uint32_t elapsed = current_time - g_debounce_state.last_trigger_time;
    if (elapsed < min_interval) {
        *last_time_ptr = current_time;  // 更新去抖动时间
        // 注意: 不能在此处使用ESP_LOGI (可能在ISR中调用)
        return false;
    }
    
    // 更新时间
    *last_time_ptr = current_time;
    
    return true;
}

/**
 * @brief 触发事件（内部实现，支持ISR和任务上下文）
 * 
 * @param type 触发类型
 * @param intensity 触发强度
 * @param from_isr 是否从ISR调用
 */
static void fire_trigger_event_internal(trigger_type_t type, uint8_t intensity, bool from_isr) {
    // 更新最后触发时间
    g_debounce_state.last_trigger_time = get_current_time_ms();
    
    // 创建事件
    trigger_event_t event = {
        .type = type,
        .timestamp = g_debounce_state.last_trigger_time,
        .intensity = intensity,
        .user_data = NULL
    };
    
    // 更新统计
    g_statistics.total_count++;
    switch (type) {
        case TRIGGER_TYPE_TOUCH:
            g_statistics.touch_count++;
            break;
        case TRIGGER_TYPE_APPROACH:
            g_statistics.approach_count++;
            break;
        case TRIGGER_TYPE_VOICE:
            g_statistics.voice_count++;
            break;
        case TRIGGER_TYPE_AUTO:
            g_statistics.auto_count++;
            break;
        default:
            break;
    }
    g_statistics.last_trigger_time = event.timestamp;
    
    // 根据上下文选择队列发送方式
    if (g_event_queue) {
        if (from_isr) {
            // ISR上下文（如触摸中断）
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(g_event_queue, &event, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        } else {
            // 任务上下文（如语音回调、轮询检测）
            xQueueSend(g_event_queue, &event, 0);
        }
    }
}

/**
 * @brief 触发事件（ISR版本，供GPIO中断使用）
 * 
 * 用于：触摸中断回调（on_touch_event）
 */
static void fire_trigger_event(trigger_type_t type, uint8_t intensity) {
    fire_trigger_event_internal(type, intensity, true);
}

/**
 * @brief 触发事件（任务版本，供轮询和回调使用）
 * 
 * 用于：语音回调、PIR轮询、自动触发定时器
 */
static void fire_trigger_event_task(trigger_type_t type, uint8_t intensity) {
    fire_trigger_event_internal(type, intensity, false);
}

/**
 * @brief 检测语音触发（GPIO44轮询模式）
 */
static bool check_voice_trigger(void) {
    if (!g_config.voice_enabled) {
        return false;
    }
    
    // 调用硬件接口检测语音关键词（GPIO44）
    bool is_detected = hw_voice_sensor_is_detected();
    
    // 边沿检测（从未检测到检测到）
    if (is_detected && !g_debounce_state.voice_state) {
        g_debounce_state.voice_state = true;
        
        if (should_trigger(TRIGGER_TYPE_VOICE)) {
            // 语音传感器固定强度值100（表示检测到关键词）
            // 使用任务版本（在detector_task中轮询调用）
            fire_trigger_event_task(TRIGGER_TYPE_VOICE, 100);
            return true;
        }
    } else if (!is_detected) {
        g_debounce_state.voice_state = false;
    }
    
    return false;
}

/**
 * @brief 检测接近触发
 */
static bool check_approach_trigger(void) {
    if (!g_config.approach_enabled) {
        return false;
    }
    
    // 调用硬件接口检测PIR人体感应（GPIO6）
    bool is_detected = hw_pir_sensor_is_detected();
    
    // 边沿检测（从未接近到接近）
    if (is_detected && !g_debounce_state.approach_state) {
        g_debounce_state.approach_state = true;
        
        if (should_trigger(TRIGGER_TYPE_APPROACH)) {
            // PIR传感器是数字传感器，使用固定强度值
            // 使用任务版本（在detector_task中轮询调用）
            fire_trigger_event_task(TRIGGER_TYPE_APPROACH, 100);
            return true;
        }
    } else if (!is_detected) {
        g_debounce_state.approach_state = false;
    }
    
    return false;
}

/**
 * @brief 检测自动触发
 */
static bool check_auto_trigger(void) {
    if (!g_config.auto_enabled) {
        return false;
    }
    
    uint32_t current_time = get_current_time_ms();
    
    if ((current_time - g_debounce_state.auto_last_time) >= g_config.auto_interval_ms) {
        g_debounce_state.auto_last_time = current_time;
        // 使用任务版本（在detector_task中调用）
        fire_trigger_event_task(TRIGGER_TYPE_AUTO, 50);
        return true;
    }
    
    return false;
}

/**
 * @brief 触发检测任务
 * 
 * 触摸通过GPIO中断触发 (on_touch_event)
 * 语音、PIR和自动触发通过轮询实现
 */
static void detector_task(void *arg) {
    ESP_LOGI(TAG, "Detector task started (touch via interrupt, voice+approach+auto via polling)");
    
    trigger_event_t event;
    TickType_t poll_ticks = pdMS_TO_TICKS(DETECTOR_CHECK_INTERVAL_MS);
    
    while (g_running) {
        // 优先检查来自ISR的队列事件（非阻塞）
        if (g_event_queue && xQueueReceive(g_event_queue, &event, 0) == pdTRUE) {
            // 从ISR接收到触发事件，在任务上下文中安全处理
            if (g_callback) {
                ESP_LOGI(TAG, "触发事件: 类型=%d, 强度=%d", event.type, event.intensity);
                g_callback(&event, g_callback_user_data);
            }
        }
        
        // 轮询检测语音、PIR接近和自动触发
        check_voice_trigger();
        check_approach_trigger();
        check_auto_trigger();
        
        // 等待一段时间
        vTaskDelay(poll_ticks);
    }
    
    ESP_LOGI(TAG, "Detector task stopped");
    g_detector_task_handle = NULL;
    vTaskDelete(NULL);
}

/* ==================== 公共函数实现 ==================== */

esp_err_t trigger_detector_init(void) {
    return trigger_detector_init_with_config(&g_config);
}

esp_err_t trigger_detector_init_with_config(const trigger_detector_config_t *config) {
    if (g_initialized) {
        ESP_LOGW(TAG, "Trigger detector already initialized");
        return ESP_OK;
    }
    
    if (config) {
        memcpy(&g_config, config, sizeof(trigger_detector_config_t));
    }
    
    ESP_LOGI(TAG, "Initializing trigger detector...");
    ESP_LOGI(TAG, "  Touch: %s (interrupt mode)", g_config.touch_enabled ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Approach: %s", g_config.approach_enabled ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Voice: %s", g_config.voice_enabled ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Auto: %s (interval=%lu ms)", 
             g_config.auto_enabled ? "enabled" : "disabled",
             g_config.auto_interval_ms);
    
    // 创建事件队列用于ISR到任务的通信
    g_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(trigger_event_t));
    if (!g_event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "  Event queue created (size=%d)", EVENT_QUEUE_SIZE);
    
    // 初始化时间戳
    uint32_t current_time = get_current_time_ms();
    g_debounce_state.auto_last_time = current_time;
    g_debounce_state.last_trigger_time = current_time;
    
    // 注册TTP223触摸传感器中断回调
    if (g_config.touch_enabled) {
        esp_err_t ret = hw_touch_sensor_register_callback(on_touch_event);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Touch interrupt callback registered");
        } else {
            ESP_LOGW(TAG, "  Failed to register touch interrupt callback: %s", esp_err_to_name(ret));
            ESP_LOGI(TAG, "  Will fall back to polling mode");
        }
    }
    
    ESP_LOGI(TAG, "  Voice detection uses GPIO44 polling (no callback needed)");
    
    g_initialized = true;
    ESP_LOGI(TAG, "Trigger detector initialized");
    
    return ESP_OK;
}

esp_err_t trigger_detector_register_callback(trigger_callback_t callback, void *user_data) {
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_callback = callback;
    g_callback_user_data = user_data;
    
    ESP_LOGI(TAG, "Callback registered");
    return ESP_OK;
}

esp_err_t trigger_detector_start(void) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Trigger detector not initialized");
        return ESP_FAIL;
    }
    
    if (g_running) {
        ESP_LOGW(TAG, "Trigger detector already running");
        return ESP_OK;
    }
    
    if (!g_callback) {
        ESP_LOGE(TAG, "No callback registered");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Starting trigger detector...");
    
    g_running = true;
    
    // 创建检测任务
    BaseType_t ret = xTaskCreate(
        detector_task,
        "trigger_detector",
        DETECTOR_TASK_STACK_SIZE,
        NULL,
        DETECTOR_TASK_PRIORITY,
        &g_detector_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create detector task");
        g_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Trigger detector started");
    return ESP_OK;
}

esp_err_t trigger_detector_stop(void) {
    if (!g_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping trigger detector...");
    
    g_running = false;
    
    // 等待任务结束
    if (g_detector_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(200));  // 等待任务退出
    }
    
    // 清空并删除队列
    if (g_event_queue) {
        vQueueDelete(g_event_queue);
        g_event_queue = NULL;
        ESP_LOGI(TAG, "Event queue deleted");
    }
    
    g_initialized = false;
    
    ESP_LOGI(TAG, "Trigger detector stopped");
    return ESP_OK;
}

bool trigger_detector_is_running(void) {
    return g_running;
}

esp_err_t trigger_detector_enable_type(trigger_type_t type, bool enabled) {
    switch (type) {
        case TRIGGER_TYPE_TOUCH:
            g_config.touch_enabled = enabled;
            break;
        case TRIGGER_TYPE_APPROACH:
            g_config.approach_enabled = enabled;
            break;
        case TRIGGER_TYPE_VOICE:
            g_config.voice_enabled = enabled;
            break;
        case TRIGGER_TYPE_AUTO:
            g_config.auto_enabled = enabled;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "%s trigger %s",
             trigger_detector_get_type_name(type),
             enabled ? "enabled" : "disabled");
    
    return ESP_OK;
}

esp_err_t trigger_detector_manual_trigger(trigger_type_t type, uint8_t intensity) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "Trigger detector not initialized");
        return ESP_FAIL;
    }
    
    if (type >= TRIGGER_TYPE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Manual trigger: %s (intensity=%d)",
             trigger_detector_get_type_name(type), intensity);
    
    fire_trigger_event(type, intensity);
    
    return ESP_OK;
}

esp_err_t trigger_detector_get_statistics(trigger_statistics_t *stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(stats, &g_statistics, sizeof(trigger_statistics_t));
    return ESP_OK;
}

esp_err_t trigger_detector_reset_statistics(void) {
    memset(&g_statistics, 0, sizeof(trigger_statistics_t));
    ESP_LOGI(TAG, "Statistics reset");
    return ESP_OK;
}

const char* trigger_detector_get_type_name(trigger_type_t type) {
    if (type >= TRIGGER_TYPE_MAX) {
        return trigger_type_names[0];  // "Unknown"
    }
    return trigger_type_names[type];
}

esp_err_t trigger_detector_get_config(trigger_detector_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(config, &g_config, sizeof(trigger_detector_config_t));
    return ESP_OK;
}

esp_err_t trigger_detector_update_config(const trigger_detector_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&g_config, config, sizeof(trigger_detector_config_t));
    ESP_LOGI(TAG, "Configuration updated");
    
    return ESP_OK;
}

void trigger_detector_clear_queue(void) {
    if (!g_event_queue) {
        return;
    }
    
    trigger_event_t dummy;
    int cleared_count = 0;
    
    // 清空队列中所有待处理的事件
    while (xQueueReceive(g_event_queue, &dummy, 0) == pdTRUE) {
        cleared_count++;
    }
    
    if (cleared_count > 0) {
        ESP_LOGI(TAG, "🗑️ Cleared %d pending trigger events from queue", cleared_count);
    }
}
