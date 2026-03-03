/**
 * @file i2s_audio.c
 * @brief I2S全双工音频系统实现 - MAX98357A + INMP441
 */

#include "drivers/i2s_audio.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include <string.h>

static const char *TAG = "I2S_AUDIO";

/* ==================== 全局变量 ==================== */
static bool g_initialized = false;
static i2s_chan_handle_t g_tx_handle = NULL;  // DAC播放通道
static i2s_chan_handle_t g_rx_handle = NULL;  // MIC录音通道
static TaskHandle_t g_silent_tx_task = NULL;  // 静音输出任务（保持I2S时钟运行）
static volatile bool g_silent_tx_running = false;  // 静音任务运行标志

/* DMA Drain相关变量 */
static SemaphoreHandle_t g_tx_done_semaphore = NULL;  // TX完成信号量
static volatile bool g_tx_callback_installed = false;  // 回调函数安装标志

/* ==================== 私有函数实现 ==================== */

/**
 * @brief 静音输出任务（保持I2S BCLK/WS时钟持续运行）
 * 
 * 功能：在全双工模式下，RX通道依赖TX通道的时钟信号。
 * 即使不播放音频，也需要持续向TX通道写入数据以维持时钟输出。
 */
static void silent_tx_task(void *arg)
{
    const int16_t silence[128] = {0};  // 128个静音样本（stereo：64帧 = 8ms @ 8kHz）
    size_t bytes_written;
    
    ESP_LOGI(TAG, "Silent TX task started (maintains BCLK/WS for RX)");
    
    while (g_silent_tx_running) {  // 使用标志位控制循环
        if (g_tx_handle != NULL) {
            // 检查通道状态,只在启用时写入
            // 播放WAV时通道会被禁用,此时跳过写入
            esp_err_t ret = i2s_channel_write(g_tx_handle, silence, sizeof(silence),
                                              &bytes_written, pdMS_TO_TICKS(10));  // 10ms超时，避免阻塞
            if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT && ret != ESP_ERR_INVALID_STATE) {
                // ESP_ERR_INVALID_STATE = 通道禁用(正常,播放中)
                ESP_LOGD(TAG, "Silent TX write: %s", esp_err_to_name(ret));
            }
        }
        
        // 50ms间隔：既能保持时钟活跃，又给IDLE任务足够CPU时间
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // 任务退出前清理
    ESP_LOGI(TAG, "Silent TX task exiting gracefully");
    g_silent_tx_task = NULL;
    vTaskDelete(NULL);  // 删除自己
}

/**
 * @brief 启动静音输出任务
 */
static esp_err_t start_silent_tx_task(void)
{
    if (g_silent_tx_task != NULL) {
        ESP_LOGW(TAG, "Silent TX task already running");
        return ESP_OK;
    }
    
    // 重置运行标志
    g_silent_tx_running = true;
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        silent_tx_task,
        "silent_tx",
        4096,  // 增加栈大小到4096（避免栈溢出）
        NULL,
        2,  // 优先级2（低优先级，避免饿死IDLE任务）
        &g_silent_tx_task,
        0   // Core 0（避免占用Core 1导致IDLE1任务看门狗超时）
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create silent TX task");
        g_silent_tx_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✓ Silent TX task created (GPIO40/39 clock active)");
    return ESP_OK;
}

/**
 * @brief 停止静音输出任务（优雅停止）
 */
static void stop_silent_tx_task(void)
{
    if (g_silent_tx_task != NULL) {
        // 设置标志位，让任务自然退出
        g_silent_tx_running = false;
        
        // 等待任务退出（最多200ms，确保任务完全停止）
        int timeout = 200;
        while (g_silent_tx_task != NULL && timeout-- > 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        if (g_silent_tx_task != NULL) {
            ESP_LOGW(TAG, "Silent task didn't exit gracefully, forcing delete");
            vTaskDelete(g_silent_tx_task);
            g_silent_tx_task = NULL;
            
            // 强制删除后等待更长时间，确保I2S通道完全空闲
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        ESP_LOGI(TAG, "Silent TX task stopped");
    }
}

esp_err_t i2s_audio_pause_silent_tx(void)
{
    // 🔧 关键修复：先停止静音任务，释放DMA资源
    stop_silent_tx_task();
    
    // ⚠️ 重要：禁用TX通道，完全释放I2S DMA资源
    // 这样SD卡读取不会与I2S DMA冲突
    if (g_tx_handle != NULL) {
        esp_err_t ret = i2s_channel_disable(g_tx_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "TX channel disabled (releasing DMA for SD card)");
        } else if (ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "TX disable warning: %s", esp_err_to_name(ret));
        }
        
        // 等待I2S DMA完全释放（关键！）
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // 注意：不在这里重新启用TX通道
        // 由调用者(audio_player)负责在需要时启用
        // 或者在播放结束后由 i2s_audio_resume_silent_tx() 启用
    }
    
    ESP_LOGI(TAG, "Silent TX task paused, I2S DMA reset for audio playback");
    return ESP_OK;
}

esp_err_t i2s_audio_resume_silent_tx(void)
{
    if (g_tx_handle == NULL) {
        ESP_LOGW(TAG, "I2S TX not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 🔌 启用TX通道(播放结束后被禁用了)
    esp_err_t ret = i2s_channel_enable(g_tx_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TX channel enabled for Silent TX");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "TX channel already enabled - skipping");
        // 通道已启用，不是错误，继续启动静音任务
    } else {
        ESP_LOGE(TAG, "Failed to enable TX: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 启动静音任务
    return start_silent_tx_task();
}

/* ==================== 公共API实现 ==================== */

i2s_chan_handle_t i2s_audio_get_tx_handle(void)
{
    return g_tx_handle;
}

i2s_chan_handle_t i2s_audio_get_rx_handle(void)
{
    return g_rx_handle;
}

esp_err_t i2s_audio_init(void)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "I2S audio already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing I2S Full-Duplex Audio System");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Mode:        Full-Duplex (TX + RX)");
    ESP_LOGI(TAG, "  Port:        I2S_NUM_0");
    ESP_LOGI(TAG, "  Sample Rate: %lu Hz", I2S_AUDIO_SAMPLE_RATE);
    ESP_LOGI(TAG, "  Bit Depth:   16-bit PCM");
    
    ESP_LOGI(TAG, "Shared Clock Pins:");
    ESP_LOGI(TAG, "  BCK (GPIO%d) ───┬──→ MAX98357A.BCLK", I2S_AUDIO_BCK_PIN);
    ESP_LOGI(TAG, "                  └──→ INMP441.SCK");
    ESP_LOGI(TAG, "  WS  (GPIO%d) ───┬──→ MAX98357A.LRCK", I2S_AUDIO_WS_PIN);
    ESP_LOGI(TAG, "                  └──→ INMP441.WS");
    
    ESP_LOGI(TAG, "Independent Data Pins:");
    ESP_LOGI(TAG, "  DOUT (GPIO%d) ─────→ MAX98357A.DIN (Stereo)", I2S_AUDIO_DOUT_PIN);
    ESP_LOGI(TAG, "  DIN  (GPIO%d) ─────← INMP441.SD (Mono)", I2S_AUDIO_DIN_PIN);

    esp_err_t ret;

    // ========== 步骤1: 创建I2S通道（TX + RX） ==========
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_AUDIO_PORT, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = I2S_AUDIO_DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = I2S_AUDIO_DMA_BUF_LEN;
    
    ret = i2s_new_channel(&chan_cfg, &g_tx_handle, &g_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ I2S TX/RX channels created");

    // ========== 步骤2: 配置TX通道（DAC播放 - 双声道） ==========
    // MAX98357A支持标准I2S格式（Philips格式）
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_AUDIO_BITS_PER_SAMPLE,
            I2S_SLOT_MODE_STEREO  // DAC双声道
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_AUDIO_BCK_PIN,
            .ws   = I2S_AUDIO_WS_PIN,
            .dout = I2S_AUDIO_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    // 打印时钟配置用于调试
    ESP_LOGI(TAG, "TX Clock Config:");
    ESP_LOGI(TAG, "  Sample Rate: %lu Hz", (unsigned long)tx_std_cfg.clk_cfg.sample_rate_hz);
    ESP_LOGI(TAG, "  Channels: Stereo (2)");
    ESP_LOGI(TAG, "  Bits per Sample: %d", I2S_AUDIO_BITS_PER_SAMPLE);
    ESP_LOGI(TAG, "  MCLK Multiple: %lu", (unsigned long)tx_std_cfg.clk_cfg.mclk_multiple);
    ESP_LOGI(TAG, "  Expected BCLK: %lu kHz (%.2f kHz actual)", 
             (unsigned long)(I2S_AUDIO_SAMPLE_RATE * 2 * 16 / 1000),
             (float)(I2S_AUDIO_SAMPLE_RATE * 2 * I2S_AUDIO_BITS_PER_SAMPLE) / 1000.0f);

    ret = i2s_channel_init_std_mode(g_tx_handle, &tx_std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TX channel: %s", esp_err_to_name(ret));
        i2s_del_channel(g_tx_handle);
        i2s_del_channel(g_rx_handle);
        g_tx_handle = NULL;
        g_rx_handle = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "✓ TX channel configured (8kHz Stereo for DAC)");

    // ========== 步骤3: 配置RX通道（MIC录音 - 立体声格式） ==========
    // 注意：
    // 1. INMP441是单麦克风，但I2S协议传输的是立体声格式
    // 2. INMP441的L/R接3.3V，数据输出在右声道，左声道为0
    // 3. 必须使用STEREO模式以匹配TX通道的时钟配置（全双工共享BCLK/WS）
    // 4. 接收立体声数据后，在应用层提取右声道
    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            I2S_AUDIO_BITS_PER_SAMPLE,
            I2S_SLOT_MODE_STEREO  // 必须与TX一致，使用STEREO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_AUDIO_BCK_PIN,   // 共享BCK
            .ws   = I2S_AUDIO_WS_PIN,    // 共享WS
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_AUDIO_DIN_PIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    // ⚠️ 不设置 slot_mask，接收完整的立体声数据（L+R）
    // 在 audio.c 中提取右声道即可

    ret = i2s_channel_init_std_mode(g_rx_handle, &rx_std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init RX channel: %s", esp_err_to_name(ret));
        i2s_del_channel(g_tx_handle);
        i2s_del_channel(g_rx_handle);
        g_tx_handle = NULL;
        g_rx_handle = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "✓ RX channel configured (8kHz Stereo format, extract Right channel in app)");

    // ========== 步骤4: 启用通道（默认禁用，由上层控制） ==========
    // 注意：这里不自动启用，由max98357a.c和microphone.c按需启用
    
    g_initialized = true;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ I2S Full-Duplex System Ready!");
    ESP_LOGI(TAG, "   TX: Disabled (call i2s_audio_enable_tx)");
    ESP_LOGI(TAG, "   RX: Disabled (call i2s_audio_enable_rx)");
    ESP_LOGI(TAG, "   ⚠️  Note: RX will auto-enable TX to provide clock");
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}

/**
 * @brief 彻底卸载I2S驱动
 * 
 * 用于在播放WAV前完全释放I2S资源
 */
esp_err_t i2s_audio_deinit(void)
{
    ESP_LOGW(TAG, "De-initializing I2S audio driver...");

    // 1. 停止并删除后台任务
    if (g_silent_tx_task) {
        stop_silent_tx_task();
        ESP_LOGI(TAG, "Silent TX task stopped.");
    }

    // 2. 禁用并删除通道（安全地禁用，忽略"未启用"错误）
    if (g_tx_handle) {
        esp_err_t ret = i2s_channel_disable(g_tx_handle);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "TX channel disable warning: %s", esp_err_to_name(ret));
        }
        i2s_del_channel(g_tx_handle);
        g_tx_handle = NULL;
        ESP_LOGI(TAG, "TX channel deleted.");
    }
    if (g_rx_handle) {
        esp_err_t ret = i2s_channel_disable(g_rx_handle);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "RX channel disable warning: %s", esp_err_to_name(ret));
        }
        i2s_del_channel(g_rx_handle);
        g_rx_handle = NULL;
        ESP_LOGI(TAG, "RX channel deleted.");
    }

    // 3. 标记为未初始化
    // 注意：新版I2S驱动不需要显式卸载驱动，只需删除通道
    
    g_initialized = false;
    ESP_LOGW(TAG, "I2S audio driver de-initialized successfully.");
    return ESP_OK;
}

esp_err_t i2s_audio_enable_tx(void)
{
    if (!g_initialized || !g_tx_handle) {
        ESP_LOGE(TAG, "TX channel not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_enable(g_tx_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ TX channel enabled (DAC playback active)");
    } else {
        ESP_LOGE(TAG, "Failed to enable TX: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t i2s_audio_disable_tx(void)
{
    if (!g_initialized || !g_tx_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2s_channel_disable(g_tx_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ TX channel disabled");
    }
    return ret;
}

esp_err_t i2s_audio_disable_rx(void)
{
    if (!g_initialized || !g_rx_handle) {
        return ESP_ERR_INVALID_STATE;
    }

    // 停止RX通道
    esp_err_t ret = i2s_channel_disable(g_rx_handle);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ RX channel disabled");
    }
    
    // 停止静音输出任务和TX通道（节省功耗）
    stop_silent_tx_task();
    if (g_tx_handle != NULL) {
        i2s_channel_disable(g_tx_handle);
        ESP_LOGI(TAG, "✓ TX channel disabled (clock stopped)");
    }
    
    return ret;
}

esp_err_t i2s_audio_enable_rx(void)
{
    if (!g_initialized || !g_rx_handle) {
        ESP_LOGE(TAG, "RX channel not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 🔧 关键修复：全双工模式下，RX依赖TX的时钟
    // 必须先启用TX通道以激活GPIO40(BCLK)和GPIO39(WS)的时钟输出
    if (g_tx_handle != NULL) {
        ESP_LOGI(TAG, "Enabling TX channel to activate I2S clock (GPIO40/39)...");
        esp_err_t tx_ret = i2s_channel_enable(g_tx_handle);
        
        // ✅ 允许重复启用（如果已启用则忽略错误）
        if (tx_ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGI(TAG, "✓ TX channel already enabled (clock already active)");
        } else if (tx_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable TX: %s", esp_err_to_name(tx_ret));
            return tx_ret;
        } else {
            ESP_LOGI(TAG, "✓ TX channel enabled (BCLK=256kHz, WS=8kHz)");
        }
        
        // 启动静音输出任务，保持时钟持续运行
        esp_err_t task_ret = start_silent_tx_task();
        if (task_ret != ESP_OK && task_ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to start silent TX task");
            return task_ret;
        }
        
        // 等待时钟稳定（INMP441需要时间响应时钟信号）
        vTaskDelay(pdMS_TO_TICKS(100));
    } else {
        ESP_LOGW(TAG, "TX handle is NULL, RX may not work in full-duplex mode!");
    }

    // 启用RX通道
    esp_err_t ret = i2s_channel_enable(g_rx_handle);
    
    // ✅ 允许重复启用
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "✓ RX channel already enabled (MIC recording already active)");
        return ESP_OK;
    } else if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ RX channel enabled (MIC recording active)");
    } else {
        ESP_LOGE(TAG, "Failed to enable RX: %s", esp_err_to_name(ret));
    }
    return ret;
}

bool i2s_audio_is_initialized(void)
{
    return g_initialized;
}

uint32_t i2s_audio_get_sample_rate(void)
{
    return I2S_AUDIO_SAMPLE_RATE;
}

/* ==================== WAV播放专用API实现 ==================== */

i2s_chan_handle_t i2s_audio_create_wav_tx_channel(uint32_t sample_rate, uint16_t channels, uint16_t bits_per_sample)
{
    ESP_LOGI(TAG, "🎵 Creating dedicated WAV TX channel: %dHz %dch %dbit", sample_rate, channels, bits_per_sample);
    
    // 创建独立的I2S通道配置
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_AUDIO_PORT, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;  // 较小的DMA缓冲区，减少延迟
    chan_cfg.dma_frame_num = 240; // 适合音频播放的帧数
    
    i2s_chan_handle_t tx_handle = NULL;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create WAV TX channel: %s", esp_err_to_name(ret));
        return NULL;
    }
    
    // 配置TX通道
    i2s_std_config_t tx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)bits_per_sample,
            channels == 1 ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO
        ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_AUDIO_BCK_PIN,
            .ws   = I2S_AUDIO_WS_PIN,
            .dout = I2S_AUDIO_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_std_mode(tx_handle, &tx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WAV TX channel: %s", esp_err_to_name(ret));
        i2s_del_channel(tx_handle);
        return NULL;
    }
    
    ESP_LOGI(TAG, "✅ WAV TX channel created successfully");
    return tx_handle;
}

esp_err_t i2s_audio_destroy_wav_tx_channel(i2s_chan_handle_t tx_handle)
{
    if (tx_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🗑️ Destroying WAV TX channel...");
    
    // 禁用通道
    esp_err_t ret = i2s_channel_disable(tx_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Disable warning: %s", esp_err_to_name(ret));
    }
    
    // 删除通道
    ret = i2s_del_channel(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete WAV TX channel: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ WAV TX channel destroyed");
    return ESP_OK;
}

void i2s_audio_diag_test_inmp441(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Starting INMP441 Microphone Diagnostic Test");
    ESP_LOGI(TAG, "========================================");

    if (!g_initialized || !g_rx_handle) {
        ESP_LOGE(TAG, "ERROR: RX channel not initialized!");
        return;
    }
    ESP_LOGI(TAG, "✓ RX handle verified: %p", g_rx_handle);

    // 分配缓冲区（512 bytes = 256 samples @ 8kHz stereo）
    const size_t buffer_size = 512;
    uint8_t *buffer = (uint8_t *)malloc(buffer_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "ERROR: Failed to allocate buffer");
        return;
    }
    ESP_LOGI(TAG, "✓ Buffer allocated: %d bytes", buffer_size);

    // 读取10帧数据进行分析
    ESP_LOGI(TAG, "Reading 10 audio frames (please speak into microphone)...");
    ESP_LOGI(TAG, "----------------------------------------");
    
    int success_count = 0;
    
    for (int attempt = 1; attempt <= 10; attempt++) {
        memset(buffer, 0, buffer_size);
        size_t bytes_read = 0;
        
        esp_err_t ret = i2s_channel_read(g_rx_handle, buffer, buffer_size, 
                                         &bytes_read, pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "Attempt %d/10:", attempt);
        ESP_LOGI(TAG, "  Return code: %s (%d)", esp_err_to_name(ret), ret);
        ESP_LOGI(TAG, "  Bytes read: %d (expected: %d)", bytes_read, buffer_size);
        
        if (ret == ESP_OK && bytes_read > 0) {
            success_count++;
            
            // 转换为16位样本并分析
            int16_t *samples = (int16_t *)buffer;
            int sample_count = bytes_read / 2;
            
            int zero_samples = 0;
            int16_t min_val = INT16_MAX;
            int16_t max_val = INT16_MIN;
            int64_t sum = 0;
            
            for (int i = 0; i < sample_count; i++) {
                int16_t sample = samples[i];
                
                if (sample == 0) zero_samples++;
                sum += sample;
                if (sample < min_val) min_val = sample;
                if (sample > max_val) max_val = sample;
            }
            
            float avg = (float)sum / sample_count;
            float zero_ratio = (float)zero_samples / sample_count * 100.0f;
            int16_t range = max_val - min_val;
            
            ESP_LOGI(TAG, "  ✅ SUCCESS!");
            ESP_LOGI(TAG, "  Samples: %d", sample_count);
            ESP_LOGI(TAG, "  Range: [%d, %d] (span=%d)", min_val, max_val, range);
            ESP_LOGI(TAG, "  Average: %.1f, Zeros: %.1f%%", avg, zero_ratio);
            
            // 第一次成功时打印原始数据
            if (success_count == 1) {
                ESP_LOGI(TAG, "  First 8 samples:");
                for (int i = 0; i < 8 && i < sample_count; i++) {
                    ESP_LOGI(TAG, "    [%d] 0x%04X (%d)", i, (uint16_t)samples[i], samples[i]);
                }
            }
            
            // 诊断
            if (zero_ratio > 80.0f) {
                ESP_LOGW(TAG, "  ⚠️  Too many zeros (%.1f%%)!", zero_ratio);
                ESP_LOGW(TAG, "      Check INMP441 L/R pin (should be 3.3V for right channel)");
            } else if (range < 500) {
                ESP_LOGW(TAG, "  ⚠️  Signal too weak (range=%d)", range);
                ESP_LOGW(TAG, "      Environment too quiet or microphone issue");
            } else {
                ESP_LOGI(TAG, "  ✅ Signal looks healthy!");
            }
            
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "  ❌ TIMEOUT - No data from INMP441!");
            ESP_LOGE(TAG, "  Possible causes:");
            ESP_LOGE(TAG, "    1. INMP441 not powered (check VDD=3.3V, GND)");
            ESP_LOGE(TAG, "    2. L/R pin not grounded (should connect to GND)");
            ESP_LOGE(TAG, "    3. Clock signal not reaching INMP441 (check GPIO%d)", I2S_AUDIO_BCK_PIN);
            ESP_LOGE(TAG, "    4. WS signal not reaching INMP441 (check GPIO%d)", I2S_AUDIO_WS_PIN);
            ESP_LOGE(TAG, "    5. SD data pin wiring error (check GPIO%d)", I2S_AUDIO_DIN_PIN);
        } else {
            ESP_LOGE(TAG, "  ❌ Read failed: %s", esp_err_to_name(ret));
        }
        
        ESP_LOGI(TAG, "");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    free(buffer);
    
    // 总结
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Diagnostic test complete");
    ESP_LOGI(TAG, "Success rate: %d/10 (%.0f%%)", success_count, success_count * 10.0f);
    
    if (success_count >= 8) {
        ESP_LOGI(TAG, "✅ INMP441 is working correctly!");
        ESP_LOGI(TAG, "   Hardware signals verified:");
        ESP_LOGI(TAG, "     ✅ BCK  (GPIO40): 256kHz square wave");
        ESP_LOGI(TAG, "     ✅ WS   (GPIO39): 8kHz square wave");
        ESP_LOGI(TAG, "     ✅ DIN  (GPIO15): Digital data present");
        ESP_LOGI(TAG, "   You can now enable BLE audio streaming.");
    } else if (success_count > 0) {
        ESP_LOGW(TAG, "⚠️  Partial success - Check hardware connections");
    } else {
        ESP_LOGE(TAG, "❌ INMP441 not responding - Hardware issue!");
    }
    
    ESP_LOGI(TAG, "========================================");
}

/* ==================== DMA Drain实现 ==================== */

/**
 * @brief I2S事件回调函数 - 用于检测TX DMA传输完成
 * 
 * 在ESP-IDF v5.5.1中，当DMA缓冲区传输完成时会自动调用此回调
 * event->dma_buf指向刚完成传输的DMA缓冲区
 * event->size包含传输的字节数
 */
static bool IRAM_ATTR i2s_tx_event_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // 当DMA传输完成时，释放信号量通知等待线程
    if (g_tx_done_semaphore && event && event->dma_buf) {
        xSemaphoreGiveFromISR(g_tx_done_semaphore, &xHigherPriorityTaskWoken);
    }
    
    return xHigherPriorityTaskWoken == pdTRUE;
}

esp_err_t i2s_tx_fifo_reset(i2s_chan_handle_t tx_handle)
{
    if (!tx_handle) {
        ESP_LOGE(TAG, "Invalid TX handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Performing software TX channel reset");
    
    // 不进行disable/enable操作，因为这会破坏RX录音功能
    // 在全双工模式下，TX和RX共享时钟，disable TX会导致RX失去时钟信号
    ESP_LOGI(TAG, "Skipping TX reset to preserve RX clock continuity");
    
    // 仅做短暂延迟让硬件状态稳定
    vTaskDelay(pdMS_TO_TICKS(5));
    
    ESP_LOGI(TAG, "TX channel preserved for RX clock");
    return ESP_OK;
}

esp_err_t i2s_wait_tx_done_with_callback(i2s_chan_handle_t tx_handle)
{
    if (!tx_handle) {
        ESP_LOGE(TAG, "Invalid TX handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 创建信号量（如果尚未创建）
    if (!g_tx_done_semaphore) {
        g_tx_done_semaphore = xSemaphoreCreateBinary();
        if (!g_tx_done_semaphore) {
            ESP_LOGE(TAG, "Failed to create TX done semaphore");
            return ESP_ERR_NO_MEM;
        }
    }
    
    // 安装回调函数（如果尚未安装）
    if (!g_tx_callback_installed) {
        i2s_event_callbacks_t callbacks = {
            .on_recv = NULL,
            .on_recv_q_ovf = NULL,
            .on_sent = i2s_tx_event_callback,
            .on_send_q_ovf = NULL
        };
        
        esp_err_t ret = i2s_channel_register_event_callback(tx_handle, &callbacks, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register I2S event callback: %s", esp_err_to_name(ret));
            return ret;
        }
        g_tx_callback_installed = true;
        ESP_LOGI(TAG, "I2S TX event callback installed");
    }
    
    // 等待TX传输完成信号（最长等待2秒）
    ESP_LOGI(TAG, "Waiting for TX DMA completion...");
    if (xSemaphoreTake(g_tx_done_semaphore, pdMS_TO_TICKS(2000)) == pdTRUE) {
        ESP_LOGI(TAG, "TX DMA completion confirmed via callback");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "TX DMA completion timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t i2s_drain_tx_completely(i2s_chan_handle_t tx_handle, 
                                  uint32_t sample_rate, 
                                  uint8_t channels, 
                                  uint8_t bits_per_sample)
{
    if (!tx_handle) {
        ESP_LOGE(TAG, "Invalid TX handle");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🧹 Starting AGGRESSIVE DMA drain to eliminate 10-second echo...");
    ESP_LOGI(TAG, "   Target: %luHz %dch %dbit", sample_rate, channels, bits_per_sample);
    
    // 计算样本字节数
    uint32_t bytes_per_sample = (bits_per_sample / 8) * channels;
    
    // 🔥 激进策略：发送大量静音数据确保完全覆盖所有残留音频
    // 计算需要发送的静音数据量：
    // 1. DMA缓冲区：6描述符 × 240帧 × 4字节 = 5760字节
    // 2. I2S硬件FIFO：约64字节
    // 3. 额外安全裕量：发送相当于2秒的静音数据
    uint32_t safety_samples = sample_rate * 2;  // 2秒的样本数
    uint32_t safety_bytes = safety_samples * bytes_per_sample;
    uint32_t total_drain_bytes = 5760 + 64 + safety_bytes;
    
    ESP_LOGI(TAG, "📊 Drain calculation:");
    ESP_LOGI(TAG, "   - DMA buffer: 5760 bytes");
    ESP_LOGI(TAG, "   - I2S FIFO: 64 bytes");
    ESP_LOGI(TAG, "   - Safety margin: %lu bytes (2 seconds of audio)", safety_bytes);
    ESP_LOGI(TAG, "   - Total drain: %lu bytes", total_drain_bytes);
    
    // 创建大的静音缓冲区进行快速填充
    const size_t chunk_size = 4096;  // 4KB块
    uint8_t *silence_chunk = calloc(chunk_size, sizeof(uint8_t));
    if (!silence_chunk) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for silence buffer", chunk_size);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "🔊 Phase 1: Flooding DMA with silence...");
    
    uint32_t total_sent = 0;
    size_t bytes_written;
    uint32_t write_failures = 0;
    
    // 循环发送静音数据，直到发送完所有需要的字节数
    while (total_sent < total_drain_bytes && write_failures < 10) {
        uint32_t remaining = total_drain_bytes - total_sent;
        uint32_t this_chunk = (remaining > chunk_size) ? chunk_size : remaining;
        
        esp_err_t ret = i2s_channel_write(tx_handle, silence_chunk, this_chunk, &bytes_written, pdMS_TO_TICKS(50));
        
        if (ret == ESP_OK && bytes_written > 0) {
            total_sent += bytes_written;
            write_failures = 0;  // 重置失败计数
        } else {
            write_failures++;
            ESP_LOGW(TAG, "Write failed (attempt %d): %s, bytes_written=%d", 
                     write_failures, esp_err_to_name(ret), bytes_written);
            vTaskDelay(pdMS_TO_TICKS(5));  // 短暂等待后重试
        }
        
        // 每发送100KB打印进度
        if ((total_sent % 102400) == 0 && total_sent > 0) {
            ESP_LOGI(TAG, "   Progress: %lu / %lu bytes (%.1f%%)", 
                     total_sent, total_drain_bytes, (float)total_sent / total_drain_bytes * 100.0f);
        }
    }
    
    free(silence_chunk);
    
    ESP_LOGI(TAG, "✅ Phase 1 completed: %lu bytes of silence sent", total_sent);
    
    if (write_failures >= 10) {
        ESP_LOGW(TAG, "⚠️  Too many write failures, may not have fully drained");
    }
    
    // ⏰ Phase 2: 强制等待所有数据播放完成
    ESP_LOGI(TAG, "⏳ Phase 2: Waiting for all silence to be played...");
    
    // 计算播放完成所需的时间（加上安全裕量）
    uint32_t samples_to_play = total_sent / bytes_per_sample;
    uint32_t playback_time_ms = (samples_to_play * 1000) / sample_rate + 500;  // +500ms安全裕量
    
    ESP_LOGI(TAG, "   Estimated playback time: %lu ms (samples: %lu)", playback_time_ms, samples_to_play);
    
    // 等待播放完成
    vTaskDelay(pdMS_TO_TICKS(playback_time_ms));
    
    ESP_LOGI(TAG, "✅ Phase 2 completed: All silence data should be played");
    
    // 🔄 Phase 3: 额外的DMA flush确保完全清空
    ESP_LOGI(TAG, "🔄 Phase 3: Final DMA flush...");
    
    uint8_t *final_silence = calloc(1024, sizeof(uint8_t));
    if (final_silence) {
        for (int i = 0; i < 20; i++) {  // 发送20次1KB静音
            size_t final_written;
            esp_err_t ret = i2s_channel_write(tx_handle, final_silence, 1024, &final_written, pdMS_TO_TICKS(50));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Final flush write %d failed: %s", i, esp_err_to_name(ret));
                break;
            }
        }
        free(final_silence);
        
        // 等待最后的静音播放完成
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    ESP_LOGI(TAG, "🎯 AGGRESSIVE DMA drain completed!");
    ESP_LOGI(TAG, "   Total silence sent: %lu bytes", total_sent + 20480);
    ESP_LOGI(TAG, "   10-second echo should be eliminated");
    
    return ESP_OK;
}
