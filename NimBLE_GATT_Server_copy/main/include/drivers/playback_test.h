/**
 * @file playback_test.h
 * @brief 播放和录制同时进行的测试功能
 */

#ifndef PLAYBACK_TEST_H
#define PLAYBACK_TEST_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief 启动播放测试（同时可录音）
 * 
 * @param wav_id WAV文件ID (1-23)
 * @return esp_err_t ESP_OK=成功
 */
esp_err_t playback_test_start(uint8_t wav_id);

/**
 * @brief 同时播放和录制的测试函数
 * 
 * @param wav_id WAV文件ID
 * @return esp_err_t ESP_OK=成功
 */
esp_err_t playback_test_with_recording(uint8_t wav_id);

#endif // PLAYBACK_TEST_H
