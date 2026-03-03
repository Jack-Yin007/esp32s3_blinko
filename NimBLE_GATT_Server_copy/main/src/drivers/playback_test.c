/**
 * @file playback_test.c
 * @brief 同时播放和录制测试功能
 * 
 * 功能：
 * 1. 播放WAV文件的同时保持I2S TX启用
 * 2. 允许录音功能同时工作（TX提供时钟）
 * 3. 验证播放和录制互不干扰
 */

#include "playback_test.h"
#include "i2s_audio.h"
#include "gatt_svc.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "PlaybackTest";

// WAV文件头结构
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} __attribute__((packed)) wav_header_t;

typedef struct {
    char id[4];
    uint32_t size;
} __attribute__((packed)) chunk_header_t;

#define AUDIO_BUFFER_SIZE 2048
#define VOLUME_SCALE 0.60f  // 60% 音量

/**
 * @brief 同时播放和录制的测试函数
 * 
 * 与普通播放的区别：
 * - 不调用 i2s_audio_pause_silent_tx()
 * - 播放期间保持TX通道启用
 * - 允许RX通道同时录音
 * 
 * @param wav_id WAV文件ID
 * @return esp_err_t ESP_OK=成功
 */
esp_err_t playback_test_with_recording(uint8_t wav_id)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "🧪 测试: 同时播放和录制 (WAV ID=%d)", wav_id);
    ESP_LOGI(TAG, "========================================");
    
    // 构建文件路径
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/spiffs/%d.wav", wav_id);
    
    ESP_LOGI(TAG, "打开文件: %s", filepath);
    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 无法打开文件: %s", filepath);
        return ESP_FAIL;
    }
    
    // 读取WAV头
    wav_header_t header;
    size_t read_size = fread(&header, 1, sizeof(wav_header_t), f);
    if (read_size != sizeof(wav_header_t)) {
        ESP_LOGE(TAG, "❌ 读取WAV头失败");
        fclose(f);
        return ESP_FAIL;
    }
    
    // 验证WAV格式
    if (memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "❌ 不是有效的WAV文件");
        fclose(f);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WAV格式: %dHz, %d通道, %d位",
             header.sample_rate, header.num_channels, header.bits_per_sample);
    
    // 查找data块
    chunk_header_t chunk;
    uint32_t data_size = 0;
    bool found_data = false;
    
    while (fread(&chunk, 1, sizeof(chunk_header_t), f) == sizeof(chunk_header_t)) {
        if (memcmp(chunk.id, "data", 4) == 0) {
            data_size = chunk.size;
            found_data = true;
            ESP_LOGI(TAG, "✅ 找到data块: %u bytes", data_size);
            break;
        } else {
            // 跳过其他块
            fseek(f, chunk.size, SEEK_CUR);
        }
    }
    
    if (!found_data) {
        ESP_LOGE(TAG, "❌ 未找到data块");
        fclose(f);
        return ESP_FAIL;
    }
    
    // 计算预期时长
    uint32_t total_samples = data_size / (header.num_channels * (header.bits_per_sample / 8));
    uint32_t expected_duration_ms = (total_samples * 1000) / header.sample_rate;
    ESP_LOGI(TAG, "📊 预期播放时长: %u.%02us (%u samples)",
             expected_duration_ms / 1000, (expected_duration_ms % 1000) / 10, total_samples);
    
    // 获取I2S TX句柄
    i2s_chan_handle_t tx_handle = i2s_audio_get_tx_handle();
    if (tx_handle == NULL) {
        ESP_LOGE(TAG, "❌ I2S TX句柄不可用");
        fclose(f);
        return ESP_FAIL;
    }
    
    // ⚠️ 关键区别：不调用 i2s_audio_pause_silent_tx()
    // 这样TX通道保持启用，RX可以同时录音
    ESP_LOGI(TAG, "⚠️ 测试模式: 不停止Silent TX，保持时钟运行");
    ESP_LOGI(TAG, "   这允许录音功能同时工作");
    
    // 启用扬声器（SD pin）
    extern esp_err_t aw9535_set_level(uint8_t pin, uint8_t level);
    aw9535_set_level(13, 1);  // P1.5 = HIGH (SD enable)
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "🔊 MAX98357A已启用");
    
    // 分配缓冲区
    uint8_t *buffer = malloc(AUDIO_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "❌ 缓冲区分配失败");
        fclose(f);
        return ESP_FAIL;
    }
    
    // 播放PCM数据
    uint32_t total_written = 0;
    size_t bytes_written;
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG, "▶️ 开始播放 (同时可录音)...");
    
    while (total_written < data_size) {
        // 读取数据块
        size_t to_read = (data_size - total_written > AUDIO_BUFFER_SIZE) ?
                         AUDIO_BUFFER_SIZE : (data_size - total_written);
        
        size_t bytes_read = fread(buffer, 1, to_read, f);
        if (bytes_read == 0) {
            break;
        }
        
        // 音量缩放 (60%)
        int16_t *samples = (int16_t *)buffer;
        size_t sample_count = bytes_read / 2;
        for (size_t i = 0; i < sample_count; i++) {
            samples[i] = (int16_t)(samples[i] * VOLUME_SCALE);
        }
        
        // 写入I2S
        esp_err_t ret = i2s_channel_write(tx_handle, buffer, bytes_read, &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ I2S写入失败: %s", esp_err_to_name(ret));
            break;
        }
        
        total_written += bytes_written;
        
        // 每1秒报告进度
        static uint32_t last_report = 0;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_report >= 1000) {
            ESP_LOGI(TAG, "📊 进度: %u/%u bytes (%.1f%%)",
                     total_written, data_size, (total_written * 100.0f) / data_size);
            last_report = now;
        }
    }
    
    uint32_t end_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t actual_duration_ms = end_time - start_time;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "⏹️ 播放完成");
    ESP_LOGI(TAG, "   总写入: %u/%u bytes (%.1f%%)",
             total_written, data_size, (total_written * 100.0f) / data_size);
    ESP_LOGI(TAG, "   预期时长: %u ms", expected_duration_ms);
    ESP_LOGI(TAG, "   实际时长: %u ms", actual_duration_ms);
    ESP_LOGI(TAG, "   误差: %+d ms (%.1f%%)",
             (int32_t)(actual_duration_ms - expected_duration_ms),
             ((actual_duration_ms - expected_duration_ms) * 100.0f) / expected_duration_ms);
    ESP_LOGI(TAG, "========================================");
    
    // 刷新DMA缓冲区
    int16_t silence[64] = {0};
    i2s_channel_write(tx_handle, silence, sizeof(silence), &bytes_written, pdMS_TO_TICKS(100));
    
    // 关闭扬声器
    aw9535_set_level(13, 0);  // P1.5 = LOW (SD disable)
    ESP_LOGI(TAG, "🔇 MAX98357A已关闭");
    
    // ⚠️ 关键：不停止TX通道，保持时钟运行
    ESP_LOGI(TAG, "⚠️ TX通道保持启用（时钟继续运行）");
    ESP_LOGI(TAG, "   录音功能可以继续工作");
    
    free(buffer);
    fclose(f);
    
    ESP_LOGI(TAG, "✅ 测试完成");
    return ESP_OK;
}

/**
 * @brief 测试任务入口
 */
static void playback_test_task(void *arg)
{
    uint8_t wav_id = (uint8_t)(uintptr_t)arg;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "🧪 播放&录制测试任务启动");
    ESP_LOGI(TAG, "   目标: 验证同时播放和录制不会相互干扰");
    ESP_LOGI(TAG, "========================================");
    
    // 等待一下，确保录音已启动
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 执行测试
    esp_err_t ret = playback_test_with_recording(wav_id);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 测试成功！");
        ESP_LOGI(TAG, "   - 播放完成");
        ESP_LOGI(TAG, "   - TX通道保持启用");
        ESP_LOGI(TAG, "   - 录音应该能听到播放的声音");
    } else {
        ESP_LOGE(TAG, "❌ 测试失败");
    }
    
    ESP_LOGI(TAG, "========================================");
    
    vTaskDelete(NULL);
}

/**
 * @brief 启动播放测试（非阻塞）
 */
esp_err_t playback_test_start(uint8_t wav_id)
{
    BaseType_t ret = xTaskCreate(
        playback_test_task,
        "playback_test",
        4096,
        (void *)(uintptr_t)wav_id,
        5,
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "❌ 创建测试任务失败");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ 测试任务已创建");
    return ESP_OK;
}
