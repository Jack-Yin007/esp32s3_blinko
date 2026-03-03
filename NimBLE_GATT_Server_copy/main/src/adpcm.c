/*
 * IMA ADPCM Encoder/Decoder Implementation
 * Based on IMA ADPCM standard
 */
#include "adpcm.h"
#include <string.h>

/* IMA ADPCM步长表 */
static const int16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

/* 索引调整表 */
static const int8_t index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

void adpcm_init(adpcm_state_t *state)
{
    state->predicted_sample = 0;
    state->step_index = 0;
}

uint8_t adpcm_encode_sample(int16_t sample, adpcm_state_t *state)
{
    int32_t diff = sample - state->predicted_sample;
    uint8_t sign = 0;
    
    if (diff < 0) {
        sign = 8;
        diff = -diff;
    }
    
    int32_t step = step_table[state->step_index];
    uint8_t code = 0;
    int32_t diffq = step >> 3;
    
    if (diff >= step) {
        code = 4;
        diff -= step;
        diffq += step;
    }
    step >>= 1;
    if (diff >= step) {
        code |= 2;
        diff -= step;
        diffq += step;
    }
    step >>= 1;
    if (diff >= step) {
        code |= 1;
        diffq += step;
    }
    
    if (sign) {
        state->predicted_sample -= diffq;
    } else {
        state->predicted_sample += diffq;
    }
    
    // 限幅
    if (state->predicted_sample > 32767) {
        state->predicted_sample = 32767;
    } else if (state->predicted_sample < -32768) {
        state->predicted_sample = -32768;
    }
    
    // 更新步长索引
    state->step_index += index_table[code];
    if (state->step_index < 0) {
        state->step_index = 0;
    } else if (state->step_index > 88) {
        state->step_index = 88;
    }
    
    return sign | code;
}

int adpcm_encode(const int16_t *pcm_data, int pcm_samples, uint8_t *adpcm_data, adpcm_state_t *state)
{
    int adpcm_bytes = 0;
    
    for (int i = 0; i < pcm_samples; i += 2) {
        uint8_t high_nibble = adpcm_encode_sample(pcm_data[i], state);
        uint8_t low_nibble = 0;
        
        if (i + 1 < pcm_samples) {
            low_nibble = adpcm_encode_sample(pcm_data[i + 1], state);
        }
        
        // 打包两个4-bit样本到一个字节
        adpcm_data[adpcm_bytes++] = (high_nibble << 4) | (low_nibble & 0x0F);
    }
    
    return adpcm_bytes;
}

int16_t adpcm_decode_sample(uint8_t adpcm_nibble, adpcm_state_t *state)
{
    int32_t step = step_table[state->step_index];
    int32_t diffq = step >> 3;
    
    if (adpcm_nibble & 4) diffq += step;
    if (adpcm_nibble & 2) diffq += step >> 1;
    if (adpcm_nibble & 1) diffq += step >> 2;
    
    if (adpcm_nibble & 8) {
        state->predicted_sample -= diffq;
    } else {
        state->predicted_sample += diffq;
    }
    
    // 限幅
    if (state->predicted_sample > 32767) {
        state->predicted_sample = 32767;
    } else if (state->predicted_sample < -32768) {
        state->predicted_sample = -32768;
    }
    
    // 更新步长索引
    state->step_index += index_table[adpcm_nibble & 0x07];
    if (state->step_index < 0) {
        state->step_index = 0;
    } else if (state->step_index > 88) {
        state->step_index = 88;
    }
    
    return state->predicted_sample;
}

int adpcm_decode(const uint8_t *adpcm_data, int adpcm_bytes, int16_t *pcm_data, adpcm_state_t *state)
{
    int pcm_samples = 0;
    
    for (int i = 0; i < adpcm_bytes; i++) {
        // 解码高4位
        uint8_t high_nibble = (adpcm_data[i] >> 4) & 0x0F;
        pcm_data[pcm_samples++] = adpcm_decode_sample(high_nibble, state);
        
        // 解码低4位
        uint8_t low_nibble = adpcm_data[i] & 0x0F;
        pcm_data[pcm_samples++] = adpcm_decode_sample(low_nibble, state);
    }
    
    return pcm_samples;
}
