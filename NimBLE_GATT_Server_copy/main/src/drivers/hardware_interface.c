/**
 * @file hardware_interface.c
 * @brief 硬件接口实现 (占位符版本)
 * 
 * 只包含尚未实现的硬件接口占位符。
 * 已实现的接口：
 * - LED: led_enhanced.c
 * - 震动: vibrator.c
 * - 音频: audio_player.c
 * - 触摸: touch_sensor.c
 * - PIR: pir_sensor.c
 * - 麦克风: microphone.c
 * 
 * @note 所有函数当前返回默认值，但不执行实际硬件操作
 */

#include "drivers/hardware_interface.h"
#include "esp_log.h"

static const char *TAG = "HW_INTERFACE";

/* ==================== 音频接口 ==================== */
// hw_audio_play_wav() 实现在 audio_player.c

/* ==================== 电源管理占位符 ==================== */

uint8_t hw_battery_get_level(void)
{
    // 返回100表示满电
    return 100;
}

bool hw_battery_is_charging(void)
{
    // 返回false表示未充电
    return false;
}

float hw_battery_get_voltage(void)
{
    // 返回3.7V (典型锂电池电压)
    return 3.7f;
}
