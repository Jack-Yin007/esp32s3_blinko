/**
 * @file aw9535.h
 * @brief AW9535 GPIO扩展器驱动头文件
 * 
 * AW9535是一款16位I2C接口的GPIO扩展器，支持GPIO输入输出和中断功能
 * - 16个GPIO引脚（P0_0 ~ P0_7, P1_0 ~ P1_7）
 * - I2C接口，7位地址可配置
 * - 支持中断功能（上升沿/下降沿/双边沿）
 * - 每个引脚可独立配置为输入或输出
 * - 输出驱动电流可达8mA
 * 
 * 硬件连接：
 * - SDA: ESP32-S3 GPIO2
 * - SCL: ESP32-S3 GPIO1
 * - INT: ESP32-S3 GPIO45 (可选)
 */

#ifndef AW9535_H
#define AW9535_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AW9535寄存器地址 */
#define AW9535_REG_INPUT_P0         0x00    // 端口0输入寄存器
#define AW9535_REG_INPUT_P1         0x01    // 端口1输入寄存器
#define AW9535_REG_OUTPUT_P0        0x02    // 端口0输出寄存器
#define AW9535_REG_OUTPUT_P1        0x03    // 端口1输出寄存器
#define AW9535_REG_INT_P0           0x04    // 端口0中断状态寄存器
#define AW9535_REG_INT_P1           0x05    // 端口1中断状态寄存器
#define AW9535_REG_CONFIG_P0        0x06    // 端口0配置寄存器(1=输入,0=输出)
#define AW9535_REG_CONFIG_P1        0x07    // 端口1配置寄存器(1=输入,0=输出)

/* 默认I2C配置 */
#define AW9535_I2C_ADDR_DEFAULT     0x20    // 默认I2C地址(ADDR引脚接VDD)
#define AW9535_I2C_TIMEOUT_MS       1000    // I2C超时时间

/**
 * @brief GPIO引脚定义（0-15）
 */
typedef enum {
    AW9535_PIN_P0_0 = 0,   // 端口0，引脚0  pin_num: P00  0
    AW9535_PIN_P0_1,       // 端口0，引脚1  pin_num: P01  1
    AW9535_PIN_P0_2,       // 端口0，引脚2  pin_num: P02  2
    AW9535_PIN_P0_3,       // 端口0，引脚3  pin_num: P03  3
    AW9535_PIN_P0_4,       // 端口0，引脚4  pin_num: P04  4
    AW9535_PIN_P0_5,       // 端口0，引脚5  pin_num: P05  5
    AW9535_PIN_P0_6,       // 端口0，引脚6  pin_num: P06  6
    AW9535_PIN_P0_7,       // 端口0，引脚7  pin_num: P07  7
    AW9535_PIN_P1_0,       // 端口1，引脚0  pin_num: P10  8
    AW9535_PIN_P1_1,       // 端口1，引脚1  pin_num: P11  9
    AW9535_PIN_P1_2,       // 端口1，引脚2  pin_num: P12  10
    AW9535_PIN_P1_3,       // 端口1，引脚3  pin_num: P13  11
    AW9535_PIN_P1_4,       // 端口1，引脚4  pin_num: P14  12
    AW9535_PIN_P1_5,       // 端口1，引脚5  pin_num: P15  13
    AW9535_PIN_P1_6,       // 端口1，引脚6  pin_num: P16  14
    AW9535_PIN_P1_7,       // 端口1，引脚7  pin_num: P17  15
    AW9535_PIN_MAX
} aw9535_pin_t;

/**
 * @brief GPIO模式
 */
typedef enum {
    AW9535_MODE_OUTPUT = 0,  // 输出模式
    AW9535_MODE_INPUT = 1    // 输入模式
} aw9535_mode_t;

/**
 * @brief GPIO电平
 */
typedef enum {
    AW9535_LEVEL_LOW = 0,    // 低电平
    AW9535_LEVEL_HIGH = 1    // 高电平
} aw9535_level_t;

/**
 * @brief 中断类型
 */
typedef enum {
    AW9535_INT_DISABLE = 0,  // 禁用中断
    AW9535_INT_RISING,       // 上升沿中断
    AW9535_INT_FALLING,      // 下降沿中断
    AW9535_INT_BOTH_EDGE     // 双边沿中断
} aw9535_int_type_t;

/**
 * @brief I2C配置结构
 */
typedef struct {
    i2c_port_t i2c_port;     // I2C端口号
    uint8_t dev_addr;        // 设备I2C地址
    gpio_num_t sda_io;       // SDA引脚
    gpio_num_t scl_io;       // SCL引脚
    uint32_t freq_hz;        // I2C频率
} aw9535_i2c_config_t;

/**
 * @brief AW9535配置结构
 */
typedef struct {
    aw9535_i2c_config_t i2c_cfg;  // I2C配置
    gpio_num_t int_io;            // 中断引脚（可选，-1表示不使用）
} aw9535_config_t;

/**
 * @brief 中断回调函数类型
 * @param pin 触发中断的引脚
 * @param level 当前引脚电平
 * @param user_data 用户数据
 */
typedef void (*aw9535_isr_t)(aw9535_pin_t pin, aw9535_level_t level, void *user_data);

/**
 * @brief 初始化AW9535驱动
 * @param config 配置参数
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL I2C初始化失败
 */
esp_err_t aw9535_init(const aw9535_config_t *config);

/**
 * @brief 反初始化AW9535驱动
 * @return 
 *     - ESP_OK 成功
 */
esp_err_t aw9535_deinit(void);

/**
 * @brief 复位AW9535设备（软件复位）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL 失败
 */
esp_err_t aw9535_reset(void);

/**
 * @brief 设置GPIO引脚模式
 * @param pin GPIO引脚
 * @param mode 模式（输入/输出）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_set_mode(aw9535_pin_t pin, aw9535_mode_t mode);

/**
 * @brief 批量设置GPIO引脚模式（端口0）
 * @param mode_mask 模式掩码（bit=1为输入，bit=0为输出）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_set_mode_port0(uint8_t mode_mask);

/**
 * @brief 批量设置GPIO引脚模式（端口1）
 * @param mode_mask 模式掩码（bit=1为输入，bit=0为输出）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_set_mode_port1(uint8_t mode_mask);

/**
 * @brief 设置GPIO输出电平
 * @param pin GPIO引脚
 * @param level 电平（高/低）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_set_level(aw9535_pin_t pin, aw9535_level_t level);

/**
 * @brief 批量设置GPIO输出电平（端口0）
 * @param level_mask 电平掩码（bit=1为高电平，bit=0为低电平）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_set_level_port0(uint8_t level_mask);

/**
 * @brief 批量设置GPIO输出电平（端口1）
 * @param level_mask 电平掩码（bit=1为高电平，bit=0为低电平）
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_set_level_port1(uint8_t level_mask);

/**
 * @brief 读取GPIO输入电平
 * @param pin GPIO引脚
 * @param level 输出参数，返回电平值
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_get_level(aw9535_pin_t pin, aw9535_level_t *level);

/**
 * @brief 批量读取GPIO输入电平（端口0）
 * @param level_mask 输出参数，返回电平掩码
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_get_level_port0(uint8_t *level_mask);

/**
 * @brief 批量读取GPIO输入电平（端口1）
 * @param level_mask 输出参数，返回电平掩码
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_get_level_port1(uint8_t *level_mask);

/**
 * @brief 配置GPIO中断
 * @param pin GPIO引脚
 * @param int_type 中断类型
 * @param isr_handler 中断回调函数
 * @param user_data 用户数据
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL 配置失败
 */
esp_err_t aw9535_set_interrupt(aw9535_pin_t pin, aw9535_int_type_t int_type, 
                                aw9535_isr_t isr_handler, void *user_data);

/**
 * @brief 启用GPIO中断
 * @param pin GPIO引脚
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 */
esp_err_t aw9535_enable_interrupt(aw9535_pin_t pin);

/**
 * @brief 禁用GPIO中断
 * @param pin GPIO引脚
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 */
esp_err_t aw9535_disable_interrupt(aw9535_pin_t pin);

/**
 * @brief 读取中断状态
 * @param int_status0 输出参数，端口0中断状态
 * @param int_status1 输出参数，端口1中断状态
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_get_interrupt_status(uint8_t *int_status0, uint8_t *int_status1);

/**
 * @brief 清除中断状态
 * @return 
 *     - ESP_OK 成功
 *     - ESP_FAIL I2C通信失败
 */
esp_err_t aw9535_clear_interrupt(void);

/**
 * @brief 翻转GPIO输出电平
 * @param pin GPIO引脚
 * @return 
 *     - ESP_OK 成功
 *     - ESP_ERR_INVALID_ARG 参数错误
 *     - ESP_FAIL 操作失败
 */
esp_err_t aw9535_toggle_level(aw9535_pin_t pin);

/**
 * @brief 诊断函数：打印AW9535和中断配置状态
 * 
 * 打印AW9535所有寄存器、P1.7引脚状态、ESP32 GPIO45状态等调试信息
 */
void aw9535_debug_print_status(void);

#ifdef __cplusplus
}
#endif

#endif // AW9535_H
