/*
 * Action Table - 动作组合表
 * 定义各情绪区间的动作组合
 */

#ifndef ACTION_TABLE_H
#define ACTION_TABLE_H

#include <stdint.h>
#include "esp_err.h"
#include "emotion_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 动作元素ID定义 ==================== */

/* LED模式ID - 与led_enhanced.c实现一致 */
typedef enum {
    LED_PATTERN_OFF = 0,           // 关闭
    LED_PATTERN_SOLID_RED,         // 纯红色 (0x01)
    LED_PATTERN_SOLID_GREEN,       // 纯绿色 (0x02)
    LED_PATTERN_SOLID_BLUE,        // 纯蓝色 (0x03)
    LED_PATTERN_SOLID_YELLOW,      // 纯黄色 (0x04)
    LED_PATTERN_BLINK_SLOW,        // 慢速闪烁 (0x05)
    LED_PATTERN_BLINK_FAST,        // 快速闪烁 (0x06)
    LED_PATTERN_WAVE,              // 波浪效果 (0x07)
    LED_PATTERN_MAX                // 最大值 (8)
} led_pattern_id_t;

/* 震动模式ID */
typedef enum {
    VIB_PATTERN_OFF = 0,           // 关闭
    VIB_PATTERN_SHORT_PULSE,       // 短脉冲 (100ms)
    VIB_PATTERN_LONG_PULSE,        // 长脉冲 (500ms)
    VIB_PATTERN_DOUBLE_PULSE,      // 双脉冲
    VIB_PATTERN_TRIPLE_PULSE,      // 三脉冲
    VIB_PATTERN_CONTINUOUS_WEAK,   // 持续弱震动
    VIB_PATTERN_CONTINUOUS_STRONG, // 持续强震动
    VIB_PATTERN_WAVE,              // 波浪震动
    VIB_PATTERN_HEARTBEAT,         // 心跳震动
    VIB_PATTERN_MAX
} vibration_pattern_id_t;

/* 音效ID */
typedef enum {
    SOUND_EFFECT_OFF = 0,          // 无音效
    SOUND_EFFECT_HAPPY,            // 开心音效
    SOUND_EFFECT_EXCITED,          // 兴奋音效
    SOUND_EFFECT_CURIOUS,          // 好奇音效
    SOUND_EFFECT_CALM,             // 平静音效
    SOUND_EFFECT_SAD,              // 悲伤音效
    SOUND_EFFECT_ALERT,            // 警报音效
    SOUND_EFFECT_GREETING,         // 问候音效
    SOUND_EFFECT_GOODBYE,          // 告别音效
    SOUND_EFFECT_MAX
} sound_effect_id_t;

/* ==================== 动作组合结构 ==================== */
typedef struct {
    uint8_t combo_id;              // 组合ID (0-255)
    const char *name;              // 动作名称
    
    // LED控制
    led_pattern_id_t led_pattern;  // LED模式
    
    // 震动控制
    vibration_pattern_id_t vib_pattern; // 震动模式
    
    // 音效控制
    sound_effect_id_t sound_effect;     // 音效
    
    // 点头马达控制 (Excel D/E/F列)
    uint8_t nod_direction;         // 点头方向: 0=停止, 1=正转, 2=反转
    uint8_t nod_speed;             // 点头速度 (0-100)
    uint16_t nod_duration;         // 点头持续时间(ms)

    uint8_t nod_execute_num;       //点头执行的次数
    uint16_t nod_interval_ms;      //点头执行的次数之间的间隔时间
    uint8_t nod_priority;           //点头优先级
    
    // 摇头马达控制 (Excel G/H/I列)
    uint8_t shake_direction;       // 摇头方向: 0=停止, 1=左摇, 2=右摇 4=troggele
    uint8_t shake_speed;           // 摇头速度 (0-100)
    uint16_t shake_duration;       // 摇头持续时间(ms)

    uint8_t shake_execute_num;       //点头执行的次数
    uint16_t shake_interval_ms;      //点头执行的次数之间的间隔时间
    uint8_t shake_priority;           //点头优先级
    
    uint16_t duration_ms;          // 总持续时间 (毫秒)
    const char *description;       // 描述

    uint8_t ear_left_direction;
    uint8_t ear_left_priority;
    uint8_t ear_left_repeat_num;
    uint8_t ear_right_direction;
    uint8_t ear_right_priority;
    uint8_t ear_right_repeat_num;
} action_combo_t;

/* ==================== 触发条件枚举 ==================== */
typedef enum {
    TRIGGER_CONDITION_TOUCH = 0,   // 触摸触发
    TRIGGER_CONDITION_APPROACH,    // 接近触发
    TRIGGER_CONDITION_VOICE,       // 语音触发
    TRIGGER_CONDITION_AUTO,        // 自动触发
    TRIGGER_CONDITION_MAX
} trigger_condition_t;

/* ==================== 公共API ==================== */

/**
 * @brief 初始化动作组合表
 * @return ESP_OK 成功
 */
esp_err_t action_table_init(void);

/**
 * @brief 获取指定情绪区间的动作组合
 * @param zone 情绪区间
 * @param combo_id 组合ID
 * @return 动作组合指针，失败返回NULL
 */
const action_combo_t* action_table_get_combo(emotion_zone_t zone, uint8_t combo_id);

/**
 * @brief 根据组合ID获取动作组合（简化版本）
 * @param combo_id 动作组合ID
 * @return 动作组合指针，失败返回NULL
 */
const action_combo_t* action_table_get_combo_by_id(uint8_t combo_id);

/**
 * @brief 获取指定情绪区间的动作组合数量
 * @param zone 情绪区间
 * @return 动作组合数量
 */
uint8_t action_table_get_combo_count(emotion_zone_t zone);

/**
 * @brief 根据触发条件获取随机动作组合
 * @param zone 情绪区间
 * @param condition 触发条件
 * @return 动作组合指针，失败返回NULL
 */
const action_combo_t* action_table_get_random_combo(emotion_zone_t zone, trigger_condition_t condition);

/**
 * @brief 获取所有动作组合列表
 * @param zone 情绪区间
 * @param combo_list 输出参数，存储组合列表指针
 * @param count 输出参数，存储组合数量
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 参数无效
 */
esp_err_t action_table_get_all_combos(emotion_zone_t zone, const action_combo_t **combo_list, uint8_t *count);

/**
 * @brief 打印动作组合信息（调试用）
 * @param combo 动作组合
 */
void action_table_print_combo(const action_combo_t *combo);

/**
 * @brief 打印所有动作组合（调试用）
 * @param zone 情绪区间
 */
void action_table_print_all(emotion_zone_t zone);

#ifdef __cplusplus
}
#endif

#endif // ACTION_TABLE_H
