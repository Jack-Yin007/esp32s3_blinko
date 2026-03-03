/**
 * @file audio_player.c
 * @brief WAV音频播放驱动 - 基于MAX98357A I2S DAC
 * 
 * 功能：从SD卡读取WAV文件并通过I2S播放
 * - 硬件：ESP32-S3 I2S → MAX98357A DAC → 扬声器
 * - 格式：支持8KHz/16KHz, 单声道/立体声, 16-bit PCM
 * - 路径：/sdcard/audio/{ID}.wav
 */

#include "drivers/hardware_interface.h"
#include "drivers/i2s_audio.h"
#include "drivers/aw9535.h"
#include "driver/i2s_std.h"
#include "driver/i2s_common.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "WAVPlayer";

/* ==================== WAV文件头结构 ==================== */
typedef struct __attribute__((packed)) {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // 文件大小 - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // fmt块大小(16)
    uint16_t audio_format;  // 音频格式(1=PCM)
    uint16_t num_channels;  // 声道数(1=单声道, 2=立体声)
    uint32_t sample_rate;   // 采样率
    uint32_t byte_rate;     // 字节率
    uint16_t block_align;   // 块对齐
    uint16_t bits_per_sample; // 位深度(16)
    char data[4];           // "data"
    uint32_t data_size;     // PCM数据大小
} wav_header_t;

/* ==================== 全局变量 ==================== */
static bool g_audio_initialized = false;
static TaskHandle_t g_audio_task = NULL;
static QueueHandle_t g_audio_queue = NULL;

// 全局播放状态标志（可被trigger_detector访问以阻止播放期间的新触发）
bool g_is_playing = false;

#define AUDIO_BUFFER_SIZE 2048  // I2S写入缓冲区大小

/* ==================== 私有函数 ==================== */

/**
 * @brief 验证WAV文件头
 */
static bool validate_wav_header(const wav_header_t *header)
{
    if (memcmp(header->riff, "RIFF", 4) != 0 ||
        memcmp(header->wave, "WAVE", 4) != 0 ||
        memcmp(header->fmt, "fmt ", 4) != 0 ||
        memcmp(header->data, "data", 4) != 0) {
        ESP_LOGE(TAG, "Invalid WAV format - Header mismatch");
        return false;
    }
    
    if (header->audio_format != 1) {
        ESP_LOGE(TAG, "Only PCM format supported (got %d)", header->audio_format);
        return false;
    }
    
    if (header->bits_per_sample != 16) {
        ESP_LOGE(TAG, "Only 16-bit supported (got %d-bit)", header->bits_per_sample);
        return false;
    }
    
    // 精简日志：仅显示关键信息
    ESP_LOGI(TAG, "✅ WAV: %dHz %dch %dbit %dB",
             header->sample_rate, header->num_channels,
             header->bits_per_sample, header->data_size);
    
    return true;
}

/**
 * @brief 播放WAV文件（使用现有TX handle，不销毁RX - 参考官方ES8311全双工模式）
 * 
 * 设计原理：
 * 1. 获取现有的TX handle（通过i2s_audio_get_tx_handle()）
 * 2. 暂停Silent TX任务（释放TX通道）
 * 3. 直接使用TX handle播放WAV（RX通道持续工作，BLE录音不中断）
 * 4. 播放完成后清空DMA缓冲区
 * 5. 恢复Silent TX任务（为RX提供时钟）
 */
static void play_wav_file(sound_effect_id_t wav_id)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "▶️ START PLAYBACK (Reuse Handle Mode): WAV ID=%d", wav_id);
    ESP_LOGI(TAG, "========================================");

    g_is_playing = true;
    ESP_LOGI(TAG, "🔒 Set g_is_playing = TRUE");

    // 步骤1: 获取现有TX handle（不销毁RX通道）
    i2s_chan_handle_t tx_handle = i2s_audio_get_tx_handle();
    if (tx_handle == NULL) {
        ESP_LOGE(TAG, "TX handle not available (i2s_audio not initialized?)");
        g_is_playing = false;
        return;
    }
    ESP_LOGI(TAG, "✓ Using existing TX handle: %p", tx_handle);

    // 步骤2: 暂停Silent TX任务（释放TX通道使用权）
    ESP_LOGI(TAG, "Pausing Silent TX task...");
    esp_err_t ret = i2s_audio_pause_silent_tx();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to pause Silent TX: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // 等待硬件状态稳定

    // 步骤2.5: 启用I2S TX通道（pause函数已禁用）
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(ret));
        g_is_playing = false;
        return;
    }
    ESP_LOGD(TAG, "I2S TX channel enabled for playback");

    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/flash/audio/%d.WAV", wav_id);

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open: %s", filepath);
        goto cleanup;
    }

    wav_header_t header;
    if (fread(&header, 1, sizeof(wav_header_t), f) != sizeof(wav_header_t)) {
        ESP_LOGE(TAG, "Failed to read WAV header");
        fclose(f);
        goto cleanup;
    }

    // 查找 "data" 块
    if (memcmp(header.data, "data", 4) != 0) {
        ESP_LOGW(TAG, "Found chunk: %.4s (skipping to find 'data')", header.data);
        
        // 跳过当前块（LIST/INFO等）
        uint32_t chunk_size = header.data_size;
        fseek(f, chunk_size, SEEK_CUR);
        
        // 查找data块（最多尝试5次）
        for (int i = 0; i < 5; i++) {
            char chunk_id[4];
            uint32_t chunk_sz;
            
            if (fread(chunk_id, 1, 4, f) != 4 || fread(&chunk_sz, 1, 4, f) != 4) {
                ESP_LOGE(TAG, "Failed to read chunk header");
                fclose(f);
                goto cleanup;
            }
            
            if (memcmp(chunk_id, "data", 4) == 0) {
                // 找到data块！
                header.data_size = chunk_sz;
                memcpy(header.data, chunk_id, 4);
                ESP_LOGI(TAG, "✅ Found 'data' chunk at offset %ld, size=%lu", 
                         ftell(f), (unsigned long)chunk_sz);
                break;
            }
            
            // 跳过这个块
            fseek(f, chunk_sz, SEEK_CUR);
            
            if (i == 4) {
                ESP_LOGE(TAG, "'data' chunk not found after 5 attempts");
                fclose(f);
                goto cleanup;
            }
        }
    }

    if (!validate_wav_header(&header)) {
        fclose(f);
        goto cleanup;
    }

    // 步骤3: 配置MAX98357A准备播放
    ESP_LOGI(TAG, "Configuring MAX98357A...");
    ret = aw9535_set_level(13, AW9535_LEVEL_HIGH);  // P1.5 (SD_MODE) = HIGH (启用)
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable MAX98357A: %s", esp_err_to_name(ret));
    }
    
    // 确保音量保持最大 (P1.3=0, P1.4=1)
    ret = aw9535_set_level(11, AW9535_LEVEL_LOW);   // P1.3 (GAIN_SELECT_1) = 0
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set P1.3: %s", esp_err_to_name(ret));
    }
    ret = aw9535_set_level(12, AW9535_LEVEL_HIGH); // P1.4 (GAIN_SELECT_2) = 1
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set P1.4: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // 等待MAX98357A启动

    // 步骤4: 播放音频数据（使用现有TX handle）
    ESP_LOGI(TAG, "Playing audio data...");
    uint8_t *buffer = malloc(AUDIO_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        fclose(f);
        goto cleanup;
    }
    
    uint32_t total_written = 0;
    size_t bytes_written;
    
    while (total_written < header.data_size) {
        size_t to_read = (header.data_size - total_written > AUDIO_BUFFER_SIZE) ? 
                         AUDIO_BUFFER_SIZE : (header.data_size - total_written);
        size_t bytes_read = fread(buffer, 1, to_read, f);
        if (bytes_read == 0) break;

        // 音量控制：降低60%避免失真
        const float VOLUME_SCALE = 0.6f;
        if (header.bits_per_sample == 16) {
            int16_t *samples = (int16_t *)buffer;
            for (size_t i = 0; i < bytes_read / 2; i++) {
                samples[i] = (int16_t)(samples[i] * VOLUME_SCALE);
            }
        }

        // 写入数据到现有TX handle（RX继续工作）
        if (i2s_channel_write(tx_handle, buffer, bytes_read, &bytes_written, portMAX_DELAY) != ESP_OK) {
            ESP_LOGE(TAG, "I2S write error");
            break;
        }
        total_written += bytes_written;
    }
    
    free(buffer);
    fclose(f);
    ESP_LOGI(TAG, "⏹️ Finished writing %u/%u bytes to I2S.", total_written, header.data_size);

    // 步骤5: 完整的DMA drain（基于ESP32技术手册）
    ESP_LOGI(TAG, "🧹 Performing complete DMA drain to eliminate residue...");
    
    esp_err_t drain_result = i2s_drain_tx_completely(
        tx_handle,
        header.sample_rate,
        header.num_channels,
        header.bits_per_sample
    );
    
    if (drain_result == ESP_OK) {
        ESP_LOGI(TAG, "✅ Complete DMA drain successful - no residue expected");
    } else {
        ESP_LOGW(TAG, "⚠️ DMA drain failed: %s - may have residue", esp_err_to_name(drain_result));
        
        // 备用方案：简单的软件flush
        ESP_LOGI(TAG, "🔄 Falling back to simple silence flush...");
        uint8_t *silence_buffer = calloc(AUDIO_BUFFER_SIZE, 1);
        if (silence_buffer != NULL) {
            size_t silence_written;
            for (int i = 0; i < 10; i++) {
                esp_err_t write_ret = i2s_channel_write(tx_handle, silence_buffer, AUDIO_BUFFER_SIZE, 
                                                       &silence_written, pdMS_TO_TICKS(100));
                if (write_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Silence write failed: %s", esp_err_to_name(write_ret));
                    break;
                }
            }
            free(silence_buffer);
            ESP_LOGI(TAG, "Backup flush completed");
        }
    }

cleanup:
    // 步骤6: 恢复Silent TX任务（为RX提供时钟）
    ESP_LOGI(TAG, "🔄 Resuming Silent TX to maintain RX clock...");
    esp_err_t resume_ret = i2s_audio_resume_silent_tx();
    if (resume_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resume Silent TX: %s - RX may not work!", esp_err_to_name(resume_ret));
    } else {
        ESP_LOGI(TAG, "✅ Silent TX resumed - RX clock active");
    }
    
    // 重置播放状态，允许新的触发
    g_is_playing = false;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ Playback complete (Reuse Handle Mode)");
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief 音频播放任务
 */
static void audio_player_task(void *arg)
{
    sound_effect_id_t wav_id;
    
    ESP_LOGI(TAG, "WAV player task started");
    
    while (1) {
        // 从队列接收播放请求
        if (xQueueReceive(g_audio_queue, &wav_id, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "🎵 Processing WAV request: %d", wav_id);
            
            // 不再清空队列 - 让每个请求都被处理
            // 这确保了连续触发不会被丢弃
            
            play_wav_file(wav_id);
        }
    }
}


/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化WAV播放器
 */
esp_err_t hw_audio_player_init(void)
{
    if (g_audio_initialized) {
        ESP_LOGW(TAG, "WAV player already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing WAV Player");
    ESP_LOGI(TAG, "  Hardware: I2S + MAX98357A DAC");
    ESP_LOGI(TAG, "========================================");
    
    // 注意：I2S音频系统应该在pet_main.c中预先初始化
    // 这里只检查I2S TX句柄是否可用
    i2s_chan_handle_t tx_handle = i2s_audio_get_tx_handle();
    if (tx_handle == NULL) {
        ESP_LOGE(TAG, "I2S TX handle not available (i2s_audio_init() not called?)");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "✓ I2S TX handle available");
    
    // 配置MAX98357A控制引脚（通过AW9535 P1口）
    // P1.3 = GAIN_SELECT_1：音量控制位1
    // P1.4 = GAIN_SELECT_2：音量控制位2
    // P1.5 = SD引脚（Shutdown）：HIGH=正常工作, LOW=关断
    // 音量组合：(P1.3, P1.4) = (1,0)最小音量; (0,0)中音量; (0,1)最大音量
    ESP_LOGI(TAG, "Configuring MAX98357A control pins...");
    
    // ⚠️ CRITICAL: Configure pins as OUTPUT first (AW9535 defaults to INPUT on power-up)
    ESP_LOGI(TAG, "🔧 Step 1: Configuring P1.3, P1.4 and P1.5 as OUTPUT...");
    
    // P1.3 = GAIN_SELECT_1 (Pin 11 in AW9535 indexing)
    esp_err_t ret = aw9535_set_mode(11, AW9535_MODE_OUTPUT);  // P1.3 (GAIN_SELECT_1)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.3 mode: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.3 (GAIN_SELECT_1) configured as OUTPUT");
    }
    
    // P1.4 = GAIN_SELECT_2 (Pin 12 in AW9535 indexing)
    ret = aw9535_set_mode(12, AW9535_MODE_OUTPUT);  // P1.4 (GAIN_SELECT_2)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.4 mode: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.4 (GAIN_SELECT_2) configured as OUTPUT");
    }
    
    // P1.5 = SD_MODE (Pin 13 in AW9535 indexing)
    ret = aw9535_set_mode(13, AW9535_MODE_OUTPUT);  // P1.5 (SD_MODE)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.5 mode: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.5 (SD_MODE) configured as OUTPUT");
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));  // Allow mode change to settle
    
    // 2. 启用SD引脚（P1.5 = HIGH），退出关断模式
    ESP_LOGI(TAG, "🔧 Step 2: Setting P1.5 (SD_MODE) to HIGH...");
    ret = aw9535_set_level(13, AW9535_LEVEL_HIGH);  // P1.5 = Pin 13
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to enable SD pin: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ SD pin set command OK");
    }
    
    // 读回验证P1.5状态
    aw9535_level_t sd_level;
    ret = aw9535_get_level(13, &sd_level);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "📊 P1.5 readback: %s (expect: HIGH)", 
                 sd_level == AW9535_LEVEL_HIGH ? "HIGH" : "LOW");
        if (sd_level != AW9535_LEVEL_HIGH) {
            ESP_LOGE(TAG, "⚠️ SD_MODE verification FAILED! Expected HIGH but got LOW");
        }
    } else {
        ESP_LOGW(TAG, "⚠️ Cannot readback P1.5 level: %s", esp_err_to_name(ret));
    }
    
    // 3. 设置音量为最大 (P1.3=0, P1.4=1)
    ESP_LOGI(TAG, "🔧 Step 3: Setting volume to MAXIMUM (P1.3=0, P1.4=1)...");
    
    // P1.3 = LOW
    ret = aw9535_set_level(11, AW9535_LEVEL_LOW);  // P1.3 (GAIN_SELECT_1) = 0
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.3 to LOW: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.3 (GAIN_SELECT_1) = LOW");
    }
    
    // P1.4 = HIGH
    ret = aw9535_set_level(12, AW9535_LEVEL_HIGH);  // P1.4 (GAIN_SELECT_2) = 1
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.4 to HIGH: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.4 (GAIN_SELECT_2) = HIGH");
    }
    
    ESP_LOGI(TAG, "🔊 MAX98357A volume set to MAXIMUM (100%)");
    
    // MAX98357A需要至少10ms启动时间（数据手册要求）
    vTaskDelay(pdMS_TO_TICKS(20));  // 等待20ms确保完全启动
    
    // 4. 创建音频播放队列
    g_audio_queue = xQueueCreate(5, sizeof(sound_effect_id_t));
    if (g_audio_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create audio queue");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ Audio queue created (size=5)");
    
    // 创建WAV播放任务
    BaseType_t task_ret = xTaskCreate(
        audio_player_task,
        "wav_player",
        4096,  // 减少栈大小以节省内存 (从8192减少)
        NULL,
        5,
        &g_audio_task
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WAV player task");
        vQueueDelete(g_audio_queue);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ WAV player task created");
    
    g_audio_initialized = true;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "WAV Player Initialized Successfully");
    ESP_LOGI(TAG, "  Source: /sdcard/audio/{ID}.wav");
    ESP_LOGI(TAG, "  Format: 8KHz/16KHz, 1/2-ch, 16-bit PCM");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

/**
 * @brief 播放WAV文件
 */
esp_err_t hw_audio_play_wav(sound_effect_id_t wav_id)
{
    if (!g_audio_initialized) {
        ESP_LOGW(TAG, "WAV player not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 无条件清空队列，只保留最新的请求（防止重复播放）
    sound_effect_id_t dummy;
    int cleared = 0;
    while (xQueueReceive(g_audio_queue, &dummy, 0) == pdTRUE) {
        cleared++;
    }
    if (cleared > 0) {
        ESP_LOGI(TAG, "🗑️ Cleared %d old requests, queuing WAV %d", cleared, wav_id);
    }
    
    // 发送到播放队列（只保留最新的请求）
    if (xQueueSend(g_audio_queue, &wav_id, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Audio queue full, WAV %d dropped", wav_id);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WAV %d queued", wav_id);
    
    return ESP_OK;
}

/**
 * @brief 停止播放
 */
esp_err_t hw_audio_stop(void)
{
    if (!g_audio_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping WAV playback");
    
    // 清空队列
    xQueueReset(g_audio_queue);
    
    // 停止I2S传输
    i2s_chan_handle_t tx_handle = i2s_audio_get_tx_handle();
    if (tx_handle != NULL) {
        i2s_channel_disable(tx_handle);
    }
    
    g_is_playing = false;
    
    return ESP_OK;
}

/**
 * @brief 设置音量（通过双GPIO控制MAX98357A增益）
 * @param volume 音量百分比 0-100
 *               0-33%   = 最小音量 (P11=1, P12=0)
 *               34-66%  = 中音量   (P11=0, P12=0)
 *               67-100% = 最大音量 (P11=0, P12=1)
 */
esp_err_t hw_audio_set_volume(uint8_t volume)
{
    if (!g_audio_initialized) {
        ESP_LOGW(TAG, "WAV player not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 限制音量范围
    if (volume > 100) {
        volume = 100;
    }
    
    esp_err_t ret;
    
    // 根据音量百分比设置P11(P1.1)和P12(P1.2)的组合
    if (volume <= 33) {
        // 最小音量: P11=1, P12=0
        ESP_LOGI(TAG, "Setting volume to MINIMUM (%d%%) - (P11=1, P12=0)", volume);
        ret = aw9535_set_level(9, AW9535_LEVEL_HIGH);  // P1.1 = 1
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set P1.1: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = aw9535_set_level(10, AW9535_LEVEL_LOW);  // P1.2 = 0
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set P1.2: %s", esp_err_to_name(ret));
            return ret;
        }
    } else if (volume <= 66) {
        // 中音量: P11=0, P12=0
        ESP_LOGI(TAG, "Setting volume to MEDIUM (%d%%) - (P11=0, P12=0)", volume);
        ret = aw9535_set_level(9, AW9535_LEVEL_LOW);   // P1.1 = 0
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set P1.1: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = aw9535_set_level(10, AW9535_LEVEL_LOW);  // P1.2 = 0
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set P1.2: %s", esp_err_to_name(ret));
            return ret;
        }
    } else {
        // 最大音量: P11=0, P12=1
        ESP_LOGI(TAG, "Setting volume to MAXIMUM (%d%%) - (P11=0, P12=1)", volume);
        ret = aw9535_set_level(9, AW9535_LEVEL_LOW);   // P1.1 = 0
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set P1.1: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = aw9535_set_level(10, AW9535_LEVEL_HIGH); // P1.2 = 1
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set P1.2: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "✅ Volume set to %d%% successfully", volume);
    return ESP_OK;
}

/**
 * @brief 检查是否正在播放
 */
bool hw_audio_is_playing(void)
{
    return g_is_playing;
}

/**
 * @brief 测试播放功能（播放测试文件）
 */
esp_err_t hw_audio_test_all_effects(void)
{
    if (!g_audio_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Testing WAV playback...");
    ESP_LOGI(TAG, "========================================");
    
    // 测试播放前3个WAV文件
    for (uint8_t i = 1; i <= 3; i++) {
        ESP_LOGI(TAG, "Testing WAV file: /sdcard/audio/%d.wav", i);
        
        esp_err_t ret = hw_audio_play_wav(i);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to queue WAV %d", i);
            continue;
        }
        
        // 等待播放完成
        while (hw_audio_is_playing()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 文件间隔
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "WAV playback test completed");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}
