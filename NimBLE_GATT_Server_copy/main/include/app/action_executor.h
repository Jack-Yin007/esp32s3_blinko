/**
 * @file action_executor.h
 * @brief 动作执行器接口 - 马达动作组合控制
 * 
 * 提供高级动作组合接口，协调多个马达同时运行
 */

#ifndef ACTION_EXECUTOR_H
#define ACTION_EXECUTOR_H

#include <stdint.h>
#include "esp_err.h"
#include "drivers/hardware_interface.h"
#include "app/action_table.h"

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_ANGLE   45
#define UNIT_ANGLE  15
//#define HIGH_DELTA_TIME              60
#define HIGH_DELTA_TIME              90
#define MID_DELTA_TIME               150
#define QUICK_UNIT_ANGLE_TIME   100
#define MID_UNIT_ANGLE_TIME     130     
#define SLOW_UNIT_ANGLE_TIME    700     //需要测量  

typedef struct {
    // 点头参数
    motor_direction_t nod_dir;
    uint8_t nod_speed;
    uint16_t nod_duration_ms;
    uint8_t nod_execute_num;   
    uint16_t nod_interval_ms;
    uint8_t nod_priority;
    // 摇头参数
    motor_direction_t shake_dir;
    uint8_t shake_speed;
    uint16_t shake_duration_ms;
    uint8_t shake_execute_num;   
    uint16_t shake_interval_ms;
    uint8_t shake_priority;

    motor_direction_t ear_left_dir;
    uint8_t ear_left_priority;
    uint8_t ear_left_execute_num;
    motor_direction_t ear_right_dir;
    uint8_t ear_right_priority;
    uint8_t ear_right_execute_num;


} motor_action_config_t;

/**
 * @brief 初始化动作执行器
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t action_executor_init(void);

/**
 * @brief 执行马达动作
 * @param nod_dir 点头方向
 * @param nod_speed 点头速度 (0-100)
 * @param nod_duration 点头持续时间(ms)
 * @param shake_dir 摇头方向
 * @param shake_speed 摇头速度 (0-100)
 * @param shake_duration 摇头持续时间(ms)
 * @return ESP_OK 成功
 */
esp_err_t action_executor_run_motor_action(
    motor_direction_t nod_dir, uint8_t nod_speed, uint16_t nod_duration,
    motor_direction_t shake_dir, uint8_t shake_speed, uint16_t shake_duration);

esp_err_t load_action_to_motor(
    motor_direction_t nod_dir, uint8_t nod_speed, uint16_t nod_duration, uint8_t nod_execute_num , uint16_t nod_interval_ms, uint8_t nod_priority,  

    motor_direction_t shake_dir, uint8_t shake_speed, uint16_t shake_duration,  uint8_t shake_execute_num , uint16_t shake_interval_ms, uint8_t shake_priority,
    motor_direction_t ear_left_dir,  uint8_t ear_left_priority, uint8_t ear_left_execute_num,
    motor_direction_t ear_right_dir, uint8_t ear_right_priority, uint8_t ear_right_execute_num );

/**
 * @brief 从动作表执行动作组合
 * @param combo_id 动作组合ID
 * @return ESP_OK 成功
 */
esp_err_t action_executor_execute_combo(uint8_t combo_id);

/**
 * @brief 停止当前动作
 * @return ESP_OK 成功
 */
esp_err_t action_executor_stop(void);

/**
 * @brief 检查是否有动作正在执行
 * @return true 正在执行，false 空闲
 */
bool action_executor_is_active(void);

/**
 * @brief 获取队列中待执行的动作数量
 * @return 待执行动作数量
 */
uint32_t action_executor_get_queue_count(void);

esp_err_t motor_control(const motor_action_config_t *action);

#ifdef __cplusplus
}
#endif

#endif // ACTION_EXECUTOR_H
