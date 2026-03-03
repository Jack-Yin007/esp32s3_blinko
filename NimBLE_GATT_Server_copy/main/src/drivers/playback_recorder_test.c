/**
 * 音频播放+录制同步测试
 * 功能：播放WAV文件的同时，通过BLE录制音频，验证播放质量
 */

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "../audio.h"
#include "i2s_audio.h"
#include "aw9535.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "PlaybackTest";

// WAV文件头结构
typedef struct {
    uint32_t chunk_id;           // "RIFF"
    uint32_t chunk_size;         
    uint32_t format;             // "WAVE"
    uint32_t subchunk1_id;       // "fmt "
    uint32_t subchunk1_size;     
    uint16_t audio_format;       // PCM = 1
    uint16_t num_channels;       // 1=Mono, 2=Stereo
    uint32_t sample_rate;        
    uint32_t byte_rate;          
    uint16_t block_align;        
    uint16_t bits_per_sample;    
    uint32_t subchunk2_id;       // "data"
    uint32_t data_size;          
} __attribute__((packed)) wav_header_t;

// 测试状态
static bool test_running = false;
static SemaphoreHandle_t test_done_sem = NULL;

/**
 * 同步播放和录制测试
 * 
 * 工作流程：
 * 1. 启动BLE音频录制（通过audio_start_recording()）
 * 2. 播放指定WAV文件（TX通道持续运行为RX提供时钟）
 * 3. 播放结束后继续录制1秒
 * 4. 停止录制
 * 5. Host端分析录制的WAV与原始WAV对比
 * 
 * @param wav_id 要播放的WAV文件ID（/spiffs/{id}.wav）
 * @return ESP_OK 成功，ESP_FAIL 失败
 */
esp_err_t playback_recorder_test_run(uint8_t wav_id)
{
    if (test_running) {
        ESP_LOGW(TAG, "Test already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "🧪 Playback + Recording Test");
    ESP_LOGI(TAG, "   WAV ID: %d", wav_id);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    test_running = true;
    
    // 创建同步信号量
    if (test_done_sem == NULL) {
        test_done_sem = xSemaphoreCreateBinary();
        if (test_done_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create semaphore");
            test_running = false;
            return ESP_FAIL;
        }
    }
    
    // 步骤1: 启动BLE音频录制
    ESP_LOGI(TAG, "Step 1: Starting BLE audio recording...");
    esp_err_t ret = audio_start_recording();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start recording: %s", esp_err_to_name(ret));
        test_running = false;
        return ret;
    }
    ESP_LOGI(TAG, "✅ Recording started");
    
    // 等待录音稳定
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 步骤2: 播放WAV文件（修改版：不停止Silent TX）
    ESP_LOGI(TAG, "Step 2: Playing WAV file (with recording active)...");
    
    // 构造文件路径
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/%d.wav", wav_id);
    
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open: %s", filepath);
        audio_stop_recording();
        test_running = false;
        return ESP_FAIL;
    }
    
    // 读取WAV头
    wav_header_t header;
    if (fread(&header, 1, sizeof(header), f) != sizeof(header)) {
        ESP_LOGE(TAG, "Failed to read WAV header");
        fclose(f);
        audio_stop_recording();
        test_running = false;
        return ESP_FAIL;
    }
    
    // 验证WAV格式
    if (header.chunk_id != 0x46464952 ||  // "RIFF"
        header.format != 0x45564157 ||     // "WAVE"
        header.audio_format != 1) {        // PCM
        ESP_LOGE(TAG, "Invalid WAV file");
        fclose(f);
        audio_stop_recording();
        test_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WAV Info:");
    ESP_LOGI(TAG, "  Sample Rate: %u Hz", header.sample_rate);
    ESP_LOGI(TAG, "  Channels: %u", header.num_channels);
    ESP_LOGI(TAG, "  Bits: %u", header.bits_per_sample);
    ESP_LOGI(TAG, "  Data Size: %u bytes", header.data_size);
    
    float expected_duration = (float)header.data_size / header.byte_rate;
    ESP_LOGI(TAG, "  Expected Duration: %.2fs", expected_duration);
    
    // 🔑 关键：不停止Silent TX，保持TX通道运行以提供时钟
    // ⚠️ 注意：这里我们不调用 i2s_audio_pause_silent_tx()
    // 因为录音需要TX通道持续提供BCK和WS时钟信号
    
    // 获取I2S TX句柄
    i2s_chan_handle_t tx_handle = i2s_audio_get_tx_handle();
    if (tx_handle == NULL) {
        ESP_LOGE(TAG, "I2S TX handle not available");
        fclose(f);
        audio_stop_recording();
        test_running = false;
        return ESP_FAIL;
    }
    
    // 启用MAX98357A扬声器
    aw9535_set_level(AW9535_PIN_P1_3, AW9535_LEVEL_HIGH);
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "🔊 Speaker enabled");
    
    // 分配播放缓冲区
    #define PLAY_BUFFER_SIZE 2048
    uint8_t *buffer = malloc(PLAY_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        fclose(f);
        audio_stop_recording();
        test_running = false;
        return ESP_FAIL;
    }
    
    // 播放音频数据
    uint32_t total_written = 0;
    size_t bytes_written;
    uint32_t play_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG, "▶️ Playing audio...");
    
    while (total_written < header.data_size) {
        size_t to_read = (header.data_size - total_written > PLAY_BUFFER_SIZE) ?
                         PLAY_BUFFER_SIZE : (header.data_size - total_written);
        
        size_t bytes_read = fread(buffer, 1, to_read, f);
        if (bytes_read == 0) {
            break;
        }
        
        // 音量调整（60%）
        int16_t *samples = (int16_t *)buffer;
        size_t sample_count = bytes_read / 2;
        for (size_t i = 0; i < sample_count; i++) {
            samples[i] = (int16_t)(samples[i] * 0.6f);
        }
        
        // 如果是单声道，需要转为立体声（复制到两个声道）
        if (header.num_channels == 1) {
            // 创建立体声缓冲区
            int16_t *stereo_buffer = malloc(bytes_read * 2);
            if (stereo_buffer) {
                for (size_t i = 0; i < sample_count; i++) {
                    stereo_buffer[i * 2] = samples[i];      // Left
                    stereo_buffer[i * 2 + 1] = samples[i];  // Right
                }
                
                // 写入I2S
                ret = i2s_channel_write(tx_handle, stereo_buffer, bytes_read * 2, &bytes_written, pdMS_TO_TICKS(1000));
                free(stereo_buffer);
                
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "I2S write error: %s", esp_err_to_name(ret));
                    break;
                }
            }
        } else {
            // 立体声直接写入
            ret = i2s_channel_write(tx_handle, buffer, bytes_read, &bytes_written, pdMS_TO_TICKS(1000));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write error: %s", esp_err_to_name(ret));
                break;
            }
        }
        
        total_written += bytes_read;
        
        // 每500ms打印进度
        if (total_written % 16000 == 0) {
            float progress = (float)total_written / header.data_size * 100;
            ESP_LOGI(TAG, "  Progress: %.1f%%", progress);
        }
    }
    
    uint32_t play_end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t actual_duration = play_end_time - play_start_time;
    
    ESP_LOGI(TAG, "✅ Playback complete");
    ESP_LOGI(TAG, "   Expected: %.2fs", expected_duration);
    ESP_LOGI(TAG, "   Actual: %.2fs", actual_duration / 1000.0f);
    ESP_LOGI(TAG, "   Difference: %+dms", (int)(actual_duration - expected_duration * 1000));
    
    // 清理
    free(buffer);
    fclose(f);
    
    // 关闭扬声器
    aw9535_set_level(AW9535_PIN_P1_3, AW9535_LEVEL_LOW);
    ESP_LOGI(TAG, "🔇 Speaker disabled");
    
    // 步骤3: 继续录制1秒（捕获播放结束后的回声或延迟）
    ESP_LOGI(TAG, "Step 3: Continue recording for 1s (echo check)...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 步骤4: 停止录制
    ESP_LOGI(TAG, "Step 4: Stopping recording...");
    ret = audio_stop_recording();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop recording: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(TAG, "✅ Recording stopped");
    
    // 测试完成
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ Test Complete!");
    ESP_LOGI(TAG, "   Please analyze recorded audio on host");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    test_running = false;
    
    // 🔑 恢复Silent TX（为下次普通播放做准备）
    // 注意：这里可以选择恢复或不恢复，取决于系统设计
    // 如果不需要持续录音，可以恢复Silent TX节省功耗
    
    return ESP_OK;
}

/**
 * 检查测试是否正在运行
 */
bool playback_recorder_test_is_running(void)
{
    return test_running;
}
