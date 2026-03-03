/**
 * @file microphone.c
 * @brief INMP441数字麦克风驱动实现 - I2S接口
 * 
 * 硬件连接：
 * - INMP441 SCK  -> ESP32-S3 GPIO40 (I2S BCK)
 * - INMP441 SD   -> ESP32-S3 GPIO15 (I2S DIN)
 * - INMP441 WS   -> ESP32-S3 GPIO39 (I2S WS)
 * - INMP441 LR   -> GND (左声道)
 * - INMP441 VDD  -> 3.3V
 * - INMP441 GND  -> GND
 * 
 * 特性：
 * - 采样率：16kHz
 * - 位深度：32位
 * - 声音检测阈值可配置
 * - 实时RMS音量计算
 * - 噪声基线自动校准
 */

#include "drivers/hardware_interface.h"
#include "drivers/i2s_audio.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "INMP441";

/* ==================== 硬件配置 ==================== */
// INMP441 I2S数字麦克风配置（使用共享的I2S0 RX通道）
#define MIC_SAMPLE_RATE_DEFAULT 8000        // 8kHz采样率

// I2S读取缓冲区大小
#define I2S_READ_LEN            1024        // 读取缓冲区大小（字节）- 匹配默认BLE MTU

/* ==================== 声音检测配置 ==================== */
#define SOUND_THRESHOLD_DEFAULT  50   // 默认声音检测阈值（0-100）
#define NOISE_FLOOR_SAMPLES      50   // 噪声基线采样数
#define SOUND_DETECTION_WINDOW   10   // 检测窗口（ms）- 减小以提高BLE流式传输效率
#define RMS_SAMPLE_COUNT         256  // RMS计算样本数

/* ==================== 全局变量 ==================== */
static bool g_microphone_initialized = false;
static TaskHandle_t g_mic_task = NULL;
static uint8_t g_sound_threshold = SOUND_THRESHOLD_DEFAULT;
static uint8_t g_current_sound_level = 0;
static int32_t g_noise_floor = 0;
static bool g_sound_detected = false;
static bool g_streaming_enabled = false;  // BLE音频流开关

// 采样率（与共享的I2S系统保持一致）
static uint32_t g_sample_rate = MIC_SAMPLE_RATE_DEFAULT;

// BLE音频回调
static void (*g_audio_data_callback)(const uint8_t *data, uint16_t len) = NULL;

/* ==================== 私有函数 ==================== */

/**
 * @brief 计算声音电平（RMS）
 * @param samples 采样数据（16位有符号）
 * @param num_samples 采样数量
 * @return 声音电平 (0-100)
 */
static uint8_t calculate_sound_level(int16_t *samples, size_t num_samples)
{
    if (num_samples == 0) return 0;
    
    // 计算RMS (Root Mean Square)
    int64_t sum_squares = 0;
    for (size_t i = 0; i < num_samples; i++) {
        // 移除噪声基线
        int32_t sample = samples[i] - g_noise_floor;
        sum_squares += (int64_t)(sample * sample);
    }
    
    int32_t rms = (int32_t)sqrt((double)sum_squares / num_samples);
    
    // 映射到0-100范围 (16位音频范围)
    uint8_t level = (rms * 100) / 16384;
    if (level > 100) level = 100;
    
    return level;
}

/**
 * @brief 测量噪声基线
 */
static void measure_noise_floor(void)
{
    ESP_LOGI(TAG, "Measuring noise floor...");
    
    i2s_chan_handle_t rx_handle = i2s_audio_get_rx_handle();
    if (!rx_handle) {
        ESP_LOGW(TAG, "RX handle not available");
        return;
    }
    
    int16_t i2s_read_buffer[RMS_SAMPLE_COUNT];
    size_t bytes_read = 0;
    int64_t sum = 0;
    int count = 0;
    
    for (int i = 0; i < NOISE_FLOOR_SAMPLES; i++) {
        if (i2s_channel_read(rx_handle, i2s_read_buffer, sizeof(i2s_read_buffer), 
                            &bytes_read, pdMS_TO_TICKS(1000)) == ESP_OK) {
            size_t samples_read = bytes_read / sizeof(int16_t);
            for (size_t j = 0; j < samples_read; j++) {
                sum += i2s_read_buffer[j];
                count++;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (count > 0) {
        g_noise_floor = (int32_t)(sum / count);
        ESP_LOGI(TAG, "Noise floor: %ld (samples: %d)", g_noise_floor, count);
    } else {
        g_noise_floor = 0;
        ESP_LOGW(TAG, "Failed to measure noise floor");
    }
}

/**
 * @brief I2S麦克风监听任务
 */
static void microphone_task(void *arg)
{
    int16_t *i2s_read_buffer = (int16_t *)malloc(I2S_READ_LEN);
    if (i2s_read_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate I2S read buffer");
        vTaskDelete(NULL);
        return;
    }
    
    // 获取共享的RX句柄
    i2s_chan_handle_t rx_handle = i2s_audio_get_rx_handle();
    if (!rx_handle) {
        ESP_LOGE(TAG, "Failed to get I2S RX handle, task exiting");
        free(i2s_read_buffer);
        vTaskDelete(NULL);
        return;
    }
    
    size_t bytes_read = 0;
    
    // 等待I2S稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 测量噪声基线
    measure_noise_floor();
    
    ESP_LOGI(TAG, "Microphone task started (8kHz/16bit, shared I2S RX)");
    
    while (1) {
        // 从I2S读取音频数据
        esp_err_t ret = i2s_channel_read(rx_handle, i2s_read_buffer, I2S_READ_LEN, 
                                         &bytes_read, pdMS_TO_TICKS(1000));
        
        if (ret == ESP_OK && bytes_read > 0) {
            size_t samples_read = bytes_read / sizeof(int16_t);
            
            // 如果BLE音频流启用，发送数据给上层
            if (g_streaming_enabled && g_audio_data_callback != NULL) {
                g_audio_data_callback((const uint8_t *)i2s_read_buffer, bytes_read);
            }
            
            // 限制计算样本数
            if (samples_read > RMS_SAMPLE_COUNT) {
                samples_read = RMS_SAMPLE_COUNT;
            }
            
            // 计算声音电平
            uint8_t level = calculate_sound_level(i2s_read_buffer, samples_read);
            g_current_sound_level = level;
            
            // 检测声音
            bool detected = (level >= g_sound_threshold);
            
            // 状态变化时打印
            if (detected != g_sound_detected) {
                g_sound_detected = detected;
                if (detected) {
                    ESP_LOGI(TAG, "Sound detected! Level: %d (threshold: %d)", 
                             level, g_sound_threshold);
                } else {
                    ESP_LOGI(TAG, "Sound ended. Level: %d", level);
                }
            }
        } else {
            ESP_LOGW(TAG, "I2S read error: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 仅在非流式传输模式下延迟，确保BLE音频流连续
        if (!g_streaming_enabled) {
            vTaskDelay(pdMS_TO_TICKS(SOUND_DETECTION_WINDOW));
        }
    }
    
    free(i2s_read_buffer);
    vTaskDelete(NULL);
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化INMP441数字麦克风
 */
esp_err_t hw_microphone_init(void)
{
    if (g_microphone_initialized) {
        ESP_LOGW(TAG, "Microphone already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing INMP441 Digital Microphone");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Mode: Shared I2S RX (full-duplex)");
    ESP_LOGI(TAG, "  Sample rate: %lu Hz", g_sample_rate);
    ESP_LOGI(TAG, "  Bit depth: 16-bit");
    ESP_LOGI(TAG, "  Channels: Mono (Left)");
    
    // 1. 检查I2S全双工系统是否已初始化
    if (!i2s_audio_is_initialized()) {
        ESP_LOGW(TAG, "I2S audio system not initialized, initializing now...");
        esp_err_t ret = i2s_audio_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize I2S audio system: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // 2. 检查RX句柄
    i2s_chan_handle_t rx_handle = i2s_audio_get_rx_handle();
    if (!rx_handle) {
        ESP_LOGE(TAG, "Failed to get I2S RX handle");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ Using shared I2S RX channel (full-duplex mode)");
    
    // 3. 启用I2S RX通道
    esp_err_t ret = i2s_audio_enable_rx();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ I2S RX channel enabled");
    
    // 4. 创建麦克风监听任务（优先级4，低于NFC的5）
    BaseType_t task_ret = xTaskCreate(microphone_task, "inmp441_mic", 4096, NULL, 4, &g_mic_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create microphone task");
        i2s_audio_disable_rx();
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ Microphone task created");
    
    g_microphone_initialized = true;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ INMP441 Microphone Ready!");
    ESP_LOGI(TAG, "========================================");
    
    return ESP_OK;
}

/**
 * @brief 启用BLE音频流传输
 */
esp_err_t hw_microphone_enable_streaming(void (*callback)(const uint8_t *data, uint16_t len))
{
    if (!g_microphone_initialized) {
        ESP_LOGW(TAG, "Microphone not initialized");
        return ESP_FAIL;
    }
    
    g_audio_data_callback = callback;
    g_streaming_enabled = true;
    ESP_LOGI(TAG, "BLE audio streaming enabled (8kHz/16bit/Mono)");
    
    return ESP_OK;
}

/**
 * @brief 禁用BLE音频流传输
 */
esp_err_t hw_microphone_disable_streaming(void)
{
    if (!g_microphone_initialized) {
        ESP_LOGW(TAG, "Microphone not initialized");
        return ESP_FAIL;
    }
    
    g_streaming_enabled = false;
    g_audio_data_callback = NULL;
    ESP_LOGI(TAG, "BLE audio streaming disabled");
    
    return ESP_OK;
}

/**
 * @brief 获取声音电平
 * @return 声音电平 (0-100)
 */
uint8_t hw_microphone_get_sound_level(void)
{
    if (!g_microphone_initialized) {
        ESP_LOGW(TAG, "Microphone not initialized");
        return 0;
    }
    
    return g_current_sound_level;
}

/**
 * @brief 检查是否检测到声音
 * @return true=检测到声音, false=未检测到
 */
bool hw_microphone_is_sound_detected(void)
{
    if (!g_microphone_initialized) {
        return false;
    }
    
    return g_sound_detected;
}

/**
 * @brief 设置声音检测阈值
 * @param threshold 阈值 (0-100)
 */
esp_err_t hw_microphone_set_threshold(uint8_t threshold)
{
    if (!g_microphone_initialized) {
        ESP_LOGW(TAG, "Microphone not initialized");
        return ESP_FAIL;
    }
    
    if (threshold > 100) {
        threshold = 100;
    }
    
    g_sound_threshold = threshold;
    ESP_LOGI(TAG, "Set sound detection threshold: %d", threshold);
    
    return ESP_OK;
}

/**
 * @brief 重新测量噪声基线
 */
esp_err_t hw_microphone_calibrate(void)
{
    if (!g_microphone_initialized) {
        ESP_LOGW(TAG, "Microphone not initialized");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Recalibrating microphone...");
    measure_noise_floor();
    
    return ESP_OK;
}

/**
 * @brief 获取噪声基线
 */
uint32_t hw_microphone_get_noise_floor(void)
{
    return (uint32_t)g_noise_floor;
}

/**
 * @brief 反初始化麦克风（释放资源）
 */
esp_err_t hw_microphone_deinit(void)
{
    if (!g_microphone_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing INMP441 microphone...");
    
    // 停止任务
    if (g_mic_task != NULL) {
        vTaskDelete(g_mic_task);
        g_mic_task = NULL;
    }
    
    // 禁用I2S RX通道（但不删除，因为是共享的）
    i2s_audio_disable_rx();
    
    g_microphone_initialized = false;
    ESP_LOGI(TAG, "INMP441 microphone deinitialized");
    
    return ESP_OK;
}
