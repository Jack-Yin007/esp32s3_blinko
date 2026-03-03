/**
 * 音频播放+录制同步测试头文件
 */

#ifndef PLAYBACK_RECORDER_TEST_H
#define PLAYBACK_RECORDER_TEST_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 运行同步播放和录制测试
 * 
 * @param wav_id WAV文件ID（/spiffs/{id}.wav）
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t playback_recorder_test_run(uint8_t wav_id);

/**
 * 检查测试是否正在运行
 * 
 * @return true 运行中，false 未运行
 */
bool playback_recorder_test_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // PLAYBACK_RECORDER_TEST_H
