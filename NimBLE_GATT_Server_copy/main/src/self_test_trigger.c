/**
 * @file self_test_trigger.c
 * @brief BLE触发自检模式实现
 */

#include "self_test_trigger.h"
#include "self_test.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"

static const char *TAG = "SelfTestTrigger";
static bool selftest_running = false;

// 外部任务句柄（需要在各模块中提供）
// 注意：只包含实际存在的任务句柄
extern TaskHandle_t audio_task_handle;  // 来自 audio.c
// extern TaskHandle_t g_nfc_task_handle;
// extern TaskHandle_t g_trigger_detector_task_handle;
// extern TaskHandle_t g_heart_rate_task_handle;
// extern TaskHandle_t g_action_executor_task_handle;

/**
 * @brief 停止非必要任务释放内存
 */
static void stop_non_essential_tasks(void)
{
    ESP_LOGI(TAG, "Stopping non-essential tasks to free memory...");
    
    // 停止音频任务（这是最占内存的任务）
    if (audio_task_handle != NULL) {
        ESP_LOGI(TAG, "  - Stopping audio recording...");
        // 使用audio.h提供的停止函数（如果有的话）
        extern esp_err_t audio_stop_recording(void);
        audio_stop_recording();
        ESP_LOGI(TAG, "  - Audio task stopped");
    }
    
    // TODO: 添加其他任务的停止逻辑
    // 注意：直接vTaskDelete()可能导致资源泄漏，最好使用模块提供的cleanup函数
    
    // 等待任务清理完成
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Tasks stopped. Free heap: %lu bytes", esp_get_free_heap_size());
}

/**
 * @brief 自检任务
 */
static void selftest_task(void *arg)
{
    selftest_running = true;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   🔧 Self-Test Mode (BLE Triggered)   ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // 停止非必要任务
    stop_non_essential_tasks();
    
    // 初始化自检模块
    self_test_init();
    
    // 执行自检
    self_test_report_t report = {0};
    self_test_run_all(&report);
    
    // 打印报告
    ESP_LOGI(TAG, "");
    self_test_print_report(&report);
    
    // 显示结果摘要
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Self-Test Complete!");
    ESP_LOGI(TAG, "  Pass: %d, Fail: %d, Warn: %d",
             report.passed, report.failed, report.warnings);
    ESP_LOGI(TAG, "  Device will restart in 10 seconds...");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // TODO: 通过BLE发送测试结果到手机
    
    // 等待10秒后重启
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    ESP_LOGI(TAG, "Restarting...");
    esp_restart();
}

esp_err_t trigger_selftest_via_ble(void)
{
    if (selftest_running) {
        ESP_LOGW(TAG, "Self-test already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Triggering self-test mode via BLE command...");
    
    // 先停止音频任务释放内存
    ESP_LOGI(TAG, "Stopping audio task to free memory...");
    if (audio_task_handle != NULL) {
        extern esp_err_t audio_stop_recording(void);
        audio_stop_recording();
        vTaskDelay(pdMS_TO_TICKS(500)); // 等待任务完全清理
    }
    
    // 显示释放后的内存
    uint32_t free_heap = esp_get_free_heap_size();
    ESP_LOGI(TAG, "Free heap after stopping audio: %lu bytes", free_heap);
    
    // 创建自检任务（高优先级，6KB栈 - 减小以适应内存）
    BaseType_t ret = xTaskCreate(
        selftest_task,
        "selftest",
        6144,  // 6KB栈（从8KB减少）
        NULL,
        5,  // 高优先级
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create self-test task (free heap: %lu)", free_heap);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

bool is_selftest_running(void)
{
    return selftest_running;
}
