/**
 * @file i2s_audio.h
 * @brief I2S全双工音频系统管理 - MAX98357A + INMP441
 * 
 * 硬件架构：
 * ┌─────────────────────────────────────────────┐
 * │  ESP32-S3 I2S0 (全双工模式)                │
 * │                                             │
 * │  共享时钟信号：                             │
 * │  ├─ BCK  (GPIO40) ──┬───→ MAX98357A.BCLK  │
 * │  │                   └───→ INMP441.SCK     │
 * │  └─ WS   (GPIO39) ──┬───→ MAX98357A.LRCK  │
 * │                      └───→ INMP441.WS      │
 * │                                             │
 * │  独立数据信号：                             │
 * │  ├─ DOUT (GPIO41) ──────→ MAX98357A.DIN   │
 * │  └─ DIN  (GPIO15) ──────← INMP441.SD      │
 * └─────────────────────────────────────────────┘
 * 
 * 特性：
 * - 统一8kHz采样率（DAC双声道 + MIC单声道）
 * - 自动时钟同步（共享BCK/WS）
 * - 支持同时录音和播放
 * - 16位PCM格式
 */

#ifndef I2S_AUDIO_H
#define I2S_AUDIO_H

#include "esp_err.h"
#include "driver/i2s_std.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== I2S全双工配置 ==================== */

/** I2S端口号（共享） */
#define I2S_AUDIO_PORT          I2S_NUM_0

/** 统一采样率（8kHz） */
#define I2S_AUDIO_SAMPLE_RATE   8000        // 8kHz

/** 位深度（16-bit PCM） */
#define I2S_AUDIO_BITS_PER_SAMPLE  I2S_DATA_BIT_WIDTH_16BIT

/** 共享时钟引脚 */
#define I2S_AUDIO_BCK_PIN       GPIO_NUM_40  // 位时钟（共享）
#define I2S_AUDIO_WS_PIN        GPIO_NUM_39  // 字时钟（共享）

/** 独立数据引脚 */
#define I2S_AUDIO_DOUT_PIN      GPIO_NUM_41  // DAC输出 → MAX98357A
#define I2S_AUDIO_DIN_PIN       GPIO_NUM_15  // MIC输入 ← INMP441

/** DMA缓冲配置 */
#define I2S_AUDIO_DMA_BUF_COUNT  3          // 减少到3个缓冲 (从6个)
#define I2S_AUDIO_DMA_BUF_LEN    512       // 512帧 = 64ms @ 8kHz (从1024减半)

/* ==================== 全局句柄（外部访问） ==================== */

/**
 * @brief 获取I2S TX通道句柄（用于MAX98357A播放）
 * @return i2s_chan_handle_t TX通道句柄，未初始化返回NULL
 */
i2s_chan_handle_t i2s_audio_get_tx_handle(void);

/**
 * @brief 获取I2S RX通道句柄（用于INMP441录音）
 * @return i2s_chan_handle_t RX通道句柄，未初始化返回NULL
 */
i2s_chan_handle_t i2s_audio_get_rx_handle(void);

/* ==================== API函数 ==================== */

/**
 * @brief 诊断测试INMP441麦克风（在BLE启动前调用）
 * 
 * 功能：
 * - 读取5次I2S RX数据（每次80ms@8kHz stereo）
 * - 分析音频数据统计信息（零值比例、信号范围、样本值）
 * - 打印前16个样本的十六进制值
 * - 判断麦克风是否正常工作
 * 
 * @note 必须在i2s_audio_init()之后、i2s_audio_enable_rx()之后调用
 * @note 参考EchoEar的audio_diag_test_microphone()实现
 */
void i2s_audio_diag_test_inmp441(void);

/**
 * @brief 初始化I2S全双工音频系统
 * 
 * 同时创建TX和RX通道，配置为：
 * - TX通道：8kHz 双声道（立体声）用于DAC播放
 * - RX通道：8kHz 单声道（左声道）用于MIC录音
 * - 共享BCK/WS时钟，自动同步
 * 
 * @return 
 *     - ESP_OK: 初始化成功
 *     - ESP_ERR_INVALID_STATE: 已经初始化
 *     - ESP_FAIL: 初始化失败
 */
esp_err_t i2s_audio_init(void);

/**
 * @brief 关闭I2S全双工音频系统
 * 
 * 禁用并删除TX和RX通道
 * 
 * @return 
 *     - ESP_OK: 关闭成功
 *     - ESP_ERR_INVALID_STATE: 未初始化
 */
esp_err_t i2s_audio_deinit(void); // 新增：用于彻底卸载I2S驱动

/**
 * @brief 启用TX通道（播放）
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_enable_tx(void);

/**
 * @brief 禁用TX通道（停止播放）
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_disable_tx(void);

/**
 * @brief 启用RX通道（录音）
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_enable_rx(void);

/**
 * @brief 禁用RX通道（停止录音）
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_disable_rx(void);

/**
 * @brief 暂停静音TX任务（用于音频播放）
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_pause_silent_tx(void);

/**
 * @brief 恢复静音TX任务（音频播放完成后）
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_resume_silent_tx(void);

/**
 * @brief 检查I2S系统是否已初始化
 * @return true 已初始化，false 未初始化
 */
bool i2s_audio_is_initialized(void);

/**
 * @brief 获取当前采样率
 * @return 采样率（Hz）
 */
uint32_t i2s_audio_get_sample_rate(void);

/* ==================== WAV播放专用API ==================== */

/**
 * @brief 为WAV播放创建独立的I2S TX通道
 * 
 * 参考ESP-ADF官方例程，每次播放都创建新的I2S通道，
 * 避免与Silent TX任务冲突，确保音频流独立无残留
 * 
 * @param sample_rate 采样率 (8000, 16000, 44100等)
 * @param channels 声道数 (1=单声道, 2=双声道)
 * @param bits_per_sample 位深度 (16)
 * @return I2S TX通道句柄，失败返回NULL
 */
i2s_chan_handle_t i2s_audio_create_wav_tx_channel(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample);

/**
 * @brief 销毁WAV播放专用的I2S TX通道
 * 
 * 彻底释放I2S通道和DMA资源，确保无残留
 * 
 * @param tx_handle I2S TX通道句柄
 * @return ESP_OK 成功
 */
esp_err_t i2s_audio_destroy_wav_tx_channel(i2s_chan_handle_t tx_handle);

/* ==================== DMA Drain API ==================== */

/**
 * @brief 完整的DMA drain - 结合软件等待和硬件复位
 * 
 * 基于ESP32技术手册的实现：
 * 1. 发送静音数据填满整个DMA缓冲区
 * 2. 等待I2S_OUT_EOF中断确认所有数据发送完成
 * 3. 硬件复位TX模块和FIFO
 * 4. 重新启用TX通道
 * 
 * @param tx_handle I2S TX通道句柄
 * @param sample_rate 采样率
 * @param channels 声道数
 * @param bits_per_sample 位深度
 * @return ESP_OK 成功
 */
esp_err_t i2s_drain_tx_completely(i2s_chan_handle_t tx_handle, 
                                  uint32_t sample_rate, 
                                  uint8_t channels, 
                                  uint8_t bits_per_sample);

/**
 * @brief 硬件复位I2S TX模块和FIFO
 * 
 * 基于ESP32技术手册 22.4.2节的寄存器复位机制
 * 
 * @param tx_handle I2S TX通道句柄
 * @return ESP_OK 成功
 */
esp_err_t i2s_tx_fifo_reset(i2s_chan_handle_t tx_handle);

/**
 * @brief 等待TX DMA发送完成（基于中断回调）
 * 
 * @param tx_handle I2S TX通道句柄
 * @return ESP_OK 成功，ESP_ERR_TIMEOUT 超时
 */
esp_err_t i2s_wait_tx_done_with_callback(i2s_chan_handle_t tx_handle);

#ifdef __cplusplus
}
#endif

#endif // I2S_AUDIO_H
