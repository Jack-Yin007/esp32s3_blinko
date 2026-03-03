/**
 * @file custom_i2s_writer.c
 * @brief Custom I2S Writer Element Implementation
 * 
 * 核心设计：
 * 1. 使用现有I2S TX handle（通过i2s_audio_get_tx_handle()获取）
 * 2. 播放前暂停Silent TX任务（释放I2S资源）
 * 3. 播放时独占TX handle进行写入
 * 4. 播放后发送静音帧清空DMA缓冲区
 * 5. 恢复Silent TX任务（为RX提供时钟）
 * 6. 不销毁RX channel，保持录音功能
 */

#include "drivers/custom_i2s_writer.h"
#include "drivers/i2s_audio.h"
#include "drivers/aw9535.h"
#include "audio_element.h"
#include "audio_common.h"
#include "audio_mem.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CUSTOM_I2S_WRITER";

/**
 * @brief Custom I2S Writer内部状态结构体
 */
typedef struct {
    i2s_chan_handle_t tx_handle;       ///< I2S TX通道句柄
    uint32_t sample_rate;              ///< 采样率
    uint16_t channels;                 ///< 声道数
    uint16_t bits_per_sample;          ///< 位深度
    bool use_existing_handle;          ///< 是否使用现有handle
    bool pause_silent_tx;              ///< 是否暂停Silent TX任务
    bool is_open;                      ///< 元素是否已打开
    uint32_t total_written;            ///< 总写入字节数（用于统计）
} custom_i2s_writer_t;

/**
 * @brief Open回调函数 - 准备I2S TX通道
 */
static esp_err_t custom_i2s_writer_open(audio_element_handle_t self)
{
    ESP_LOGI(TAG, "🔓 Opening Custom I2S Writer...");
    
    custom_i2s_writer_t *writer = (custom_i2s_writer_t *)audio_element_getdata(self);
    if (writer == NULL) {
        ESP_LOGE(TAG, "Writer context is NULL");
        return ESP_FAIL;
    }
    
    // 1. 暂停Silent TX任务（释放I2S TX通道）
    if (writer->pause_silent_tx) {
        ESP_LOGI(TAG, "Pausing Silent TX task...");
        esp_err_t ret = i2s_audio_pause_silent_tx();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to pause Silent TX: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // 等待任务完全停止
    }
    
    // 2. 获取I2S TX handle
    if (writer->use_existing_handle) {
        writer->tx_handle = i2s_audio_get_tx_handle();
        if (writer->tx_handle == NULL) {
            ESP_LOGE(TAG, "Failed to get existing TX handle");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "✅ Using existing TX handle: %p", writer->tx_handle);
    } else {
        // 创建新的I2S通道（预留功能，当前不使用）
        ESP_LOGW(TAG, "Creating new TX channel not implemented yet");
        return ESP_FAIL;
    }
    
    // 3. 配置MAX98357A（启用输出）
    ESP_LOGI(TAG, "Configuring MAX98357A for playback...");
    esp_err_t ret = aw9535_set_level(13, AW9535_LEVEL_HIGH);  // P1.5 (SD_MODE) = HIGH (启用)
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable MAX98357A: %s", esp_err_to_name(ret));
    }
    ret = aw9535_set_level(11, AW9535_LEVEL_LOW);   // P1.3 (GAIN_SELECT_1) = LOW
    ret = aw9535_set_level(12, AW9535_LEVEL_HIGH);  // P1.4 (GAIN_SELECT_2) = HIGH (最大音量)
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set MAX98357A gain: %s", esp_err_to_name(ret));
    }
    vTaskDelay(pdMS_TO_TICKS(50));  // 等待MAX98357A启动
    
    // 4. 确保TX通道已启用
    ret = i2s_channel_enable(writer->tx_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to enable TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    writer->is_open = true;
    writer->total_written = 0;
    
    ESP_LOGI(TAG, "✅ Custom I2S Writer opened successfully");
    return ESP_OK;
}

/**
 * @brief Write回调函数 - 写入音频数据到I2S
 */
static int custom_i2s_writer_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    custom_i2s_writer_t *writer = (custom_i2s_writer_t *)audio_element_getdata(self);
    if (writer == NULL || !writer->is_open) {
        ESP_LOGE(TAG, "Writer not opened");
        return AEL_IO_FAIL;
    }
    
    if (buffer == NULL || len <= 0) {
        return 0;
    }
    
    // 音量控制：降低60%音量避免失真
    const float VOLUME_SCALE = 0.6f;
    int16_t *samples = (int16_t *)buffer;
    int sample_count = len / 2;
    for (int i = 0; i < sample_count; i++) {
        samples[i] = (int16_t)(samples[i] * VOLUME_SCALE);
    }
    
    // 写入I2S通道
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(writer->tx_handle, buffer, len, &bytes_written, ticks_to_wait);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write error: %s", esp_err_to_name(ret));
        return AEL_IO_FAIL;
    }
    
    writer->total_written += bytes_written;
    
    return (int)bytes_written;
}

/**
 * @brief Process回调函数 - Pipeline数据处理（不使用，保留接口）
 */
static audio_element_err_t custom_i2s_writer_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    // 不使用Process模式，改用Read/Write模式
    return AEL_PROCESS_FAIL;
}

/**
 * @brief Close回调函数 - 清理DMA缓冲区，恢复系统状态
 */
static esp_err_t custom_i2s_writer_close(audio_element_handle_t self)
{
    ESP_LOGI(TAG, "🔒 Closing Custom I2S Writer...");
    
    custom_i2s_writer_t *writer = (custom_i2s_writer_t *)audio_element_getdata(self);
    if (writer == NULL || !writer->is_open) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Total written: %lu bytes", (unsigned long)writer->total_written);
    
    // 1. 发送静音帧清空DMA缓冲区（避免回声）
    ESP_LOGI(TAG, "🔇 Flushing DMA buffer with silence frames...");
    const size_t SILENCE_SIZE = 2048;
    uint8_t *silence_buffer = calloc(SILENCE_SIZE, 1);  // 全零=静音
    if (silence_buffer != NULL) {
        size_t silence_written;
        // 连续发送15次静音帧，彻底冲刷DMA
        for (int i = 0; i < 15; i++) {
            i2s_channel_write(writer->tx_handle, silence_buffer, SILENCE_SIZE, &silence_written, pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        free(silence_buffer);
        ESP_LOGI(TAG, "✅ DMA buffer flushed with 15 silence frames");
    } else {
        ESP_LOGW(TAG, "Failed to allocate silence buffer");
    }
    
    // 2. 等待DMA完全清空
    ESP_LOGI(TAG, "⏳ Waiting for DMA drain...");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 3. 暂时禁用MAX98357A（避免切换噪声）
    ESP_LOGI(TAG, "🔇 Disabling MAX98357A to prevent switching noise...");
    aw9535_set_level(AW9535_PIN_P1_3, AW9535_LEVEL_LOW);  // SD_MODE = LOW
    vTaskDelay(pdMS_TO_TICKS(200));  // 等待硬件完全关断
    
    // 4. 恢复Silent TX任务（为RX提供时钟）
    if (writer->pause_silent_tx) {
        ESP_LOGI(TAG, "Resuming Silent TX task...");
        esp_err_t ret = i2s_audio_resume_silent_tx();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to resume Silent TX: %s", esp_err_to_name(ret));
        }
    }
    
    // 5. 重新启用MAX98357A（恢复正常状态）
    ESP_LOGI(TAG, "🔊 Re-enabling MAX98357A for normal operation...");
    esp_err_t sd_ret = aw9535_set_level(AW9535_PIN_P1_3, AW9535_LEVEL_HIGH);
    esp_err_t gain_ret = aw9535_set_level(AW9535_PIN_P1_2, AW9535_LEVEL_HIGH);
    if (sd_ret != ESP_OK || gain_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to restore MAX98357A: SD=%s, GAIN=%s", 
                 esp_err_to_name(sd_ret), esp_err_to_name(gain_ret));
    }
    vTaskDelay(pdMS_TO_TICKS(200));  // 等待硬件稳定
    
    writer->is_open = false;
    writer->tx_handle = NULL;  // 不删除handle（由i2s_audio管理）
    
    ESP_LOGI(TAG, "✅ Custom I2S Writer closed successfully");
    return ESP_OK;
}

/**
 * @brief Destroy回调函数 - 释放元素资源
 */
static esp_err_t custom_i2s_writer_destroy(audio_element_handle_t self)
{
    ESP_LOGI(TAG, "🗑️ Destroying Custom I2S Writer...");
    
    custom_i2s_writer_t *writer = (custom_i2s_writer_t *)audio_element_getdata(self);
    if (writer != NULL) {
        audio_free(writer);
    }
    
    ESP_LOGI(TAG, "✅ Custom I2S Writer destroyed");
    return ESP_OK;
}

/**
 * @brief 初始化Custom I2S Writer Element
 */
audio_element_handle_t custom_i2s_writer_init(custom_i2s_writer_cfg_t *cfg)
{
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Config is NULL");
        return NULL;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing Custom I2S Writer Element");
    ESP_LOGI(TAG, "  Sample Rate: %lu Hz", (unsigned long)cfg->sample_rate);
    ESP_LOGI(TAG, "  Channels: %d", cfg->channels);
    ESP_LOGI(TAG, "  Bits: %d", cfg->bits_per_sample);
    ESP_LOGI(TAG, "  Use Existing Handle: %s", cfg->use_existing_handle ? "Yes" : "No");
    ESP_LOGI(TAG, "  Pause Silent TX: %s", cfg->pause_silent_tx ? "Yes" : "No");
    ESP_LOGI(TAG, "========================================");
    
    // 分配内部状态结构体
    custom_i2s_writer_t *writer = audio_calloc(1, sizeof(custom_i2s_writer_t));
    AUDIO_NULL_CHECK(TAG, writer, return NULL);
    
    writer->sample_rate = cfg->sample_rate;
    writer->channels = cfg->channels;
    writer->bits_per_sample = cfg->bits_per_sample;
    writer->use_existing_handle = cfg->use_existing_handle;
    writer->pause_silent_tx = cfg->pause_silent_tx;
    writer->is_open = false;
    writer->tx_handle = NULL;
    writer->total_written = 0;
    
    // 配置Audio Element
    audio_element_cfg_t el_cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    el_cfg.open = custom_i2s_writer_open;
    el_cfg.write = custom_i2s_writer_write;
    el_cfg.process = custom_i2s_writer_process;
    el_cfg.close = custom_i2s_writer_close;
    el_cfg.destroy = custom_i2s_writer_destroy;
    el_cfg.task_stack = 4096;  // 栈大小
    el_cfg.task_prio = 5;      // 优先级
    el_cfg.task_core = 0;      // Core 0
    el_cfg.out_rb_size = 0;    // Writer不需要输出ringbuffer
    el_cfg.tag = "custom_i2s_writer";
    
    audio_element_handle_t el = audio_element_init(&el_cfg);
    AUDIO_NULL_CHECK(TAG, el, {
        audio_free(writer);
        return NULL;
    });
    
    audio_element_setdata(el, writer);
    
    ESP_LOGI(TAG, "✅ Custom I2S Writer Element created successfully");
    return el;
}
