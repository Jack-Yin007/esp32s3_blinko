/*
 * Pet Robot Configuration Header
 * 宠物机器人配置文件
 * 
 * 本文件包含所有硬件配置和系统参数
 */

#ifndef PET_CONFIG_H
#define PET_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

/*=============================================================================
 * 系统配置 (System Configuration)
 *=============================================================================*/
#define PET_ROBOT_VERSION           "1.0.0"
#define PET_ROBOT_NAME              "SmartPet"
#define PET_LOG_TAG                 "PetRobot"

// 任务优先级定义
#define TASK_PRIORITY_HIGH          5
#define TASK_PRIORITY_MEDIUM        3  
#define TASK_PRIORITY_LOW           1

// 任务堆栈大小
#define TASK_STACK_SIZE_LARGE       (8 * 1024)
#define TASK_STACK_SIZE_MEDIUM      (4 * 1024)
#define TASK_STACK_SIZE_SMALL       (2 * 1024)

/*=============================================================================
 * GPIO 引脚配置 (GPIO Pin Configuration)
 *=============================================================================*/

// 传感器输入引脚
#define PIN_TOUCH_SENSOR            GPIO_NUM_5      // 触摸传感器
#define PIN_MOTION_SENSOR           GPIO_NUM_4      // 人体感应传感器 
// #define PIN_BUTTON_YES              GPIO_NUM_7      // 已移除：GPIO7 现用于天问语音模块中断
#define PIN_BUTTON_NO               GPIO_NUM_6      // 取消按钮
#define PIN_VOICE_INTERRUPT         GPIO_NUM_7      // 天问语音唤醒模块中断 (2025-12-24 硬件变更)

// 执行器输出引脚  
#define PIN_SERVO_HEAD_NOD          GPIO_NUM_1      // 头部点头舵机
#define PIN_SERVO_HEAD_LEFT         GPIO_NUM_2      // 头部左转舵机
#define PIN_SERVO_HEAD_RIGHT        GPIO_NUM_42     // 头部右转舵机

// LED 指示灯
#define PIN_LED_STATUS              GPIO_NUM_8      // 状态指示LED

#define PIN_SDMMC_CLK               GPIO_NUM_16
#define PIN_SDMMC_DAT0              GPIO_NUM_17
#define PIN_SDMMC_CMD               GPIO_NUM_38

#define PIN_NFC_TXD                 GPIO_NUM_48
#define PIN_NFC_RXD                 GPIO_NUM_47

/*=============================================================================
 * 简化的配置结构 (Simplified Configuration Structure)
 *=============================================================================*/

// 简化的配置结构体（用于逐步迁移）
typedef struct {
    struct {
        uint8_t touch_pin;
        uint8_t pir_sensor_pin;
        uint8_t led_pin;
        uint8_t buzzer_pin;
    } gpio;
} pet_config_t;

/*=============================================================================
 * 函数声明 (Function Declarations)
 *=============================================================================*/

/**
 * @brief 初始化宠物配置
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t pet_config_init(void);

/**
 * @brief 获取宠物配置
 * @return 配置结构体指针
 */
const pet_config_t* pet_config_get(void);

#endif // PET_CONFIG_H
