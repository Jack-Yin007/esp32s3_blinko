/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* Includes */
#include "audio.h"
#include "common.h"
#include "adpcm.h"  // ADPCM压缩
#include "drivers/i2s_audio.h"
#include "drivers/gatt_svc.h"  // For b11_send_audio_data()
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <math.h>  // 添加数学库支持fabsf函数

/* Private variables */
static bool audio_notify_enabled = false;
static uint16_t audio_conn_handle = 0;
static uint16_t audio_attr_handle = 0;
TaskHandle_t audio_task_handle = NULL;  // 非static，供self_test_trigger使用
static i2s_chan_handle_t rx_handle = NULL;
static bool recording_active = false;
static adpcm_state_t adpcm_encoder_state;  // ADPCM编码器状态

/* Private function declarations */
static void audio_record_task(void *arg);
static esp_err_t audio_send_notification(uint8_t *data, uint16_t len);
static void audio_check_gpio_config(void);

/* Audio GATT Service Definition */
static const ble_uuid16_t audio_svc_uuid = BLE_UUID16_INIT(AUDIO_SERVICE_UUID);
static const ble_uuid16_t audio_char_uuid = BLE_UUID16_INIT(AUDIO_CHAR_UUID);

/* Store the characteristic value handle */
static uint16_t audio_char_val_handle;

static int
audio_gatt_char_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def audio_gatt_svcs[] = {
    {
        /* Audio Service */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &audio_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Audio Data Characteristic */
            .uuid = &audio_char_uuid.u,
            .access_cb = audio_gatt_char_access,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE,
            .val_handle = &audio_char_val_handle,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },
    {
        0, /* No more services */
    },
};

/* I2S Configuration and Initialization */
esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "Initializing Audio (using shared I2S RX channel)");

    // 检查 I2S 全双工系统是否已初始化
    if (!i2s_audio_is_initialized()) {
        ESP_LOGW(TAG, "I2S audio system not initialized, initializing now...");
        esp_err_t ret = i2s_audio_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize I2S audio system: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    // 获取共享的 RX 句柄
    rx_handle = i2s_audio_get_rx_handle();
    if (rx_handle == NULL) {
        ESP_LOGE(TAG, "Failed to get I2S RX handle from shared system");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✓ Using shared I2S RX channel (8kHz Mono)");
    ESP_LOGI(TAG, "✓ Audio system ready for BLE streaming");
    
    return ESP_OK;
}

esp_err_t audio_start_recording(void)
{
    if (recording_active) {
        ESP_LOGW(TAG, "Audio recording already active");
        return ESP_OK;
    }

    if (rx_handle == NULL) {
        ESP_LOGE(TAG, "I2S not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting audio recording...");

    /* 初始化ADPCM编码器 */
    adpcm_init(&adpcm_encoder_state);
    ESP_LOGI(TAG, "✓ ADPCM encoder initialized (4:1 compression)");

    /* 🎤 启动Silent TX任务,为INMP441提供I2S时钟(BCLK/WS) */
    /* 注意: 扬声器保持SD=LOW(关闭状态),只有I2S时钟输出 */
    esp_err_t ret = i2s_audio_resume_silent_tx();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to resume Silent TX: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✓ Silent TX started for microphone clock");
    }
    
    /* Enable shared I2S RX channel */
    ret = i2s_audio_enable_rx();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Shared I2S RX channel enabled");

    /* 🔍 Test I2S RX immediately after enable */
    uint8_t test_buffer[64];
    size_t bytes_read = 0;
    ret = i2s_channel_read(rx_handle, test_buffer, sizeof(test_buffer), &bytes_read, pdMS_TO_TICKS(500));
    ESP_LOGI(TAG, "🔍 Test read result: ret=%d, bytes=%d", ret, bytes_read);
    if (ret == ESP_OK && bytes_read > 0) {
        ESP_LOGI(TAG, "✅ I2S RX is working! First samples: %d %d %d %d", 
                 ((int16_t*)test_buffer)[0], ((int16_t*)test_buffer)[1],
                 ((int16_t*)test_buffer)[2], ((int16_t*)test_buffer)[3]);
    } else {
        ESP_LOGW(TAG, "⚠️ I2S RX test failed: %s", esp_err_to_name(ret));
    }

    /* Set recording flag before creating task */
    recording_active = true;

    /* Create audio recording task with same config as gatt_server */
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        audio_record_task, 
        "audio_record", 
        4096,           // Stack size
        NULL, 
        5,              // Priority
        &audio_task_handle,
        1               // Core 1 (avoid conflict with BLE on core 0)
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio recording task");
        recording_active = false;
        i2s_audio_disable_rx();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "✓ Audio recording task created");
    ESP_LOGI(TAG, "✅ Audio recording started (8kHz mono → BLE)");
    return ESP_OK;
}

esp_err_t audio_set_gain(uint8_t gain_level)
{
    /* Update the gain multiplier (1-8 range, 2 is default) */
    if (gain_level < 1 || gain_level > 8) {
        ESP_LOGW(TAG, "Invalid gain level %d, using default 2", gain_level);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Audio gain set to %dx", gain_level);
    return ESP_OK;
}

esp_err_t audio_stop_recording(void)
{
    if (!recording_active) {
        ESP_LOGW(TAG, "Audio recording not active");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping audio recording...");

    /* Set flag to stop recording task gracefully */
    recording_active = false;
    
    /* 🔇 停止Silent TX任务,消除扬声器噪声 */
    esp_err_t ret_silent = i2s_audio_pause_silent_tx();
    if (ret_silent != ESP_OK) {
        ESP_LOGW(TAG, "Failed to pause Silent TX: %s", esp_err_to_name(ret_silent));
    } else {
        ESP_LOGI(TAG, "✓ Silent TX stopped - speaker silent");
    }    /* Give task time to exit gracefully */
    TaskHandle_t task_to_wait = audio_task_handle;
    if (task_to_wait != NULL) {
        ESP_LOGI(TAG, "Waiting for audio task to exit...");
        
        /* Wait for task to finish (max 1 second) */
        for (int i = 0; i < 20; i++) {
            vTaskDelay(pdMS_TO_TICKS(50));
            
            /* Check if task handle was cleared (task exited) */
            if (audio_task_handle == NULL) {
                ESP_LOGI(TAG, "Audio recording task exited");
                break;
            }
        }
        
        /* If task still exists after timeout, log warning but don't force delete */
        if (audio_task_handle != NULL) {
            ESP_LOGW(TAG, "Audio task didn't exit within timeout, may cause issues");
            /* DO NOT call vTaskDelete() here - task will clean up itself */
            audio_task_handle = NULL;  /* Clear handle to prevent double-delete */
        }
    }

    /* ⚠️ IMPORTANT: Keep I2S channels enabled for instant restart
     * Disabling I2S RX/TX would:
     * 1. Stop silent_tx_task → I2S clock stops
     * 2. Cause 100ms+ delay on next recording start
     * 3. May interfere with BLE operations during disable
     * 
     * Solution: Only stop the recording task, keep I2S running
     */
    ESP_LOGI(TAG, "✓ I2S channels remain enabled for instant restart");

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ Audio Recording Stopped");
    ESP_LOGI(TAG, "========================================");
    return ESP_OK;
}

static void audio_record_task(void *arg)
{
    size_t bytes_read;
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Audio Recording Task Started");
    ESP_LOGI(TAG, "  Sample Rate: 8 kHz");
    ESP_LOGI(TAG, "  Channels: Mono");
    ESP_LOGI(TAG, "  Bit Depth: 16-bit");
    ESP_LOGI(TAG, "  Buffer Size: %d bytes", I2S_READ_LEN);
    ESP_LOGI(TAG, "========================================");
    
    // Register this task with the watchdog timer
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register audio task with watchdog: %s", esp_err_to_name(wdt_ret));
    }
    
    uint8_t *i2s_read_buff = (uint8_t *) malloc(I2S_READ_LEN);
    if (i2s_read_buff == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio buffer");
        recording_active = false;
        esp_task_wdt_delete(NULL);  // Unregister from watchdog
        vTaskDelete(NULL);
        return;
    }
    
    /* Check GPIO configuration */
    audio_check_gpio_config();

    ESP_LOGI(TAG, "Starting audio capture loop...");
    ESP_LOGI(TAG, "rx_handle=%p, recording_active=%d, audio_notify_enabled=%d", 
             rx_handle, recording_active, audio_notify_enabled);
    uint32_t frame_count = 0;
    uint32_t timeout_count = 0;

    while (recording_active) {
        /* Feed the watchdog */
        esp_task_wdt_reset();
        
        /* Read data from shared I2S RX channel */
        esp_err_t ret = i2s_channel_read(rx_handle, i2s_read_buff, I2S_READ_LEN, &bytes_read, pdMS_TO_TICKS(100));
        
        frame_count++;
        
        if (ret == ESP_ERR_TIMEOUT) {
            /* I2S read timeout - log periodically */
            timeout_count++;
            if (timeout_count % 50 == 0) {  // Log every 5 seconds
                ESP_LOGW(TAG, "I2S read timeout x%lu, total frames=%lu", timeout_count, frame_count);
            }
            continue;
        } else if (ret == ESP_OK && bytes_read > 0) {
            /* Log successful read periodically */
            if (frame_count % 100 == 1) {  // Log every 10 seconds
                ESP_LOGI(TAG, "I2S read success: bytes=%d, frame=%lu", bytes_read, frame_count);
            }
            
            /* 
             * 🔧 数据格式处理：
             * - I2S RX配置为STEREO，读取的数据是立体声格式：L R L R L R...
             * - INMP441的L/R接3.3V，有效数据在右声道，左声道为0
             * - 需要提取右声道数据
             */
            int16_t *stereo_samples = (int16_t*)i2s_read_buff;
            uint16_t stereo_pair_count = bytes_read / 4;  // 每对 = L(2字节) + R(2字节)
            
            // 提取右声道到临时缓冲区
            static int16_t mono_buffer[I2S_READ_LEN / 4];
            for (uint16_t i = 0; i < stereo_pair_count; i++) {
                mono_buffer[i] = stereo_samples[i * 2 + 1];  // 只取右声道（奇数索引）
            }
            
            /* Process mono audio data and apply gain */
            int16_t *samples = mono_buffer;
            uint16_t sample_count = stereo_pair_count;
            
            /* Enhanced Audio Processing - Simplified for Better Performance */
            static float dc_filter_state = 0.0f;     // DC阻断滤波器状态
            static float hpf_prev_sample = 0.0f;     // 高通滤波器前一个样本
            // static float preemph_prev = 0.0f;        // 预加重滤波器状态 - 未使用
            static float noise_threshold_adaptive = AUDIO_NOISE_THRESHOLD; // 自适应噪声阈值
            
            /* 滤波器参数 */
            const float HPF_ALPHA = 0.9802f;         // 100Hz高通滤波器系数 (8kHz采样率)
            // const float PREEMPH_COEFF = 0.97f;    // 预加重系数 (降低强度，保留更自然的声音) - 未使用
            const float NOISE_ADAPT_RATE = 0.001f;   // 噪声阈值适应速率
            
            /* 音频质量改进处理循环 */
            for (int i = 0; i < sample_count; i++) {
                float sample = (float)samples[i];
                
                /* 1. DC阻断滤波器 (强力去除DC偏移) */
                dc_filter_state = AUDIO_DC_FILTER_ALPHA * dc_filter_state + sample - AUDIO_DC_FILTER_ALPHA * sample;
                sample = sample - dc_filter_state;
                
#if AUDIO_HPF_ENABLED
                /* 2. 100Hz高通滤波器 (去除低频噪声) */
                float hpf_output = HPF_ALPHA * (sample - hpf_prev_sample) + HPF_ALPHA * sample;
                hpf_prev_sample = sample;
                sample = hpf_output;
#endif

                /* 3. 先应用基础增益 (在噪声门之前放大信号) */
                sample *= AUDIO_GAIN_MULTIPLIER;

#if AUDIO_PREEMPHASIS
                /* 4. 预加重滤波器 (轻微提升高频，不要过度) */
                float preemph_output = sample - PREEMPH_COEFF * preemph_prev;
                preemph_prev = sample;
                sample = preemph_output;
#endif
                
                /* 5. 更宽松的噪声门 (在增益后应用，减少误判) */
                float abs_sample = fabsf(sample);
                
                /* 更新自适应噪声阈值 */
                if (abs_sample > noise_threshold_adaptive) {
                    noise_threshold_adaptive += NOISE_ADAPT_RATE * (abs_sample - noise_threshold_adaptive);
                } else {
                    noise_threshold_adaptive *= (1.0f - NOISE_ADAPT_RATE);
                }
                
                /* 应用非常宽松的噪声门 (只过滤极弱信号) */
                if (abs_sample < AUDIO_NOISE_THRESHOLD) {
                    sample = 0.0f;  // 静音处理
                }
                    
#if AUDIO_AGC_ENABLED
                /* 6. AGC增益控制 (产品板优化：平衡增益避免削波) */
                if (sample != 0.0f) {
                    static float agc_level = 15000.0f;  // 降低目标电平到15000
                    static float current_gain = 1.8f;   // 初始增益降到1.8
                    
                    /* 每32个样本更新一次AGC */
                    static int agc_counter = 0;
                    if (++agc_counter >= 32) {
                        agc_counter = 0;
                        float abs_sample_current = fabsf(sample);
                        if (abs_sample_current > 100.0f) {
                            float target_gain = agc_level / abs_sample_current;
                            if (target_gain > 5.0f) target_gain = 5.0f;  // 最大增益降到5.0
                            if (target_gain < 0.8f) target_gain = 0.8f;
                            current_gain = current_gain * 0.85f + target_gain * 0.15f;
                        }
                    }
                    sample *= current_gain;
                }
#endif
                
                /* 7. 最终软限幅 (防止削峰) */
                if (sample > 31000.0f) {
                    sample = 31000.0f + (sample - 31000.0f) * 0.2f;
                } else if (sample < -31000.0f) {
                    sample = -31000.0f + (sample + 31000.0f) * 0.2f;
                }
                
                /* 绝对限幅 */
                if (sample > 32767.0f) sample = 32767.0f;
                if (sample < -32768.0f) sample = -32768.0f;
                
                samples[i] = (int16_t)sample;
            }
            
            /* Debug: Check signal strength with improved detection */
            bool has_signal = false;
            int16_t max_sample = 0;
            int significant_samples = 0;
            
            for (int i = 0; i < sample_count; i++) {
                int16_t abs_sample = abs(samples[i]);
                if (abs_sample > max_sample) max_sample = abs_sample;
                
                /* Count samples above noise threshold (更严格的信号检测) */
                if (abs_sample > AUDIO_NOISE_THRESHOLD * 2) {  // 2倍噪声阈值检测有效信号
                    significant_samples++;
                }
            }
            
            /* 只有当有效信号样本数达到一定比例时才认为有信号 */
            if (significant_samples > (sample_count / 10)) {  // 至少10%的样本有效
                has_signal = true;
            }
            
            static int log_counter = 0;
            if (has_signal) {
                ESP_LOGI(TAG, "Audio signal detected! Bytes: %zu, Max: %d, Active samples: %d", 
                         bytes_read, max_sample, significant_samples);
            } else {
                log_counter++;
                if (log_counter % 100 == 0) {  // 进一步减少静音日志频率
                    ESP_LOGD(TAG, "Background noise (frames: %d, max: %d)", log_counter, max_sample);
                }
            }
            
            /* Send audio data via BLE if notifications are enabled */
            if (audio_notify_enabled && audio_conn_handle != 0) {
                /* Log every 50 frames (about 3 seconds at 8kHz) */
                if (frame_count % 50 == 0) {
                    ESP_LOGI(TAG, "📡 BLE Audio Streaming: Frame %lu, Signal: %s, Max: %d", 
                             frame_count, has_signal ? "YES" : "no", max_sample);
                }
                
#if AUDIO_USE_ADPCM
                /* 使用ADPCM压缩：16-bit PCM → 4-bit ADPCM (4:1压缩比) */
                static uint8_t adpcm_buffer[ADPCM_BUFFER_SIZE + 4];  // 压缩缓冲区，额外4字节用于header
                
                /* 编码为ADPCM */
                int adpcm_bytes = adpcm_encode(mono_buffer, sample_count, adpcm_buffer + 4, &adpcm_encoder_state);
                
                /* 添加header: 样本数 (2 bytes) + 原始字节数 (2 bytes) */
                adpcm_buffer[0] = (sample_count >> 8) & 0xFF;
                adpcm_buffer[1] = sample_count & 0xFF;
                adpcm_buffer[2] = (sample_count * 2 >> 8) & 0xFF;
                adpcm_buffer[3] = (sample_count * 2) & 0xFF;
                
                int total_bytes = adpcm_bytes + 4;
                
                /* 分包发送ADPCM数据 */
                uint16_t chunk_size = AUDIO_BUFFER_SIZE;  // 244 bytes per BLE packet
                for (size_t offset = 0; offset < total_bytes; offset += chunk_size) {
                    if (!recording_active) break;
                    
                    size_t remaining = total_bytes - offset;
                    size_t send_len = (remaining > chunk_size) ? chunk_size : remaining;
                    
                    if (send_len > 0) {
                        esp_err_t send_ret = audio_send_notification(adpcm_buffer + offset, send_len);
                        if (send_ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send ADPCM data");
                        }
                    }
                }
                
                /* Log compression ratio every 100 frames */
                if (frame_count % 100 == 0) {
                    int pcm_bytes = sample_count * 2;
                    float ratio = (float)pcm_bytes / (float)total_bytes;
                    ESP_LOGI(TAG, "🗜️  ADPCM: %d bytes → %d bytes (%.1fx compression)", 
                             pcm_bytes, total_bytes, ratio);
                }
#else
                /* 原始PCM数据发送（无压缩） */
                uint16_t mono_bytes = sample_count * 2;
                uint16_t chunk_size = AUDIO_BUFFER_SIZE;
                
                for (size_t offset = 0; offset < mono_bytes; offset += chunk_size) {
                    if (!recording_active) break;
                    
                    size_t remaining = mono_bytes - offset;
                    size_t send_len = (remaining > chunk_size) ? chunk_size : remaining;
                    
                    if (send_len % 2 != 0) {
                        send_len--;
                    }
                    
                    if (send_len > 0) {
                        esp_err_t send_ret = audio_send_notification((uint8_t*)(mono_buffer) + offset, send_len);
                        if (send_ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send audio data");
                        }
                    }
                }
#endif
            }
        } else {
            ESP_LOGW(TAG, "I2S read failed or no data: %s", esp_err_to_name(ret));
        }
        
        /* 🔧 I2S读取间隔：完全移除延迟，全速运行
         * - I2S_READ_LEN=512字节需要~16ms采集时间
         * - 无人工延迟，让I2S硬件自然限速
         * - CPU在等待I2S时会自动yield给其他任务
         */
        // vTaskDelay removed - no artificial throttling!
    }

    // Unregister from watchdog before exiting
    esp_task_wdt_delete(NULL);
    
    free(i2s_read_buff);
    ESP_LOGI(TAG, "Audio recording task exited");
    
    /* Clear handle before deleting to prevent double-delete */
    audio_task_handle = NULL;
    
    vTaskDelete(NULL);
}

static esp_err_t audio_send_notification(uint8_t *data, uint16_t len)
{
    if (!audio_notify_enabled || audio_conn_handle == 0) {
        ESP_LOGW(TAG, "Cannot send notification: enabled=%d, conn_handle=%d", 
                 audio_notify_enabled, audio_conn_handle);
        return ESP_ERR_INVALID_STATE;
    }

    /* Debug: Check data being sent */
    static int send_debug_counter = 0;
    if (send_debug_counter++ % 100 == 0) {  // Log every 100 sends
        int16_t *samples = (int16_t*)data;
        uint16_t sample_count = len / 2;
        ESP_LOGI(TAG, "BLE send: len=%d, samples=[%d,%d,%d,%d], handle=%d", 
                 len,
                 sample_count > 0 ? samples[0] : 0,
                 sample_count > 1 ? samples[1] : 0,
                 sample_count > 2 ? samples[2] : 0,
                 sample_count > 3 ? samples[3] : 0,
                 audio_attr_handle);
    }
    
    /* Print hex data for debugging - show first 16 bytes */
    if (send_debug_counter % 50 == 1) {  // Print hex every 50 sends
        ESP_LOGI(TAG, "Audio data HEX (%d bytes):", len);
        ESP_LOG_BUFFER_HEX(TAG, data, len > 32 ? 32 : len);  // Show first 32 bytes max
    }

    /* ✅ Use B11 audio data function instead of standard GATT notify */
    int rc = b11_send_audio_data(data, len);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to send audio data via B11: %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void audio_set_notify_enable(bool enable)
{
    audio_notify_enabled = enable;
    ESP_LOGI(TAG, "Audio notifications %s", enable ? "enabled" : "disabled");
    
    if (enable && !recording_active) {
        audio_start_recording();
    } else if (!enable && recording_active) {
        audio_stop_recording();
    }
}

void audio_set_connection_info(uint16_t conn_handle, uint16_t attr_handle)
{
    audio_conn_handle = conn_handle;
    audio_attr_handle = attr_handle;
    ESP_LOGI(TAG, "Audio connection info set: conn_handle=%d, attr_handle=%d", 
             conn_handle, attr_handle);
}

bool audio_is_recording(void)
{
    return recording_active;
}

static int
audio_gatt_char_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        ESP_LOGI(TAG, "Audio characteristic read");
        /* Return current audio status */
        {
            uint8_t status = recording_active ? 1 : 0;
            int rc = os_mbuf_append(ctxt->om, &status, sizeof(status));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        break;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        ESP_LOGI(TAG, "Audio characteristic write");
        /* Control recording start/stop */
        if (ctxt->om->om_len >= 1) {
            uint8_t command;
            ble_hs_mbuf_to_flat(ctxt->om, &command, 1, NULL);
            
            if (command == 1) {
                audio_start_recording();
            } else {
                audio_stop_recording();
            }
        }
        return 0;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static void audio_check_gpio_config(void) {
    ESP_LOGI(TAG, "=== Checking GPIO Configuration ===");
    ESP_LOGI(TAG, "INMP441 GPIO pins:");
    ESP_LOGI(TAG, "  SCK (Clock): GPIO %d", I2S_SCK_IO);
    ESP_LOGI(TAG, "  WS (Word Select): GPIO %d", I2S_WS_IO);  
    ESP_LOGI(TAG, "  SD (Serial Data): GPIO %d", I2S_SD_IO);
    ESP_LOGI(TAG, "  L/R: Connected to 3.3V for right channel");
    ESP_LOGI(TAG, "  VDD: Should be connected to 3.3V");
    ESP_LOGI(TAG, "  GND: Should be connected to GND");
}

int audio_gatt_svc_init(void)
{
    int rc = ble_gatts_count_cfg(audio_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT configuration: %d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(audio_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add audio GATT service: %d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "Audio GATT service initialized");
    return 0;
}