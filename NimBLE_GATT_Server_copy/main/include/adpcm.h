/*
 * IMA ADPCM Encoder/Decoder
 * Compresses 16-bit PCM to 4-bit ADPCM (4:1 compression ratio)
 */
#ifndef ADPCM_H
#define ADPCM_H

#include <stdint.h>
#include <stdbool.h>

/**
 * ADPCM编码器状态
 */
typedef struct {
    int32_t predicted_sample;   // 预测样本值（使用int32_t避免溢出）
    int8_t step_index;          // 步长索引
} adpcm_state_t;

/**
 * 初始化ADPCM状态
 */
void adpcm_init(adpcm_state_t *state);

/**
 * 编码一个16-bit PCM样本到4-bit ADPCM
 * @param sample 输入的16-bit PCM样本
 * @param state ADPCM编码器状态
 * @return 4-bit ADPCM值（存储在uint8_t的低4位）
 */
uint8_t adpcm_encode_sample(int16_t sample, adpcm_state_t *state);

/**
 * 批量编码PCM数据到ADPCM
 * @param pcm_data 输入的16-bit PCM数据数组
 * @param pcm_samples PCM样本数量
 * @param adpcm_data 输出的4-bit ADPCM数据（2个样本打包成1字节）
 * @param state ADPCM编码器状态
 * @return 输出的ADPCM字节数
 */
int adpcm_encode(const int16_t *pcm_data, int pcm_samples, uint8_t *adpcm_data, adpcm_state_t *state);

/**
 * 解码一个4-bit ADPCM样本到16-bit PCM
 * @param adpcm_nibble 4-bit ADPCM值
 * @param state ADPCM解码器状态
 * @return 16-bit PCM样本
 */
int16_t adpcm_decode_sample(uint8_t adpcm_nibble, adpcm_state_t *state);

/**
 * 批量解码ADPCM数据到PCM
 * @param adpcm_data 输入的4-bit ADPCM数据
 * @param adpcm_bytes ADPCM字节数
 * @param pcm_data 输出的16-bit PCM数据数组
 * @param state ADPCM解码器状态
 * @return 输出的PCM样本数
 */
int adpcm_decode(const uint8_t *adpcm_data, int adpcm_bytes, int16_t *pcm_data, adpcm_state_t *state);

#endif // ADPCM_H
