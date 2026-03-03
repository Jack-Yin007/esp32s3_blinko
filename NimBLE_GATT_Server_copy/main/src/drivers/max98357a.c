/**
 * @file max98357a.c
 * @brief MAX98357A I2S音频DAC驱动实现
 * 
 * 功能特性：
 * - I2S标准模式通信
 * - 4档增益控制（+3/+6/+9/+12 dB）
 * - 关断模式（SD_MODE）省电
 * - 单音调生成（测试用）
 */

#include "drivers/max98357a.h"
#include "drivers/i2s_audio.h"
#include "drivers/aw9535.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "MAX98357A";

/* ==================== 全局变量 ==================== */
static bool g_initialized = false;
static max98357a_config_t g_config;
static max98357a_gain_t g_current_gain;

/* ==================== 私有函数 ==================== */

/**
 * @brief 配置增益引脚
 */
static esp_err_t configure_gain_pins(max98357a_gain_t gain)
{
    bool gain1 = (gain == MAX98357A_GAIN_9DB || gain == MAX98357A_GAIN_12DB);
    bool gain2 = (gain == MAX98357A_GAIN_6DB || gain == MAX98357A_GAIN_12DB);
    
    esp_err_t ret;
    
    // 设置GAIN1
    ret = aw9535_set_level(g_config.gain1_pin, 
                          gain1 ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GAIN1: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 设置GAIN2
    ret = aw9535_set_level(g_config.gain2_pin, 
                          gain2 ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set GAIN2: %s", esp_err_to_name(ret));
        return ret;
    }
    
    g_current_gain = gain;
    
    ESP_LOGI(TAG, "Gain configured: GAIN1=%d GAIN2=%d (%+ddB)", 
             gain1, gain2, 3 + (int)gain * 3);
    
    return ESP_OK;
}

/**
 * @brief 生成正弦波PCM数据
 */
static void generate_sine_wave(int16_t *buffer, size_t samples, 
                               uint32_t frequency, uint32_t sample_rate)
{
    for (size_t i = 0; i < samples; i++) {
        float t = (float)i / sample_rate;
        float value = sinf(2.0f * M_PI * frequency * t);
        buffer[i] = (int16_t)(value * 16000);  // 振幅减半防止削波
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化MAX98357A
 */
esp_err_t max98357a_init(const max98357a_config_t *config)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "MAX98357A already initialized");
        return ESP_OK;
    }
    
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing MAX98357A I2S Audio DAC");
    ESP_LOGI(TAG, "========================================");
    
    // 保存配置
    memcpy(&g_config, config, sizeof(max98357a_config_t));
    
    ESP_LOGI(TAG, "I2S Pin Configuration:");
    ESP_LOGI(TAG, "  BCLK:  GPIO%d", config->bclk_io);
    ESP_LOGI(TAG, "  WS:    GPIO%d", config->ws_io);
    ESP_LOGI(TAG, "  DOUT:  GPIO%d", config->dout_io);
    ESP_LOGI(TAG, "Control Pin Configuration (via AW9535):");
    ESP_LOGI(TAG, "  GAIN1: P1.%d", config->gain1_pin);
    ESP_LOGI(TAG, "  GAIN2: P1.%d", config->gain2_pin);
    ESP_LOGI(TAG, "  SD:    P1.%d", config->sd_pin);
    ESP_LOGI(TAG, "Audio Configuration:");
    ESP_LOGI(TAG, "  Sample Rate: %lu Hz", config->sample_rate);
    ESP_LOGI(TAG, "  Bits/Sample: %d", config->bits_per_sample);
    
    // 1. 配置AW9535控制引脚为输出
    esp_err_t ret;
    
    ret = aw9535_set_mode(config->gain1_pin, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GAIN1 pin");
        return ret;
    }
    
    ret = aw9535_set_mode(config->gain2_pin, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GAIN2 pin");
        return ret;
    }
    
    ret = aw9535_set_mode(config->sd_pin, AW9535_MODE_OUTPUT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SD_MODE pin");
        return ret;
    }
    ESP_LOGI(TAG, "✓ Control pins configured");
    
    // 2. 初始关闭音频输出
    ret = aw9535_set_level(config->sd_pin, AW9535_LEVEL_LOW);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable audio output");
        return ret;
    }
    ESP_LOGI(TAG, "✓ Audio output disabled (SD_MODE=LOW)");
    
    // 3. 配置增益
    ret = configure_gain_pins(config->default_gain);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure gain");
        return ret;
    }
    ESP_LOGI(TAG, "✓ Gain configured to %+ddB", 3 + (int)config->default_gain * 3);
    
    // 4. 检查I2S全双工系统是否已初始化
    if (!i2s_audio_is_initialized()) {
        ESP_LOGW(TAG, "I2S audio system not initialized, initializing now...");
        ret = i2s_audio_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize I2S audio system: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    ESP_LOGI(TAG, "✓ Using shared I2S TX channel (full-duplex mode)");
    
    g_initialized = true;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "MAX98357A Initialized Successfully");
    ESP_LOGI(TAG, "  Status: Ready (SD_MODE=LOW)");
    ESP_LOGI(TAG, "  Use max98357a_enable() to start output");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

/**
 * @brief 关闭MAX98357A
 */
esp_err_t max98357a_deinit(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing MAX98357A...");
    
    // 禁用音频输出
    max98357a_disable();
    
    // 禁用I2S TX通道（但不删除，因为是共享的）
    i2s_audio_disable_tx();
    
    g_initialized = false;
    
    ESP_LOGI(TAG, "MAX98357A deinitialized");
    
    return ESP_OK;
}

/**
 * @brief 启用音频输出
 */
esp_err_t max98357a_enable(void)
{
    if (!g_initialized) {
        ESP_LOGW(TAG, "MAX98357A not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 启用I2S TX通道
    esp_err_t ret = i2s_audio_enable_tx();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel");
        return ret;
    }
    
    // 启用MAX98357A硬件输出
    ret = aw9535_set_level(g_config.sd_pin, AW9535_LEVEL_HIGH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable audio output");
        i2s_audio_disable_tx();
        return ret;
    }
    
    ESP_LOGI(TAG, "Audio output enabled (I2S TX + SD_MODE=HIGH)");
    
    // 等待DAC稳定
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return ESP_OK;
}

/**
 * @brief 禁用音频输出
 */
esp_err_t max98357a_disable(void)
{
    if (!g_initialized) {
        return ESP_OK;
    }
    
    // 禁用MAX98357A硬件输出
    esp_err_t ret = aw9535_set_level(g_config.sd_pin, AW9535_LEVEL_LOW);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disable audio output");
        return ret;
    }
    
    // 禁用I2S TX通道
    i2s_audio_disable_tx();
    
    ESP_LOGI(TAG, "Audio output disabled (SD_MODE=LOW + I2S TX disabled)");
    
    return ESP_OK;
}

/**
 * @brief 设置增益
 */
esp_err_t max98357a_set_gain(max98357a_gain_t gain)
{
    if (!g_initialized) {
        ESP_LOGW(TAG, "MAX98357A not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (gain > MAX98357A_GAIN_12DB) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return configure_gain_pins(gain);
}

/**
 * @brief 写入PCM音频数据
 */
esp_err_t max98357a_write_data(const void *data, size_t size, 
                               size_t *bytes_written, uint32_t timeout_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    i2s_chan_handle_t tx_handle = i2s_audio_get_tx_handle();
    if (!tx_handle) {
        ESP_LOGE(TAG, "I2S TX handle is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = i2s_channel_write(tx_handle, data, size, 
                                      bytes_written, pdMS_TO_TICKS(timeout_ms));
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief 播放单音调（测试用）
 */
esp_err_t max98357a_play_tone(uint32_t frequency, uint32_t duration_ms)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Playing tone: %lu Hz for %lu ms", frequency, duration_ms);
    
    // 启用输出
    max98357a_enable();
    
    // 生成音调数据
    const size_t samples_per_buffer = 512;
    int16_t buffer[samples_per_buffer * 2];  // 立体声
    
    uint32_t total_samples = (g_config.sample_rate * duration_ms) / 1000;
    uint32_t samples_sent = 0;
    
    while (samples_sent < total_samples) {
        size_t samples_to_send = (total_samples - samples_sent) < samples_per_buffer ?
                                 (total_samples - samples_sent) : samples_per_buffer;
        
        // 生成单声道正弦波
        int16_t mono_buffer[samples_per_buffer];
        generate_sine_wave(mono_buffer, samples_to_send, frequency, g_config.sample_rate);
        
        // 转换为立体声（左右声道相同）
        for (size_t i = 0; i < samples_to_send; i++) {
            buffer[i * 2] = mono_buffer[i];      // 左声道
            buffer[i * 2 + 1] = mono_buffer[i];  // 右声道
        }
        
        size_t bytes_written;
        esp_err_t ret = max98357a_write_data(buffer, samples_to_send * 4, 
                                            &bytes_written, 1000);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to write audio data");
            max98357a_disable();
            return ret;
        }
        
        samples_sent += samples_to_send;
    }
    
    // 等待播放完成
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // 禁用输出
    max98357a_disable();
    
    return ESP_OK;
}

/**
 * @brief 获取当前增益
 */
max98357a_gain_t max98357a_get_gain(void)
{
    return g_current_gain;
}

/**
 * @brief 检查是否已初始化
 */
bool max98357a_is_initialized(void)
{
    return g_initialized;
}
