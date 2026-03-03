/**
 * @file audio_test_example.c
 * @brief MAX98357A音频系统测试示例
 * 
 * 使用说明：
 * 1. 在pet_main.c中调用 hw_audio_player_init() 初始化
 * 2. 使用 hw_audio_play_effect() 播放音效
 * 3. 使用 hw_audio_set_volume() 调整音量
 * 4. 使用 hw_audio_test_all_effects() 测试所有音效
 */

#include "drivers/hardware_interface.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AudioTest";

/**
 * @brief 音频系统测试示例1：播放单个音效
 */
void test_single_effect(void)
{
    ESP_LOGI(TAG, "=== Test 1: Playing single effect ===");
    
    // 播放开心音效
    hw_audio_play_effect(0x01);  // AUDIO_HAPPY
    
    // 等待播放完成
    while (hw_audio_is_playing()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Single effect test completed");
}

/**
 * @brief 音频系统测试示例2：播放多个音效序列
 */
void test_effect_sequence(void)
{
    ESP_LOGI(TAG, "=== Test 2: Playing effect sequence ===");
    
    const uint8_t sequence[] = {
        0x01,  // AUDIO_HAPPY
        0x07,  // AUDIO_CONFIRM
        0x04,  // AUDIO_CALM
    };
    
    for (int i = 0; i < sizeof(sequence); i++) {
        ESP_LOGI(TAG, "Playing effect 0x%02X...", sequence[i]);
        hw_audio_play_effect(sequence[i]);
        
        // 等待播放完成
        while (hw_audio_is_playing()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 音效间隔
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "Effect sequence test completed");
}

/**
 * @brief 音频系统测试示例3：音量控制
 */
void test_volume_control(void)
{
    ESP_LOGI(TAG, "=== Test 3: Volume control ===");
    
    const uint8_t volumes[] = {25, 50, 75, 100};
    
    for (int i = 0; i < sizeof(volumes); i++) {
        ESP_LOGI(TAG, "Setting volume to %d%%", volumes[i]);
        hw_audio_set_volume(volumes[i]);
        
        // 播放测试音
        hw_audio_play_effect(0x04);  // AUDIO_CALM (单音)
        
        while (hw_audio_is_playing()) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // 恢复默认音量
    hw_audio_set_volume(75);
    
    ESP_LOGI(TAG, "Volume control test completed");
}

/**
 * @brief 音频系统测试示例4：测试所有音效
 */
void test_all_effects(void)
{
    ESP_LOGI(TAG, "=== Test 4: Testing all effects ===");
    
    hw_audio_test_all_effects();
    
    ESP_LOGI(TAG, "All effects test completed");
}

/**
 * @brief 完整音频系统测试流程
 */
void audio_system_full_test(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Starting Audio System Full Test");
    ESP_LOGI(TAG, "========================================");
    
    // 测试1：单个音效
    test_single_effect();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试2：音效序列
    test_effect_sequence();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试3：音量控制
    test_volume_control();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试4：所有音效
    test_all_effects();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Audio System Full Test Completed");
    ESP_LOGI(TAG, "========================================");
}

/* 
 * 在pet_main.c的app_main()中调用：
 * 
 * void app_main(void) 
 * {
 *     // ... 其他初始化 ...
 *     
 *     // 初始化音频播放器
 *     ret = hw_audio_player_init();
 *     if (ret != ESP_OK) {
 *         ESP_LOGE(TAG, "Failed to initialize audio player");
 *         return;
 *     }
 *     
 *     // 设置音量
 *     hw_audio_set_volume(75);
 *     
 *     // 可选：运行完整测试
 *     // audio_system_full_test();
 *     
 *     // 或者单独播放音效
 *     hw_audio_play_effect(0x01);  // 播放开心音效
 * }
 */
