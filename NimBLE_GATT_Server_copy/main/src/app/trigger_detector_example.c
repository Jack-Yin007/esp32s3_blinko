/*
 * 触发检测器使用示例
 * 演示如何使用trigger_detector
 */

#include "app/trigger_detector.h"
#include "app/emotion_manager.h"
#include "app/action_table.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "TriggerExample";

/**
 * @brief 触发事件回调函数
 */
static void on_trigger_detected(const trigger_event_t *event, void *user_data) {
    ESP_LOGI(TAG, "========== 触发事件 ==========");
    ESP_LOGI(TAG, "  类型: %s", trigger_detector_get_type_name(event->type));
    ESP_LOGI(TAG, "  强度: %d%%", event->intensity);
    ESP_LOGI(TAG, "  时间: %lu ms", event->timestamp);
    
    // 1. 获取当前情绪区间
    emotion_zone_t current_zone = emotion_manager_get_zone();
    ESP_LOGI(TAG, "  当前情绪: %s", emotion_manager_get_zone_name(current_zone));
    
    // 2. 随机选择一个动作组合
    uint8_t combo_count = action_table_get_combo_count(current_zone);
    uint8_t combo_id = esp_random() % combo_count;
    
    const action_combo_t *combo = action_table_get_combo(current_zone, combo_id);
    if (combo) {
        ESP_LOGI(TAG, "  动作组合: [%d] %s", combo_id, combo->description);
        
        // 3. 执行动作组合
        // TODO: 调用硬件驱动接口
        // hw_led_set_pattern(combo->led_pattern);
        // hw_vibrator_set_pattern(combo->vib_pattern);
        // hw_audio_play_effect(combo->sound_effect);
        // vTaskDelay(pdMS_TO_TICKS(combo->duration_ms));
        
        ESP_LOGI(TAG, "  LED模式: %d", combo->led_pattern);
        ESP_LOGI(TAG, "  震动模式: %d", combo->vib_pattern);
        ESP_LOGI(TAG, "  音效: %d", combo->sound_effect);
        ESP_LOGI(TAG, "  持续时间: %d ms", combo->duration_ms);
        
        // 4. TODO: 上报到手机（通过BLE）
        // b11_report_trigger_action(event->type, combo_id);
    }
    
    // 5. 增加互动计数
    emotion_manager_increment_interaction();
    
    ESP_LOGI(TAG, "================================");
}

/**
 * @brief 触发检测器使用示例
 */
void trigger_detector_example(void) {
    ESP_LOGI(TAG, "\n========== 触发检测器示例 ==========\n");
    
    // 1. 初始化触发检测器
    ESP_LOGI(TAG, "1. 初始化触发检测器...");
    trigger_detector_init();
    
    // 2. 注册回调函数
    ESP_LOGI(TAG, "2. 注册回调函数...");
    trigger_detector_register_callback(on_trigger_detected, NULL);
    
    // 3. 配置触发检测器
    ESP_LOGI(TAG, "3. 配置触发检测器...");
    trigger_detector_config_t config;
    trigger_detector_get_config(&config);
    
    // 示例：只启用触摸和接近，禁用语音
    config.touch_enabled = true;
    config.approach_enabled = true;
    config.voice_enabled = false;
    config.auto_enabled = false;  // 禁用自动触发
    config.min_interval_ms = 2000;  // 最小间隔2秒
    
    trigger_detector_update_config(&config);
    
    // 4. 启动触发检测器
    ESP_LOGI(TAG, "4. 启动触发检测器...");
    trigger_detector_start();
    
    ESP_LOGI(TAG, "触发检测器已启动，等待用户触发...");
    ESP_LOGI(TAG, "（实际硬件驱动未实现时，可以使用手动触发测试）\n");
    
    // 5. 模拟手动触发（用于测试）
    ESP_LOGI(TAG, "5秒后将模拟一次触摸触发...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    ESP_LOGI(TAG, ">>> 手动触发: 触摸");
    trigger_detector_manual_trigger(TRIGGER_TYPE_TOUCH, 85);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ESP_LOGI(TAG, ">>> 手动触发: 接近");
    trigger_detector_manual_trigger(TRIGGER_TYPE_APPROACH, 70);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 6. 查看统计信息
    trigger_statistics_t stats;
    trigger_detector_get_statistics(&stats);
    
    ESP_LOGI(TAG, "\n========== 触发统计 ==========");
    ESP_LOGI(TAG, "  触摸触发: %lu 次", stats.touch_count);
    ESP_LOGI(TAG, "  接近触发: %lu 次", stats.approach_count);
    ESP_LOGI(TAG, "  语音触发: %lu 次", stats.voice_count);
    ESP_LOGI(TAG, "  自动触发: %lu 次", stats.auto_count);
    ESP_LOGI(TAG, "  总触发: %lu 次", stats.total_count);
    ESP_LOGI(TAG, "  最后触发: %lu ms", stats.last_trigger_time);
    ESP_LOGI(TAG, "================================\n");
    
    // 7. 停止触发检测器（如果需要）
    // trigger_detector_stop();
    
    ESP_LOGI(TAG, "========== 示例结束 ==========\n");
}

/**
 * @brief 启用自动触发模式示例
 */
void trigger_detector_auto_mode_example(void) {
    ESP_LOGI(TAG, "启用自动触发模式（每15秒触发一次）");
    
    trigger_detector_config_t config;
    trigger_detector_get_config(&config);
    
    config.auto_enabled = true;
    config.auto_interval_ms = 15000;  // 15秒
    
    trigger_detector_update_config(&config);
    
    ESP_LOGI(TAG, "自动触发已启用，设备将每15秒自动执行一次动作");
}
