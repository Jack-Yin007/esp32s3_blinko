/**
 * @file aw9535_pinmap.h
 * @brief AW9535 GPIO扩展器引脚映射定义
 * 
 * 本文件定义了AW9535所有16个引脚的功能分配
 * 硬件版本: 2025-12-24
 */

#ifndef AW9535_PINMAP_H
#define AW9535_PINMAP_H

#include "drivers/aw9535.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== Port 0 引脚定义 (P0.0 - P0.7) ==================== */

/** P00 (Pin 0) - 传感器电源控制 */
#define AW9535_PIN_SENSOR_POWER     AW9535_PIN_P0_0

/** P01 (Pin 1) - 马达1中断输入 */
#define AW9535_PIN_MOTOR1_INT       AW9535_PIN_P0_1

/** P02 (Pin 2) - RGB LED 红色通道 */
#define AW9535_PIN_LED_R            AW9535_PIN_P0_2

/** P03 (Pin 3) - RGB LED 绿色通道 */
#define AW9535_PIN_LED_G            AW9535_PIN_P0_3

/** P04 (Pin 4) - RGB LED 蓝色通道 */
#define AW9535_PIN_LED_B            AW9535_PIN_P0_4

/** P05 (Pin 5) - 充电芯片使能 (CW6305 CE) */
#define AW9535_PIN_CHARGE_CE        AW9535_PIN_P0_5

/** P06 (Pin 6) - 充电中断输入 */
#define AW9535_PIN_CHARGE_INT       AW9535_PIN_P0_6

/** P07 (Pin 7) - BUCK降压电路使能 */
#define AW9535_PIN_BUCK_EN          AW9535_PIN_P0_7

/* ==================== Port 1 引脚定义 (P1.0 - P1.7) ==================== */

/** P10 (Pin 8) - 马达2中断3 */
#define AW9535_PIN_MOTOR2_INT3      AW9535_PIN_P1_0

/** P11 (Pin 9) - 马达2中断2 */
#define AW9535_PIN_MOTOR2_INT2      AW9535_PIN_P1_1

/** P12 (Pin 10) - 马达2中断1 */
#define AW9535_PIN_MOTOR2_INT1      AW9535_PIN_P1_2

/** P1.0 (Pin 8) - 马达2中断3 (已在上面定义) */
/** P1.1 (Pin 9) - 马达2中断2 (已在上面定义) */
/** P1.2 (Pin 10) - 马达2中断1 (已在上面定义) */

/** P1.3 (Pin 11) - MAX98357A 增益选择1 */
#define AW9535_PIN_GAIN_SELECT_1    AW9535_PIN_P1_3

/** P1.4 (Pin 12) - MAX98357A 增益选择2 */
#define AW9535_PIN_GAIN_SELECT_2    AW9535_PIN_P1_4

/** P1.5 (Pin 13) - MAX98357A 关断模式 (SD_MODE) */
#define AW9535_PIN_SD_MODE          AW9535_PIN_P1_5

/** P1.6 (Pin 14) - 天问语音模块使能 */
#define AW9535_PIN_TW_EN            AW9535_PIN_P1_6

/** P1.7 (Pin 15) - 马达使能 */
#define AW9535_PIN_MOTOR_EN         AW9535_PIN_P1_7

/* ==================== 引脚功能组 ==================== */

/**
 * @brief 初始化所有AW9535控制引脚
 * @return ESP_OK 成功
 */
esp_err_t aw9535_pinmap_init(void);

#ifdef __cplusplus
}
#endif

#endif // AW9535_PINMAP_H
