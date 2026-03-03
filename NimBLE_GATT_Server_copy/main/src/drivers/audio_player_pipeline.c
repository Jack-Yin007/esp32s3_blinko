/**
 * @file audio_player_pipeline.c
 * @brief WAV音频播放驱动 - 基于ESP-ADF Pipeline + Custom I2S Writer
 * 
 * 架构：
 * - 使用ESP-ADF Pipeline框架进行状态管理
 * - FatFS Reader → WAV Decoder → Custom I2S Writer
 * - Custom I2S Writer使用现有TX handle，不销毁RX channel
 * - 保持BLE录音功能持续工作
 * 
 * 功能：从Flash读取WAV文件并通过I2S播放
 * - 硬件：ESP32-S3 I2S → MAX98357A DAC → 扬声器
 * - 格式：支持8KHz/16KHz, 单声道/立体声, 16-bit PCM
 * - 路径：/flash/audio/{ID}.wav
 */

#include "drivers/hardware_interface.h"
#include "drivers/i2s_audio.h"
#include "drivers/custom_i2s_writer.h"
#include "drivers/aw9535.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "wav_decoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char *TAG = "WAVPlayer";

/* ==================== 全局变量 ==================== */
static bool g_audio_initialized = false;
static TaskHandle_t g_audio_task = NULL;
static QueueHandle_t g_audio_queue = NULL;

// 全局播放状态标志（可被trigger_detector访问以阻止播放期间的新触发）
bool g_is_playing = false;

/* ==================== 私有函数 ==================== */

/**
 * @brief 播放WAV文件（使用ESP-ADF Pipeline + Custom I2S Writer）
 * 
 * Pipeline架构：
 * FatFS Stream Reader → WAV Decoder → Custom I2S Writer
 * 
 * 优势：
 * 1. 使用现有TX handle，不销毁RX channel（BLE录音持续工作）
 * 2. ESP-ADF负责状态管理和资源清理
 * 3. 无需手动管理I2S驱动卸载/重装
 * 4. 正确的silence flushing和硬件控制
 */
static void play_wav_file(sound_effect_id_t wav_id)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "▶️ START PLAYBACK (Pipeline Mode): WAV ID=%d", wav_id);
    ESP_LOGI(TAG, "========================================");

    g_is_playing = true;
    ESP_LOGI(TAG, "🔒 Set g_is_playing = TRUE");

    // Pipeline架构：FatFS Reader → WAV Decoder → Custom I2S Writer
    audio_pipeline_handle_t pipeline = NULL;
    audio_element_handle_t fatfs_reader = NULL, wav_decoder = NULL, i2s_writer = NULL;
    audio_event_iface_handle_t evt = NULL;
    
    // 构建文件路径
    char filepath[64];
    snprintf(filepath, sizeof(filepath), "/flash/audio/%d.WAV", wav_id);

    // 步骤1: 创建Pipeline
    ESP_LOGI(TAG, "[1] Creating audio pipeline...");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    if (pipeline == NULL) {
        ESP_LOGE(TAG, "Failed to create pipeline");
        goto cleanup;
    }

    // 步骤2: 创建FatFS Stream Reader
    ESP_LOGI(TAG, "[2] Creating FatFS stream reader...");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_reader = fatfs_stream_init(&fatfs_cfg);
    if (fatfs_reader == NULL) {
        ESP_LOGE(TAG, "Failed to create FatFS reader");
        goto cleanup;
    }

    // 步骤3: 创建WAV Decoder
    ESP_LOGI(TAG, "[3] Creating WAV decoder...");
    wav_decoder_cfg_t wav_cfg = DEFAULT_WAV_DECODER_CONFIG();
    wav_decoder = wav_decoder_init(&wav_cfg);
    if (wav_decoder == NULL) {
        ESP_LOGE(TAG, "Failed to create WAV decoder");
        goto cleanup;
    }

    // 步骤4: 创建Custom I2S Writer
    ESP_LOGI(TAG, "[4] Creating custom I2S writer...");
    custom_i2s_writer_cfg_t writer_cfg = CUSTOM_I2S_WRITER_DEFAULT_CFG();
    writer_cfg.use_existing_handle = true;
    writer_cfg.pause_silent_tx = true;
    i2s_writer = custom_i2s_writer_init(&writer_cfg);
    if (i2s_writer == NULL) {
        ESP_LOGE(TAG, "Failed to create custom I2S writer");
        goto cleanup;
    }

    // 步骤5: 注册所有元素到Pipeline
    ESP_LOGI(TAG, "[5] Registering elements to pipeline...");
    audio_pipeline_register(pipeline, fatfs_reader, "file");
    audio_pipeline_register(pipeline, wav_decoder, "wav");
    audio_pipeline_register(pipeline, i2s_writer, "i2s");

    // 步骤6: 链接元素
    ESP_LOGI(TAG, "[6] Linking elements: file->wav->i2s");
    const char *link_tag[3] = {"file", "wav", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    // 步骤7: 设置文件URI
    ESP_LOGI(TAG, "[7] Setting URI: %s", filepath);
    audio_element_set_uri(fatfs_reader, filepath);

    // 步骤8: 设置事件监听器
    ESP_LOGI(TAG, "[8] Setting up event listener...");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);
    if (evt == NULL) {
        ESP_LOGE(TAG, "Failed to create event interface");
        goto cleanup;
    }
    audio_pipeline_set_listener(pipeline, evt);

    // 步骤9: 启动Pipeline
    ESP_LOGI(TAG, "[9] Starting audio pipeline...");
    audio_pipeline_run(pipeline);

    // 步骤10: 等待播放完成
    ESP_LOGI(TAG, "[10] Waiting for playback to finish...");
    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Event interface error: %s", esp_err_to_name(ret));
            break;
        }

        // 检查是否播放完成
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && 
            msg.source == (void *)i2s_writer &&
            msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
            (((int)msg.data == AEL_STATUS_STATE_STOPPED) || 
             ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGI(TAG, "✅ Playback finished");
            break;
        }
    }

cleanup:
    // 步骤11: 清理资源
    ESP_LOGI(TAG, "[11] Cleaning up pipeline resources...");
    
    if (pipeline != NULL) {
        audio_pipeline_stop(pipeline);
        audio_pipeline_wait_for_stop(pipeline);
        audio_pipeline_terminate(pipeline);

        if (fatfs_reader != NULL) {
            audio_pipeline_unregister(pipeline, fatfs_reader);
        }
        if (wav_decoder != NULL) {
            audio_pipeline_unregister(pipeline, wav_decoder);
        }
        if (i2s_writer != NULL) {
            audio_pipeline_unregister(pipeline, i2s_writer);
        }

        audio_pipeline_remove_listener(pipeline);
    }

    if (evt != NULL) {
        audio_event_iface_destroy(evt);
    }

    if (pipeline != NULL) {
        audio_pipeline_deinit(pipeline);
    }
    if (fatfs_reader != NULL) {
        audio_element_deinit(fatfs_reader);
    }
    if (wav_decoder != NULL) {
        audio_element_deinit(wav_decoder);
    }
    if (i2s_writer != NULL) {
        audio_element_deinit(i2s_writer);
    }

    // 重置播放状态
    g_is_playing = false;
    ESP_LOGI(TAG, "🔓 Set g_is_playing = FALSE");

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ EXIT PLAYBACK (Pipeline Mode)");
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief 音频播放任务
 */
static void audio_player_task(void *arg)
{
    sound_effect_id_t wav_id;
    
    ESP_LOGI(TAG, "WAV player task started (Pipeline mode)");
    
    while (1) {
        // 从队列接收播放请求
        if (xQueueReceive(g_audio_queue, &wav_id, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "🎵 Processing WAV request: %d", wav_id);
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
    ESP_LOGI(TAG, "Initializing WAV Player (Pipeline Mode)");
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
    // P1.2 = GAIN引脚：HIGH=+15dB, LOW=+9dB
    // P1.3 = SD引脚（Shutdown）：HIGH=正常工作, LOW=关断
    ESP_LOGI(TAG, "Configuring MAX98357A control pins...");
    
    // ⚠️ CRITICAL: Configure pins as OUTPUT first (AW9535 defaults to INPUT on power-up)
    ESP_LOGI(TAG, "🔧 Step 1: Configuring P1.2 and P1.3 as OUTPUT...");
    esp_err_t ret = aw9535_set_mode(10, AW9535_MODE_OUTPUT);  // P1.2 (GAIN)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.2 mode: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.2 (GAIN) configured as OUTPUT");
    }
    
    ret = aw9535_set_mode(11, AW9535_MODE_OUTPUT);  // P1.3 (SD_MODE)
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to set P1.3 mode: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ P1.3 (SD_MODE) configured as OUTPUT");
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));  // Allow mode change to settle
    
    // 2. 启用SD引脚（P1.3 = HIGH），退出关断模式
    ESP_LOGI(TAG, "🔧 Step 2: Setting P1.3 (SD_MODE) to HIGH...");
    ret = aw9535_set_level(11, AW9535_LEVEL_HIGH);  // P1.3 = Pin 11
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to enable SD pin: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ SD pin set command OK");
    }
    
    // MAX98357A需要至少10ms启动时间（数据手册要求）
    vTaskDelay(pdMS_TO_TICKS(20));  // 等待20ms确保完全启动
    
    // 3. 设置GAIN引脚（P1.2 = HIGH），最大增益 +15dB
    ESP_LOGI(TAG, "🔧 Step 3: Setting P1.2 (GAIN) to HIGH...");
    ret = aw9535_set_level(10, AW9535_LEVEL_HIGH);  // P1.2 = Pin 10
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "❌ Failed to set GAIN: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ GAIN set command OK");
    }
    
    // 创建音频播放队列
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
        8192,  // 增大栈以支持Pipeline
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
    ESP_LOGI(TAG, "  Mode: ESP-ADF Pipeline + Custom I2S Writer");
    ESP_LOGI(TAG, "  Source: /flash/audio/{ID}.wav");
    ESP_LOGI(TAG, "  Format: 8KHz/16KHz, 1/2-ch, 16-bit PCM");
    ESP_LOGI(TAG, "  RX Channel: Preserved (BLE recording continuous)");
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
    
    // 清空队列，只保留最新的请求（防止重复播放）
    sound_effect_id_t dummy;
    int cleared = 0;
    while (xQueueReceive(g_audio_queue, &dummy, 0) == pdTRUE) {
        cleared++;
    }
    if (cleared > 0) {
        ESP_LOGI(TAG, "🗑️ Cleared %d old requests, queuing WAV %d", cleared, wav_id);
    }
    
    // 发送到播放队列
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
    
    g_is_playing = false;
    
    return ESP_OK;
}

/**
 * @brief 设置音量（预留接口，MAX98357A通过硬件增益控制）
 */
esp_err_t hw_audio_set_volume(uint8_t volume)
{
    if (!g_audio_initialized) {
        ESP_LOGW(TAG, "WAV player not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // MAX98357A的音量通过GAIN引脚控制（硬件设置）
    // 软件层面可以通过调整PCM数据幅度实现音量控制（在custom_i2s_writer中实现）
    ESP_LOGI(TAG, "Volume control: %d%% (implemented via PCM scaling in custom_i2s_writer)", volume);
    
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
    ESP_LOGI(TAG, "Testing WAV playback (Pipeline mode)...");
    ESP_LOGI(TAG, "========================================");
    
    // 测试播放前3个WAV文件
    for (uint8_t i = 1; i <= 3; i++) {
        ESP_LOGI(TAG, "Testing WAV file: /flash/audio/%d.wav", i);
        
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
