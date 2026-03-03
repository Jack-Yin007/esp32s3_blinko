/**
 * @file motor_test.c
 * @brief 马达驱动测试程序
 * 
 * 测试项目：
 * 1. 点头马达正转/反转
 * 2. 摇头马达正转/反转
 * 3. 双马达同时运行
 * 4. 速度控制测试
 * 5. 动作执行器测试
 */

#include "drivers/hardware_interface.h"
#include "app/action_executor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MotorTest";

/**
 * @brief 测试马达基本功能
 */
void test_motor_basic(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Motor Basic Test ===");
    
    // 测试1: 点头马达正转
    ESP_LOGI(TAG, "Test 1: Nod motor forward 50%% for 2s");
    hw_motor_set(MOTOR_NOD, MOTOR_DIR_FORWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(2000));
    hw_motor_stop(MOTOR_NOD);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试2: 点头马达反转
    ESP_LOGI(TAG, "Test 2: Nod motor backward 50%% for 2s");
    hw_motor_set(MOTOR_NOD, MOTOR_DIR_BACKWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(2000));
    hw_motor_stop(MOTOR_NOD);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试3: 摇头马达左摇（正转）
    ESP_LOGI(TAG, "Test 3: Shake motor left (forward) 60%% for 1s");
    hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_FORWARD, 60);
    vTaskDelay(pdMS_TO_TICKS(1000));
    hw_motor_stop(MOTOR_SHAKE);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 测试4: 摇头马达右摇（反转）
    ESP_LOGI(TAG, "Test 4: Shake motor right (backward) 60%% for 1s");
    hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_BACKWARD, 60);
    vTaskDelay(pdMS_TO_TICKS(1000));
    hw_motor_stop(MOTOR_SHAKE);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 测试5: 同时运行两个马达
    ESP_LOGI(TAG, "Test 5: Both motors running 2s");
    hw_motor_set(MOTOR_NOD, MOTOR_DIR_FORWARD, 50);
    hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_FORWARD, 50);
    vTaskDelay(pdMS_TO_TICKS(2000));
    hw_motor_stop_all();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "=== Basic Test Complete ===");
    ESP_LOGI(TAG, "");
}

/**
 * @brief 测试速度控制
 */
void test_motor_speed(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Motor Speed Test ===");
    
    uint8_t speeds[] = {10, 30, 50, 70, 100};
    
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Speed test: %d%%", speeds[i]);
        hw_motor_set(MOTOR_NOD, MOTOR_DIR_FORWARD, speeds[i]);
        vTaskDelay(pdMS_TO_TICKS(1500));
        hw_motor_stop(MOTOR_NOD);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "=== Speed Test Complete ===");
    ESP_LOGI(TAG, "");
}

/**
 * @brief 测试动作执行器
 */
void test_action_executor(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=== Action Executor Test ===");
    
    // 测试1: 单独点头
    ESP_LOGI(TAG, "Test 1: Nod only (forward 70%%, 2s)");
    action_executor_run_motor_action(
        MOTOR_DIR_FORWARD, 70, 2000,    // 点头：正转70%，2秒
        MOTOR_DIR_STOP, 0, 0            // 摇头：停止
    );
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试2: 单独摇头
    ESP_LOGI(TAG, "Test 2: Shake only (forward 60%%, 1.5s)");
    action_executor_run_motor_action(
        MOTOR_DIR_STOP, 0, 0,           // 点头：停止
        MOTOR_DIR_FORWARD, 60, 1500     // 摇头：正转60%，1.5秒
    );
    vTaskDelay(pdMS_TO_TICKS(2500));
    
    // 测试3: 同时运行，点头时间更长
    ESP_LOGI(TAG, "Test 3: Both motors (nod 2s, shake 1.5s)");
    action_executor_run_motor_action(
        MOTOR_DIR_FORWARD, 70, 2000,    // 点头：正转70%，2秒
        MOTOR_DIR_BACKWARD, 60, 1500    // 摇头：反转60%，1.5秒
    );
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 测试4: 队列测试
    ESP_LOGI(TAG, "Test 4: Queue test (3 actions)");
    action_executor_run_motor_action(
        MOTOR_DIR_FORWARD, 50, 1000,
        MOTOR_DIR_STOP, 0, 0
    );
    action_executor_run_motor_action(
        MOTOR_DIR_STOP, 0, 0,
        MOTOR_DIR_FORWARD, 60, 800
    );
    action_executor_run_motor_action(
        MOTOR_DIR_FORWARD, 50, 1000,
        MOTOR_DIR_BACKWARD, 60, 800
    );
    
    vTaskDelay(pdMS_TO_TICKS(6000));
    
    ESP_LOGI(TAG, "=== Action Executor Test Complete ===");
    ESP_LOGI(TAG, "");
}

/**
 * @brief 马达测试任务
 */
void motor_test_task(void *pvParameters)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    Motor Driver Test Suite");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // 等待系统稳定
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 运行测试
    test_motor_basic();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    test_motor_speed();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    test_action_executor();
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    All Tests Complete!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // 删除任务
    vTaskDelete(NULL);
}

/**
 * @brief 启动马达测试
 * @note 在主程序初始化完成后调用
 */
void motor_test_start(void)
{
    xTaskCreate(
        motor_test_task,
        "motor_test",
        4096,
        NULL,
        4,
        NULL
    );
}
