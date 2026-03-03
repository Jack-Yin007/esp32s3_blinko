/**
 * @file custom_i2s_writer.h
 * @brief Custom I2S Writer Element for ESP-ADF Pipeline
 * 
 * 设计目标：
 * - 使用现有的I2S TX handle（i2s_audio_get_tx_handle()），不销毁RX channel
 * - 实现ESP-ADF Audio Element接口，与Pipeline无缝集成
 * - 在元素关闭时发送静音帧清空DMA缓冲区（避免回声）
 * - 保持MAX98357A控制（通过AW9535的P1.2/P1.3引脚）
 */

#ifndef CUSTOM_I2S_WRITER_H
#define CUSTOM_I2S_WRITER_H

#include "audio_element.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom I2S Writer配置结构体
 */
typedef struct {
    uint32_t sample_rate;        ///< 采样率（Hz）
    uint16_t channels;           ///< 声道数（1=单声道, 2=立体声）
    uint16_t bits_per_sample;    ///< 位深度（16）
    bool use_existing_handle;    ///< 使用现有TX handle还是创建新的（true=使用现有）
    bool pause_silent_tx;        ///< 播放前暂停Silent TX任务
} custom_i2s_writer_cfg_t;

/**
 * @brief 默认配置宏
 */
#define CUSTOM_I2S_WRITER_DEFAULT_CFG() {   \
    .sample_rate = 8000,                    \
    .channels = 2,                          \
    .bits_per_sample = 16,                  \
    .use_existing_handle = true,            \
    .pause_silent_tx = true,                \
}

/**
 * @brief 初始化Custom I2S Writer Element
 * 
 * @param cfg 配置结构体指针
 * @return audio_element_handle_t 成功返回element句柄，失败返回NULL
 */
audio_element_handle_t custom_i2s_writer_init(custom_i2s_writer_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif // CUSTOM_I2S_WRITER_H
