/**
 * @file max98357a.h
 * @brief MAX98357A I2S音频DAC驱动头文件
 * 
 * 硬件连接：
 * MAX98357A端:          ESP32-S3端:
 * ├─ BCLK (位时钟)  ←── I2S_BCLK (GPIO_40)
 * ├─ LRCLK (字时钟) ←── I2S_WS (GPIO_39)
 * ├─ DIN (数据)     ←── I2S_DOUT (GPIO_41)
 * ├─ GAIN1          ──→ AW9535 Pin13 (P1.5增益控制1) - 2025-12-24 硬件变更
 * ├─ GAIN2          ──→ AW9535 Pin14 (P1.6增益控制2) - 2025-12-24 硬件变更
 * ├─ SD_MODE        ──→ AW9535 Pin15 (P1.7关断控制) - 2025-12-24 硬件变更
 * └─ VDD/GND        ──→ 电源
 */

#ifndef MAX98357A_H
#define MAX98357A_H

#include "esp_err.h"
#include "driver/i2s_std.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 增益等级定义 ==================== */
typedef enum {
    MAX98357A_GAIN_3DB = 0,   // +3dB (GAIN1=0, GAIN2=0) - 最低
    MAX98357A_GAIN_6DB = 1,   // +6dB (GAIN1=0, GAIN2=1)
    MAX98357A_GAIN_9DB = 2,   // +9dB (GAIN1=1, GAIN2=0)
    MAX98357A_GAIN_12DB = 3,  // +12dB (GAIN1=1, GAIN2=1) - 最高
} max98357a_gain_t;

/* ==================== 配置结构体 ==================== */
typedef struct {
    // I2S引脚配置
    int bclk_io;        // 位时钟引脚
    int ws_io;          // 字时钟引脚
    int dout_io;        // 数据输出引脚
    
    // GPIO扩展器引脚（控制GAIN和SD_MODE）
    uint8_t gain1_pin;  // GAIN1控制引脚（AW9535）
    uint8_t gain2_pin;  // GAIN2控制引脚（AW9535）
    uint8_t sd_pin;     // SD_MODE控制引脚（AW9535）
    
    // I2S配置
    uint32_t sample_rate;   // 采样率（Hz）
    uint8_t bits_per_sample; // 位深度（16/24/32）
    
    // 初始增益
    max98357a_gain_t default_gain;
} max98357a_config_t;

/* ==================== 默认配置 ==================== */
#define MAX98357A_DEFAULT_CONFIG() {            \
    .bclk_io = 40,                              \
    .ws_io = 39,                                \
    .dout_io = 41,                              \
    .gain1_pin = AW9535_PIN_P1_5,  /* Pin13 */ \
    .gain2_pin = AW9535_PIN_P1_6,  /* Pin14 */ \
    .sd_pin = AW9535_PIN_P1_7,     /* Pin15 */ \
    .sample_rate = 8000,                        \
    .bits_per_sample = 16,                      \
    .default_gain = MAX98357A_GAIN_9DB,        \
}

/* ==================== API函数 ==================== */

/**
 * @brief 初始化MAX98357A
 * @param config 配置参数
 * @return ESP_OK 成功
 */
esp_err_t max98357a_init(const max98357a_config_t *config);

/**
 * @brief 关闭MAX98357A
 * @return ESP_OK 成功
 */
esp_err_t max98357a_deinit(void);

/**
 * @brief 启用音频输出（SD_MODE = HIGH）
 * @return ESP_OK 成功
 */
esp_err_t max98357a_enable(void);

/**
 * @brief 禁用音频输出（SD_MODE = LOW，关断模式）
 * @return ESP_OK 成功
 */
esp_err_t max98357a_disable(void);

/**
 * @brief 设置增益
 * @param gain 增益等级
 * @return ESP_OK 成功
 */
esp_err_t max98357a_set_gain(max98357a_gain_t gain);

/**
 * @brief 写入PCM音频数据
 * @param data PCM数据缓冲区
 * @param size 数据大小（字节）
 * @param[out] bytes_written 实际写入字节数
 * @param timeout_ms 超时时间（毫秒）
 * @return ESP_OK 成功
 */
esp_err_t max98357a_write_data(const void *data, size_t size, 
                               size_t *bytes_written, uint32_t timeout_ms);

/**
 * @brief 播放单音调（测试用）
 * @param frequency 频率（Hz）
 * @param duration_ms 持续时间（毫秒）
 * @return ESP_OK 成功
 */
esp_err_t max98357a_play_tone(uint32_t frequency, uint32_t duration_ms);

/**
 * @brief 获取当前增益
 * @return 当前增益等级
 */
max98357a_gain_t max98357a_get_gain(void);

/**
 * @brief 检查是否已初始化
 * @return true 已初始化
 */
bool max98357a_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // MAX98357A_H
