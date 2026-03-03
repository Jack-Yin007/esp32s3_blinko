/**
 * @file touch_ttp223.h
 * @brief TTP223触摸传感器驱动头文件
 * 
 * 硬件: TTP223电容触摸传感器芯片（数字输出）
 * 通道: 
 *   - GPIO4: 额头触摸 (TOUCH_INTN_1)
 *   - GPIO5: 后背触摸 (TOUCH_INTN_2)
 * 
 * 特性:
 *   - 数字输出（触摸=高电平，未触摸=低电平）
 *   - GPIO中断支持（上升沿/下降沿触发）
 *   - 触摸事件回调机制
 *   - 防抖处理（60ms）
 */

#ifndef TOUCH_TTP223_H
#define TOUCH_TTP223_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * 类型定义
 *=============================================================================*/

/**
 * @brief 触摸通道定义
 */
typedef enum {
    TOUCH_CHANNEL_FOREHEAD = 0,  ///< 额头触摸 (GPIO4)
    TOUCH_CHANNEL_BACK     = 1,  ///< 后背触摸 (GPIO5)
    TOUCH_CHANNEL_MAX      = 2   ///< 通道总数
} touch_ttp223_channel_t;

/**
 * @brief 触摸事件类型
 */
typedef enum {
    TOUCH_EVENT_PRESSED,   ///< 触摸按下（上升沿）
    TOUCH_EVENT_RELEASED,  ///< 触摸释放（下降沿）
} touch_ttp223_event_t;

/**
 * @brief 触摸事件数据
 */
typedef struct {
    touch_ttp223_channel_t channel;  ///< 触发的通道
    touch_ttp223_event_t event_type; ///< 事件类型
    uint32_t timestamp_ms;           ///< 事件时间戳（毫秒）
} touch_ttp223_event_data_t;

/**
 * @brief 触摸事件回调函数类型
 * @param event_data 事件数据指针
 */
typedef void (*touch_ttp223_callback_t)(const touch_ttp223_event_data_t* event_data);

/*=============================================================================
 * 公共API
 *=============================================================================*/

/**
 * @brief 初始化TTP223触摸传感器
 * @return ESP_OK 成功，其他值表示错误
 * 
 * 初始化动作:
 * - 配置GPIO4/GPIO5为输入模式
 * - 启用内部下拉电阻
 * - 配置GPIO中断（上升沿+下降沿）
 * - 安装GPIO ISR服务
 */
esp_err_t hw_touch_sensor_init(void);

/**
 * @brief 反初始化触摸传感器
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t hw_touch_sensor_deinit(void);

/**
 * @brief 检测额头触摸状态（轮询方式）
 * @return true=已触摸, false=未触摸
 */
bool hw_touch_sensor_is_forehead_touched(void);

/**
 * @brief 检测后背触摸状态（轮询方式）
 * @return true=已触摸, false=未触摸
 */
bool hw_touch_sensor_is_back_touched(void);

/**
 * @brief 检测任意触摸状态
 * @return true=任意通道被触摸, false=都未触摸
 */
bool hw_touch_sensor_is_any_touched(void);

/**
 * @brief 注册触摸事件回调函数（中断方式）
 * @param callback 回调函数指针
 * @return ESP_OK 成功，其他值表示错误
 * 
 * 注意: 
 * - 回调函数在ISR上下文中执行，应尽快返回
 * - 不能在回调中调用阻塞函数
 * - 建议使用队列或事件组传递事件到应用层
 */
esp_err_t hw_touch_sensor_register_callback(touch_ttp223_callback_t callback);

/**
 * @brief 注销触摸事件回调函数
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t hw_touch_sensor_unregister_callback(void);

/**
 * @brief 启用触摸中断
 * @param channel 触摸通道
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t hw_touch_sensor_enable_interrupt(touch_ttp223_channel_t channel);

/**
 * @brief 禁用触摸中断
 * @param channel 触摸通道
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t hw_touch_sensor_disable_interrupt(touch_ttp223_channel_t channel);

/**
 * @brief 获取触摸通道名称
 * @param channel 触摸通道
 * @return 通道名称字符串
 */
const char* hw_touch_sensor_get_channel_name(touch_ttp223_channel_t channel);

#ifdef __cplusplus
}
#endif

#endif // TOUCH_TTP223_H
