/*
 * Trigger Detector - 触发检测器
 * 负责检测用户触发事件（触摸/接近/语音）
 */

#ifndef TRIGGER_DETECTOR_H
#define TRIGGER_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 触发类型定义 ==================== */
typedef enum {
    TRIGGER_TYPE_TOUCH    = 0x01,  // 触摸触发
    TRIGGER_TYPE_APPROACH = 0x02,  // 接近触发
    TRIGGER_TYPE_VOICE    = 0x03,  // 语音触发
    TRIGGER_TYPE_AUTO     = 0x04,  // 自动触发（定时）
    TRIGGER_TYPE_MAX
} trigger_type_t;

/* ==================== 触发事件结构 ==================== */
typedef struct {
    trigger_type_t type;           // 触发类型
    uint32_t timestamp;            // 触发时间戳（毫秒）
    uint8_t intensity;             // 触发强度（0-100）
    void *user_data;               // 用户数据
} trigger_event_t;

/* ==================== 触发回调函数类型 ==================== */
/**
 * @brief 触发事件回调函数
 * @param event 触发事件
 * @param user_data 用户自定义数据
 */
typedef void (*trigger_callback_t)(const trigger_event_t *event, void *user_data);

/* ==================== 触发检测配置 ==================== */
typedef struct {
    bool touch_enabled;            // 启用触摸检测
    bool approach_enabled;         // 启用接近检测
    bool voice_enabled;            // 启用语音检测
    bool auto_enabled;             // 启用自动触发
    
    uint32_t debounce_ms;          // 去抖动时间（毫秒）
    uint32_t min_interval_ms;      // 最小触发间隔（毫秒）
    uint32_t auto_interval_ms;     // 自动触发间隔（毫秒）
    
    uint8_t touch_threshold;       // 触摸灵敏度阈值（0-100）
    uint8_t voice_threshold;       // 语音触发阈值（0-100）
} trigger_detector_config_t;

/* ==================== 触发统计信息 ==================== */
typedef struct {
    uint32_t touch_count;          // 触摸触发次数
    uint32_t approach_count;       // 接近触发次数
    uint32_t voice_count;          // 语音触发次数
    uint32_t auto_count;           // 自动触发次数
    uint32_t total_count;          // 总触发次数
    uint32_t last_trigger_time;    // 最后触发时间
} trigger_statistics_t;

/* ==================== 公共API ==================== */

/**
 * @brief 初始化触发检测器（使用默认配置）
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t trigger_detector_init(void);

/**
 * @brief 使用自定义配置初始化触发检测器
 * @param config 配置参数
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t trigger_detector_init_with_config(const trigger_detector_config_t *config);

/**
 * @brief 注册触发回调函数
 * @param callback 回调函数
 * @param user_data 用户自定义数据
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t trigger_detector_register_callback(trigger_callback_t callback, void *user_data);

/**
 * @brief 启动触发检测器
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t trigger_detector_start(void);

/**
 * @brief 停止触发检测器
 * @return ESP_OK 成功
 */
esp_err_t trigger_detector_stop(void);

/**
 * @brief 检查触发检测器是否正在运行
 * @return true 运行中，false 已停止
 */
bool trigger_detector_is_running(void);

/**
 * @brief 启用/禁用特定类型的触发检测
 * @param type 触发类型
 * @param enabled true启用，false禁用
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 类型无效
 */
esp_err_t trigger_detector_enable_type(trigger_type_t type, bool enabled);

/**
 * @brief 手动触发事件（用于测试）
 * @param type 触发类型
 * @param intensity 触发强度（0-100）
 * @return ESP_OK 成功
 */
esp_err_t trigger_detector_manual_trigger(trigger_type_t type, uint8_t intensity);

/**
 * @brief 获取触发统计信息
 * @param stats 输出参数，存储统计信息
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数为空
 */
esp_err_t trigger_detector_get_statistics(trigger_statistics_t *stats);

/**
 * @brief 重置触发统计信息
 * @return ESP_OK 成功
 */
esp_err_t trigger_detector_reset_statistics(void);

/**
 * @brief 获取触发类型名称
 * @param type 触发类型
 * @return 类型名称字符串
 */
const char* trigger_detector_get_type_name(trigger_type_t type);

/**
 * @brief 获取当前配置
 * @param config 输出参数，存储配置信息
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数为空
 */
esp_err_t trigger_detector_get_config(trigger_detector_config_t *config);

/**
 * @brief 更新配置
 * @param config 新配置
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数为空
 */
esp_err_t trigger_detector_update_config(const trigger_detector_config_t *config);

/**
 * @brief 清空事件队列（用于防止重复触发）
 * 应在长时间操作（如WAV播放）完成后调用
 */
void trigger_detector_clear_queue(void);

#ifdef __cplusplus
}
#endif

#endif // TRIGGER_DETECTOR_H
