/*
 * Emotion Manager - 情绪管理器
 * 负责管理Blinko电子宠物的情绪状态
 */

#ifndef EMOTION_MANAGER_H
#define EMOTION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 情绪区间定义 ==================== */
typedef enum {
    EMOTION_ZONE_S = 0x00,  // S区间：亢奋 (Excited)
    EMOTION_ZONE_A = 0x01,  // A区间：兴奋 (Energetic)
    EMOTION_ZONE_B = 0x02,  // B区间：积极 (Positive)
    EMOTION_ZONE_C = 0x03,  // C区间：日常 (Normal)
    EMOTION_ZONE_D = 0x04,  // D区间：消极 (Negative)
    EMOTION_ZONE_MAX
} emotion_zone_t;

/* ==================== 情绪状态结构 ==================== */
typedef struct {
    emotion_zone_t current_zone;      // 当前情绪区间
    uint32_t zone_enter_time;         // 进入当前区间的时间戳 (ms)
    uint32_t total_interactions;      // 总互动次数
    uint8_t zone_interaction_count;   // 当前区间互动次数
    bool is_initialized;              // 是否已初始化
} emotion_state_t;

/* ==================== 情绪统计信息 ==================== */
typedef struct {
    uint32_t zone_duration[EMOTION_ZONE_MAX];  // 各区间停留时长(秒)
    uint32_t zone_count[EMOTION_ZONE_MAX];     // 各区间进入次数
    uint32_t last_change_time;                 // 上次切换时间戳
} emotion_statistics_t;

/* ==================== 公共API ==================== */

/**
 * @brief 初始化情绪管理器
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t emotion_manager_init(void);

/**
 * @brief 设置情绪区间
 * @param zone 目标情绪区间
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 无效参数
 */
esp_err_t emotion_manager_set_zone(emotion_zone_t zone);

/**
 * @brief 获取当前情绪区间
 * @return 当前情绪区间
 */
emotion_zone_t emotion_manager_get_zone(void);

/**
 * @brief 获取完整的情绪状态
 * @param state 输出参数，存储情绪状态
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数为空
 */
esp_err_t emotion_manager_get_state(emotion_state_t *state);

/**
 * @brief 增加互动计数
 * @return ESP_OK 成功
 */
esp_err_t emotion_manager_increment_interaction(void);

/**
 * @brief 获取情绪统计信息
 * @param stats 输出参数，存储统计信息
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数为空
 */
esp_err_t emotion_manager_get_statistics(emotion_statistics_t *stats);

/**
 * @brief 重置情绪统计信息
 * @return ESP_OK 成功
 */
esp_err_t emotion_manager_reset_statistics(void);

/**
 * @brief 保存情绪状态到NVS
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t emotion_manager_save_to_nvs(void);

/**
 * @brief 从NVS加载情绪状态
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t emotion_manager_load_from_nvs(void);

/**
 * @brief 获取情绪区间名称
 * @param zone 情绪区间
 * @return 区间名称字符串
 */
const char* emotion_manager_get_zone_name(emotion_zone_t zone);

/**
 * @brief 获取情绪区间描述
 * @param zone 情绪区间
 * @return 区间描述字符串
 */
const char* emotion_manager_get_zone_description(emotion_zone_t zone);

#ifdef __cplusplus
}
#endif

#endif // EMOTION_MANAGER_H
