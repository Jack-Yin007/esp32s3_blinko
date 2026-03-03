/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef AUDIO_H
#define AUDIO_H

/* Includes */
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "host/ble_gatt.h"

/* Audio Configuration */
#define I2S_SAMPLE_RATE         8000   // 降低到8kHz以匹配BLE传输能力
#define I2S_BITS_PER_SAMPLE     16
#define I2S_CHANNELS            1

/* I2S Buffer Size Options - Choose based on your needs */
// 实时语音通话 (低延迟)
// #define I2S_READ_LEN         (256)    // 8ms数据，128samples，延迟低但开销大

// 平衡模式 (推荐) - 为BLE音频优化
#define I2S_READ_LEN            (512)    // 16ms数据，256samples，最佳BLE传输效率

// 音乐录制 (高质量)
// #define I2S_READ_LEN         (1024)   // 32ms数据，512samples，质量高但延迟大

#define AUDIO_BUFFER_SIZE       (244)    // BLE packet size (MTU 256 - 12 bytes overhead)

/* Calculated Audio Parameters */
#define SAMPLES_PER_READ        (I2S_READ_LEN / 2)                    // 每次读取的样本数
#define READ_DURATION_MS        ((SAMPLES_PER_READ * 1000) / I2S_SAMPLE_RATE)  // 读取时长(ms)
#define BYTES_PER_SECOND        (I2S_SAMPLE_RATE * 2 * I2S_CHANNELS) // 每秒字节数

/* ADPCM Compression */
#define AUDIO_USE_ADPCM         1       // 启用ADPCM压缩 (4:1压缩比)
#define ADPCM_BUFFER_SIZE       (I2S_READ_LEN / 4)  // ADPCM压缩后的缓冲区大小

/* Enhanced Audio Processing Parameters - Optimized for INMP441 */
// 产品板专用增益配置：平衡信号强度和动态范围
#define AUDIO_GAIN_MULTIPLIER   10.0f   // 麦克风增益倍数 (降低到10x避免削波)
#define AUDIO_NOISE_THRESHOLD   80      // 噪声阈值 (适配10x增益)
#define AUDIO_AGC_ENABLED       1       // 启用简化AGC提升动态范围
#define AUDIO_HPF_ENABLED       1       // 启用100Hz高通滤波器
#define AUDIO_DC_FILTER_ALPHA   0.995f  // DC阻断滤波器系数
#define AUDIO_PREEMPHASIS       0       // 禁用预加重滤波器（会改变音色和速度感）

/* GPIO pins for INMP441 Digital Microphone - 匹配开发板I2S专用引脚 */
#define I2S_SCK_IO              40      // Serial Clock (BCLK) - 使用专用I2S_BCK引脚
#define I2S_WS_IO               39      // Word Select (LRCLK) - 使用专用I2S_WS引脚
#define I2S_SD_IO               15      // Serial Data (DIN) - 使用专用I2S_DI引脚
// Note: L/R pin should be connected to GND for left channel
// 🔌 硬件连接：INMP441.SCK→GPIO40, INMP441.WS→GPIO39, INMP441.SD→GPIO15

/* Audio GATT Service UUIDs */
#define AUDIO_SERVICE_UUID      0x1910
#define AUDIO_CHAR_UUID         0x2A56
#define AUDIO_DESCR_UUID        0x2902

/* Buffer Size Presets */
typedef enum {
    AUDIO_BUFFER_LOW_LATENCY = 256,    // 8ms - 实时通话
    AUDIO_BUFFER_BALANCED = 512,       // 16ms - 平衡模式 (推荐)  
    AUDIO_BUFFER_HIGH_QUALITY = 1024   // 32ms - 高质量录制
} audio_buffer_size_t;

/* Task handle for external access (e.g., self-test) */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern TaskHandle_t audio_task_handle;

/* Public function declarations */
esp_err_t audio_init(void);
esp_err_t audio_start_recording(void);
esp_err_t audio_stop_recording(void);
esp_err_t audio_set_gain(uint8_t gain_level);       // 设置音频增益 (1-8)
esp_err_t audio_set_buffer_size(audio_buffer_size_t buffer_size);  // 设置缓冲区大小
void audio_set_notify_enable(bool enable);
void audio_set_connection_info(uint16_t conn_handle, uint16_t attr_handle);
bool audio_is_recording(void);

/* BLE Audio Service functions */
int audio_gatt_svc_init(void);
int audio_gatt_char_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

#endif // AUDIO_H