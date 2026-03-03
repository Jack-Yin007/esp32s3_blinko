/*
 * Pet Robot Main Application - 3层架构
 * 宠物机器人主应用程序
 * 
 * 架构层次：
 * ┌─────────────────────────────────┐
 * │  应用层 (Application Layer)      │  业务逻辑
 * ├─────────────────────────────────┤
 * │  驱动层 (Driver Layer)          │  硬件驱动 + BLE通信
 * ├─────────────────────────────────┤
 * │  系统层 (System Layer)          │  ESP-IDF + FreeRTOS
 * └─────────────────────────────────┘
 * 
 * 音频文件说明：
 * - 音频文件存储在SD卡: /audio/01.wav ~ /audio/22.wav
 * - 文件命名格式: /audio/{sound_effect:02d}.wav
 * - 音频编号由 action_table.c 中的 sound_effect 字段定义
 * - 硬件接口 hw_audio_play_effect() 负责从SD卡加载并播放WAV文件
 */

#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_task_wdt.h"
#include "esp_vfs_fat.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"

// 应用层包含
#include "app/pet_config.h"
#include "app/emotion_manager.h"
#include "app/action_table.h"
#include "app/trigger_detector.h"
#include "app/action_executor.h"

// 驱动层包含
#include "common.h"
#include "drivers/gap.h"
#include "drivers/gatt_svc.h"
#include "drivers/i2s_audio.h"
#include "drivers/led_driver.h"
#include "drivers/heart_rate_sensor.h"
#include "drivers/hardware_interface.h"
#include "drivers/aw9535.h"
#include "drivers/aw9535_pinmap.h"  // AW9535引脚映射配置
#include "drivers/sd_card.h"
#include "drivers/flash_storage.h"
#include "drivers/voice_recognition.h"
#include "drivers/charger.h"
#include "heart_rate.h"
// #include "led.h"  // 旧版LED，已被led_enhanced.c取代
#include "audio.h"
#include "drivers/indatachange.h"
#include "log_collector.h"
#include "fs_service.h"
#include "version.h"
#include "self_test.h"
/*=============================================================================
 * 全局定义 (Global Definitions)
 *=============================================================================*/

// TAG已在common.h中定义为"Blinko"

// 应用版本信息
#define PET_ROBOT_VERSION_MAJOR     2
#define PET_ROBOT_VERSION_MINOR     0
#define PET_ROBOT_VERSION_PATCH     0
#define PET_ROBOT_ARCHITECTURE      "3-Layer"

// 初始化阶段
typedef enum {
    INIT_STAGE_START = 0,
    INIT_STAGE_NVS,
    INIT_STAGE_CONFIG,
    INIT_STAGE_DRIVERS,
    INIT_STAGE_COMMUNICATION,
    INIT_STAGE_APPLICATION,
    INIT_STAGE_COMPLETE
} init_stage_t;

/*=============================================================================
 * 私有函数声明 (Private Function Declarations)
 *=============================================================================*/

static bool check_selftest_trigger(void);
static void run_selftest_mode(void) __attribute__((noreturn));
static esp_err_t pet_robot_init_nvs(void);
static esp_err_t pet_robot_init_drivers(void);
static esp_err_t pet_robot_init_communication(void);
static esp_err_t pet_robot_init_application(void);
static esp_err_t pet_robot_start_services(void);
static void pet_robot_print_system_info(void);
static void pet_robot_system_monitor_task(void* pvParameters);
static void nimble_host_reset_cb(int reason);
static void nimble_host_sync_cb(void);
static void nimble_host_task(void* param);
static void heart_rate_task(void* param);
static void on_trigger_event(const trigger_event_t *event, void *user_data);

/*=============================================================================
 * 外部库函数声明
 *=============================================================================*/
void ble_store_config_init(void);

/*=============================================================================
 * 触发事件回调实现
 *=============================================================================*/

/**
 * @brief 获取LED模式名称及引脚信息
 */
__attribute__((unused)) static const char* get_led_pattern_name(led_pattern_id_t pattern) {
    switch (pattern) {
        case LED_PATTERN_OFF: return "关闭";
        case LED_PATTERN_SOLID_RED: return "纯红色 [R=P1.4(AW9535)]";
        case LED_PATTERN_SOLID_GREEN: return "纯绿色 [G=P1.5(AW9535)]";
        case LED_PATTERN_SOLID_BLUE: return "纯蓝色 [B=P1.6(AW9535)]";
        case LED_PATTERN_SOLID_YELLOW: return "纯黄色 [R+G=P1.4+P1.5]";
        case LED_PATTERN_BLINK_SLOW: return "慢速闪烁 [RGB=P1.4/5/6]";
        case LED_PATTERN_BLINK_FAST: return "快速闪烁 [RGB=P1.4/5/6]";
        case LED_PATTERN_WAVE: return "波浪效果 [RGB=P1.4/5/6]";
        default: return "未知";
    }
}

/**
 * @brief 获取震动模式名称及引脚信息
 */
__attribute__((unused)) static const char* get_vibration_pattern_name(vibration_pattern_id_t pattern) {
    switch (pattern) {
        case VIB_PATTERN_OFF: return "关闭";
        case VIB_PATTERN_SHORT_PULSE: return "短脉冲(100ms) [GPIO9]";
        case VIB_PATTERN_LONG_PULSE: return "长脉冲(500ms) [GPIO9]";
        case VIB_PATTERN_DOUBLE_PULSE: return "双震 [GPIO9]";
        case VIB_PATTERN_TRIPLE_PULSE: return "三连震 [GPIO9]";
        case VIB_PATTERN_CONTINUOUS_WEAK: return "持续弱震 [GPIO9]";
        case VIB_PATTERN_CONTINUOUS_STRONG: return "持续强震 [GPIO9]";
        case VIB_PATTERN_WAVE: return "波浪震动 [GPIO9]";
        case VIB_PATTERN_HEARTBEAT: return "心跳震动 [GPIO9]";
        default: return "未知";
    }
}

/**
 * @brief 触发事件回调函数
 * 
 * 当触发检测器检测到触发事件时，此函数会被调用
 * 
 * @param event 触发事件信息
 * @param user_data 用户数据（未使用）
 */
static void on_trigger_event(const trigger_event_t *event, void *user_data)
{
    (void)user_data;  // 未使用
    
    // 1. 获取当前情绪区间
    emotion_zone_t current_zone = emotion_manager_get_zone();
    
    // 2. 根据事件类型确定触发条件
    trigger_condition_t condition;
    switch (event->type) {
        case TRIGGER_TYPE_TOUCH:
            condition = TRIGGER_CONDITION_TOUCH;
            break;
        case TRIGGER_TYPE_APPROACH:
            condition = TRIGGER_CONDITION_APPROACH;
            break;
        case TRIGGER_TYPE_VOICE:
            condition = TRIGGER_CONDITION_VOICE;
            break;
        case TRIGGER_TYPE_AUTO:
            condition = TRIGGER_CONDITION_AUTO;
            break;
        default:
            ESP_LOGW(TAG, "未知触发类型");
            return;
    }
    
    // 3. 根据情绪区间和触发条件随机选择动作组合
    const action_combo_t *combo = action_table_get_random_combo(current_zone, condition);
    if (combo == NULL) {
        ESP_LOGW(TAG, "未找到动作组合");
        return;
    }
    
    // 精简日志：仅显示触发类型和动作组合
    ESP_LOGI(TAG, "🎯 %s触发 → 组合#%d: %s [LED=%d 震动=%d 音效=%d]", 
             trigger_detector_get_type_name(event->type),
             combo->combo_id, 
             combo->description,
             combo->led_pattern,
             combo->vib_pattern,
             combo->sound_effect);
    
    // 4. 执行动作组合
    
    // 4.1 设置LED灯光
    hw_led_set_pattern(combo->led_pattern);
    
    // 4.2 设置震动模式
    hw_vibrator_set_pattern(combo->vib_pattern);
    
    // 4.3 播放音频（从SD卡加载WAV文件）
    if (combo->sound_effect > 0) {
        esp_err_t audio_ret = hw_audio_play_wav(combo->sound_effect);
        if (audio_ret != ESP_OK) {
            ESP_LOGW(TAG, "音频播放失败: WAV %d", combo->sound_effect);
        }
    }
    
    // 4.4 执行马达动作（使用动作执行器）
    if ((combo->nod_direction >= 0 && combo->nod_duration >= 0) || 
        (combo->shake_direction >= 0 && combo->shake_duration >= 0)) { 
        
        // action_executor_run_motor_action(
        //     (motor_direction_t)combo->nod_direction,
        //     combo->nod_speed,
        //     combo->nod_duration,
        //     (motor_direction_t)combo->shake_direction,
        //     combo->shake_speed,
        //     combo->shake_duration
        // );

        load_action_to_motor(
            (motor_direction_t)combo->nod_direction,
            combo->nod_speed,
            combo->nod_duration,
            combo->nod_execute_num,
            combo->nod_interval_ms,
            combo->nod_priority,
            (motor_direction_t)combo->shake_direction,
            combo->shake_speed,
            combo->shake_duration,
            combo->shake_execute_num,
            combo->shake_interval_ms,
            combo->shake_priority,
            combo->ear_left_direction,
            combo->ear_left_priority,
            combo->ear_left_repeat_num,
            combo->ear_right_direction,
            combo->ear_right_priority,
            combo->ear_right_repeat_num
        );
    }
    
    // 5. 通过B11协议上报触发事件到手机
    uint8_t b11_trigger_type = 0;
    switch (event->type) {
        case TRIGGER_TYPE_TOUCH:
            b11_trigger_type = 0x01;  // B11_TRIGGER_TOUCH
            break;
        case TRIGGER_TYPE_APPROACH:
            b11_trigger_type = 0x02;  // B11_TRIGGER_APPROACH
            break;
        case TRIGGER_TYPE_VOICE:
            b11_trigger_type = 0x03;  // B11_TRIGGER_VOICE
            break;
        default:
            b11_trigger_type = 0x00;  // Unknown
            break;
    }
    
    if (b11_trigger_type > 0) {
        b11_report_trigger_action(b11_trigger_type, combo->combo_id);
    }
    
    // 6. 增加互动计数
    emotion_manager_increment_interaction();
    
    // 7. 保存到NVS（可选，避免频繁写入）
    // emotion_manager_save_to_nvs();
}

/*=============================================================================
 * 自检模式函数 (Self-Test Mode Functions)
 *=============================================================================*/

/**
 * @brief 检测是否触发自检模式
 * 
 * 检测方法：同时按下额头触摸(GPIO4)和背部触摸(GPIO5)传感器
 * 
 * @return true=进入自检模式, false=正常启动
 */
static bool check_selftest_trigger(void)
{
    // 配置触摸传感器GPIO为输入（带上拉）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4) | (1ULL << GPIO_NUM_5),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // 简单直接检测：只要任意一次检测到两个GPIO同时为低电平就触发
    // 这是最容易触发的方式
    vTaskDelay(pdMS_TO_TICKS(100));
    
    printf("\n========================================\n");
    printf("[SELFTEST] Checking for self-test trigger...\n");
    printf("========================================\n");
    
    // 连续检测2秒，只要任意时刻检测到就触发
    for (int i = 0; i < 20; i++) {
        int gpio4 = gpio_get_level(GPIO_NUM_4);
        int gpio5 = gpio_get_level(GPIO_NUM_5);
        
        printf("[SELFTEST] Check %2d/20: GPIO4=%d, GPIO5=%d", i+1, gpio4, gpio5);
        
        if (gpio4 == 0 && gpio5 == 0) {
            printf(" -> BOTH LOW! Triggering self-test!\n");
            printf("========================================\n\n");
            return true;
        }
        
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    printf("[SELFTEST] No trigger detected, entering normal mode\n");
    printf("========================================\n\n");
    return false;
}

/**
 * @brief 运行自检模式
 * 
 * 此函数永不返回，自检完成后会自动重启设备
 */
static void run_selftest_mode(void)
{
    // 等待系统完全初始化（包括日志系统的mutex/semaphore）
    // 增加到1秒确保FreeRTOS调度器和所有系统服务完全就绪
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Entering Self-Test Mode");
    ESP_LOGI(TAG, "  Normal functions are DISABLED");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // 1. 初始化NVS（必需）
    ESP_LOGI(TAG, "[1/4] Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "      ✓ NVS initialized");
    
    // 2. 初始化I2C和GPIO扩展器（硬件测试需要）
    ESP_LOGI(TAG, "[2/4] Initializing AW9535 GPIO expander...");
    // 使用默认配置
    aw9535_config_t aw_config = {
        .i2c_cfg = {
            .i2c_port = I2C_NUM_0,
            .dev_addr = AW9535_I2C_ADDR_DEFAULT,
            .sda_io = GPIO_NUM_2,
            .scl_io = GPIO_NUM_1,
            .freq_hz = 400000
        },
        .int_io = GPIO_NUM_45
    };
    ret = aw9535_init(&aw_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "      ✓ AW9535 initialized");
    } else {
        ESP_LOGW(TAG, "      ⚠ AW9535 init failed, some tests may fail");
    }
    
    // 3. 初始化LED（用于状态指示）
    ESP_LOGI(TAG, "[3/4] Initializing LED driver...");
    ret = hw_led_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "      ✓ LED driver initialized");
        hw_led_set_pattern(LED_PATTERN_BLINK_FAST);  // 快速闪烁表示自检模式
    }
    
    // 4. 运行自检
    ESP_LOGI(TAG, "[4/4] Starting hardware self-test...");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Free heap before test: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "");
    
    // 初始化自检模块
    self_test_init();
    
    // 运行完整自检
    self_test_report_t report;
    self_test_run_all(&report);
    
    // 5. 打印详细报告
    ESP_LOGI(TAG, "");
    self_test_print_report(&report);
    
    // 6. 显示测试摘要
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║      📊 Self-Test Complete Report      ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║  Total:     %2d devices                ║", report.total_devices);
    ESP_LOGI(TAG, "║  ✅ Passed:    %2d devices               ║", report.passed);
    ESP_LOGI(TAG, "║  ❌ Failed:    %2d devices               ║", report.failed);
    ESP_LOGI(TAG, "║  ⚠️  Warnings: %2d devices               ║", report.warnings);
    ESP_LOGI(TAG, "║  ⏭️  Skipped:  %2d devices               ║", report.skipped);
    ESP_LOGI(TAG, "║  ⏱️  Time:     %lu ms                   ║", report.total_time_ms);
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // 7. 根据结果设置LED颜色
    if (report.failed > 0) {
        ESP_LOGI(TAG, "Test result: FAILED ❌");
        hw_led_set_pattern(LED_PATTERN_SOLID_RED);  // 失败：红色
    } else if (report.warnings > 0) {
        ESP_LOGI(TAG, "Test result: WARNING ⚠️");
        hw_led_set_pattern(LED_PATTERN_SOLID_YELLOW);  // 警告：黄色
    } else {
        ESP_LOGI(TAG, "Test result: ALL PASS ✅");
        hw_led_set_pattern(LED_PATTERN_SOLID_BLUE);  // 全部通过：蓝色
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Free heap after test: %lu bytes", esp_get_free_heap_size());
    
    // 8. 等待5秒后重启
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Self-test mode will exit in 5 seconds...");
    ESP_LOGI(TAG, "Device will restart to normal mode.");
    ESP_LOGI(TAG, "");
    
    for (int i = 5; i > 0; i--) {
        ESP_LOGI(TAG, "Restarting in %d...", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    // 9. 重启设备
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
    
    // 永不到达
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*=============================================================================
 * 主函数 (Main Function)
 *=============================================================================*/

void app_main(void)
{
    // ============================================
    // 启动时检测：是否进入自检模式
    // ============================================
    // 临时禁用：GPIO4/GPIO5默认为0，导致每次都触发自检
    // TODO: 需要先初始化AW9535为TTP223供电后再检测
    #if 0
    if (check_selftest_trigger()) {
        // 进入自检模式（永不返回）
        run_selftest_mode();
        // 永不到达
    }
    #endif

    // ============================================
    // 正常启动流程
    // ============================================
    esp_err_t ret = ESP_OK;
    init_stage_t current_stage = INIT_STAGE_START;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Pet Robot System - Normal Mode");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Version: %d.%d.%d", 
             PET_ROBOT_VERSION_MAJOR, 
             PET_ROBOT_VERSION_MINOR, 
             PET_ROBOT_VERSION_PATCH);
    ESP_LOGI(TAG, "  Architecture: %s", PET_ROBOT_ARCHITECTURE);
    ESP_LOGI(TAG, "========================================");

    // 打印系统信息
    pet_robot_print_system_info();

    // 🔇 NFC日志设置（允许交换通知，但抑制连接检测刷屏）
    esp_log_level_set("PN532_COMM", ESP_LOG_INFO);  // 允许NFC交换日志
    esp_log_level_set("NFC_UART", ESP_LOG_WARN);     // NFC UART只显示警告

    // 阶段1: 系统层 - NVS初始化
    current_stage = INIT_STAGE_NVS;
    ESP_LOGI(TAG, "[系统层] Stage %d: Initializing NVS Flash", current_stage);
    ret = pet_robot_init_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        goto error_handler;
    }

    // 初始化BLE日志收集器 (16KB RAM循环缓冲区)
    ESP_LOGI(TAG, "[系统层] Initializing BLE Log Collector (16KB RAM buffer)");
    log_collector_init();
    log_collector_install_hook();  // 拦截所有ESP_LOG输出
    ESP_LOGI(TAG, "[系统层] BLE Log Collector ready - logs available via BLE service 0x6E400010");

    // 阶段2: 应用层 - 配置系统初始化
    current_stage = INIT_STAGE_CONFIG;
    ESP_LOGI(TAG, "[应用层] Stage %d: Initializing Configuration", current_stage);
    ret = pet_config_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Configuration system not available, using defaults");
    }

    // 阶段3: 驱动层 - 设备驱动初始化
    current_stage = INIT_STAGE_DRIVERS;
    ESP_LOGI(TAG, "[驱动层] Stage %d: Initializing Device Drivers", current_stage);
    ret = pet_robot_init_drivers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize drivers: %s", esp_err_to_name(ret));
        goto error_handler;
    }

    // 🔧 诊断测试：在BLE启动前测试INMP441麦克风
    ESP_LOGI(TAG, "[诊断] Testing INMP441 microphone before BLE...");
    vTaskDelay(pdMS_TO_TICKS(1000));  // 等待音频系统稳定
    i2s_audio_diag_test_inmp441();
    vTaskDelay(pdMS_TO_TICKS(500));   // 等待测试完成
    ESP_LOGI(TAG, "[诊断] Microphone test completed, proceeding to BLE init");

    // 阶段4: 驱动层 - BLE通信初始化
    current_stage = INIT_STAGE_COMMUNICATION;
    ESP_LOGI(TAG, "[驱动层] Stage %d: Initializing BLE Communication", current_stage);
    ret = pet_robot_init_communication();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize communication: %s", esp_err_to_name(ret));
        goto error_handler;
    }

    // 阶段5: 应用层 - 应用逻辑初始化
    current_stage = INIT_STAGE_APPLICATION;
    ESP_LOGI(TAG, "[应用层] Stage %d: Initializing Application Logic", current_stage);
    ret = pet_robot_init_application();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize application: %s", esp_err_to_name(ret));
        goto error_handler;
    }

    // 阶段6: 启动所有服务
    current_stage = INIT_STAGE_COMPLETE;
    ESP_LOGI(TAG, "Stage %d: Starting All Services", current_stage);
    ret = pet_robot_start_services();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start services: %s", esp_err_to_name(ret));
        goto error_handler;
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Pet Robot System Started Successfully");
    ESP_LOGI(TAG, "========================================");

    // 创建系统监控任务
    xTaskCreate(pet_robot_system_monitor_task, 
                "system_monitor", 
                4096,  // 减少栈大小以节省内存 (从6144减少)
                NULL, 
                1, 
                NULL);

    return;

error_handler:
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "  Pet Robot System Failed to Start");
    ESP_LOGE(TAG, "========================================");
    ESP_LOGE(TAG, "Failed at stage %d: %s", current_stage, esp_err_to_name(ret));
    
    // 错误恢复：等待5秒后重启
    ESP_LOGE(TAG, "Restarting in 5 seconds...");
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}

/*=============================================================================
 * 初始化函数实现 (Initialization Functions)
 *=============================================================================*/

static esp_err_t pet_robot_init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ NVS Flash initialized successfully");
    }
    
    return ret;
}

static esp_err_t pet_robot_init_drivers(void)
{
    esp_err_t ret = ESP_OK;

    // 初始化AW9535 GPIO扩展器（必须最先初始化，因为LED和Audio都依赖它）
    aw9535_config_t aw9535_cfg = {
        .i2c_cfg = {
            .i2c_port = I2C_NUM_0,
            .dev_addr = 0x20,
            .sda_io = GPIO_NUM_2,
            .scl_io = GPIO_NUM_1,
            .freq_hz = 400000
        },
        .int_io = GPIO_NUM_45
    };
    ret = aw9535_init(&aw9535_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ AW9535 GPIO expander initialized (I2C: SDA=GPIO2, SCL=GPIO1, Addr=0x20)");
        
        // 初始化AW9535引脚映射配置
        ret = aw9535_pinmap_init();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✓ AW9535 pin mappings configured");
        } else {
            ESP_LOGW(TAG, "⚠ AW9535 pin mapping init failed");
        }
    } else {
        ESP_LOGE(TAG, "✗ AW9535 GPIO expander init failed - LED and Audio will not work!");
        // 不返回错误，继续初始化其他设备
    }

	struct charger_config_t charger_config = {
		.charge_voltage = CW6116_CHARGE_VOLTAGE_4V35,
		.charge_current = CW6116_CHARGE_CURRENT_2000mA,
		.over_current_protection = CW6116_OC_PROTECTION_2400mA,
	};
	ret = aw9535_set_mode(AW9535_PIN_P0_5, AW9535_MODE_OUTPUT);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set P0.5 to output mode");
	}

	ret = aw9535_set_level(AW9535_PIN_P0_5, AW9535_LEVEL_LOW);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to set P0.5 to low");
	}
	ret = charger_init(&charger_config);
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "✓ Charger initialized successfully");
	} else {
		ESP_LOGE(TAG, "✗ Charger initialization failed");
	}

    // 初始化LED驱动（使用AW9535扩展器：P1.4/P1.5/P1.6）
    ret = hw_led_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ LED driver initialized (AW9535: R=P1.4, G=P1.5, B=P1.6)");
    } else {
        ESP_LOGW(TAG, "LED driver init failed");
    }

	// 初始化Flash FAT分区（音频和NFC文件）
	ret = flash_fat_init();
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "✓ Flash FAT partition initialized (/flash/audio, /flash/nfc)");
		
		// 列出Flash audio目录内容
		ret = sd_card_list_dir("/flash/audio");
		if (ret != ESP_OK) {
			ESP_LOGW(TAG, "Failed to list /flash/audio directory");
		}
	} else {
		ESP_LOGE(TAG, "Flash FAT init failed - audio playback unavailable");
	}

	ret = sd_card_init();
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "✓ SDCARD initialized");
		
		// 检查Flash分区是否有足够的WAV文件（至少20个，排除损坏文件）
		if (!flash_storage_has_wav_files()) {
			ESP_LOGI(TAG, "Flash audio directory empty or incomplete, copying from SD card...");
			ret = flash_storage_copy_wav_files();
			if (ret == ESP_OK) {
				ESP_LOGI(TAG, "✓ WAV files copied to Flash");
				// 复制后重新列出目录验证
				sd_card_list_dir("/flash/audio");
			} else {
				ESP_LOGW(TAG, "WAV file copy failed - will use SD card fallback");
			}
		} else {
			ESP_LOGI(TAG, "✓ Flash already contains WAV files, skipping copy");
		}
	} else {
		ESP_LOGW(TAG, "SDCARD driver init failed");
	}

	ret = nfc_init();
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "✓ NFC initialized");
	} else {
		ESP_LOGW(TAG, "NFC driver init failed");
	}

	// 初始化语音识别驱动 (GPIO44轮询模式)
	ret = hw_aduio_sensor_init();
	if (ret == ESP_OK) {
		ESP_LOGI(TAG, "✓ Voice sensor initialized (GPIO44 polling mode, active low)");
	} else {
		ESP_LOGW(TAG, "Voice sensor init failed");
	}

    // 初始化I2S全双工音频系统（MAX98357A DAC + INMP441 MIC共享I2S0）
    ret = i2s_audio_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ I2S Full-Duplex Audio initialized (8kHz, TX=Stereo, RX=Mono)");
        
        // 启用RX通道以便后续诊断测试
        ret = i2s_audio_enable_rx();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✓ I2S RX channel enabled for microphone testing");
        } else {
            ESP_LOGE(TAG, "❌ Failed to enable I2S RX: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ I2S audio init failed: %s", esp_err_to_name(ret));
    }

    // 初始化音频播放器（MAX98357A I2S DAC，增益控制使用AW9535）
    ret = hw_audio_player_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Audio player initialized (MAX98357A: I2S GPIO40/39/41, Dual-GPIO volume control)");
        // 设置默认音量为最大(100%)
        ESP_LOGI(TAG, "Setting default volume to 100% (maximum)...");
        hw_audio_set_volume(100);
    } else {
        ESP_LOGW(TAG, "Audio player init failed");
    }

    // 初始化触摸传感器 (TTP223双通道)
    ret = hw_touch_sensor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Touch sensor initialized (TTP223)");
    } else {
        ESP_LOGW(TAG, "Touch sensor init failed");
    }

    // 初始化PIR人体感应传感器
    ret = hw_pir_sensor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ PIR motion sensor initialized (GPIO6)");
    } else {
        ESP_LOGW(TAG, "PIR sensor init failed");
    }

    // 初始化心率传感器驱动（新架构，模拟模式）
    ret = heart_rate_driver_init(true);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Heart rate sensor initialized (simulation mode)");
        heart_rate_driver_start_measurement();
    } else {
        ESP_LOGW(TAG, "Heart rate driver init failed, using legacy mock");
    }

    // 初始化震动马达驱动
    ret = hw_vibrator_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Vibrator motor initialized (GPIO9)");
    } else {
        ESP_LOGW(TAG, "Vibrator init failed");
    }

    // 初始化马达驱动（点头+摇头）
   // ret = hw_motor_init();
    ret = hw_servo_motor_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✓ Motors initialized (Nod: GPIO46/3, Shake: GPIO21/14)");
    } else {
        ESP_LOGW(TAG, "Motor driver init failed");
    }

    ESP_LOGI(TAG, "✓ Device drivers initialized successfully");
    return ESP_OK;
}

static esp_err_t pet_robot_init_communication(void)
{
    esp_err_t ret = ESP_OK;
    int rc;

    // 初始化NimBLE协议栈
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize nimble stack: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "✓ NimBLE stack initialized");

    // 初始化GAP服务
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize GAP service: %d", rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ BLE GAP service initialized");

    // 初始化GATT服务器
    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initialize GATT server: %d", rc);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✓ BLE GATT server initialized");

    // 初始化BLE文件系统服务
    ESP_LOGI(TAG, "[系统层] Initializing BLE File System Service");
    fs_service_init();
    ESP_LOGI(TAG, "[系统层] BLE FS Service ready - file browsing via BLE service 0x6E400020");

    // 初始化音频GATT服务
    rc = audio_gatt_svc_init();
    if (rc != 0) {
        ESP_LOGW(TAG, "Audio GATT service init failed: %d (optional)", rc);
    } else {
        ESP_LOGI(TAG, "✓ Audio GATT service initialized");
    }

    // 初始化音频硬件
    ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Audio hardware init failed: %s (optional)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✓ Audio hardware initialized");
    }

    ESP_LOGI(TAG, "✓ BLE Communication layer initialized");
    return ESP_OK;
}

static esp_err_t pet_robot_init_application(void)
{
    esp_err_t ret;
    
    // 初始化情绪系统（在gatt_svc_init中已初始化，这里加载状态）
    ESP_LOGI(TAG, "加载情绪系统状态...");
    ret = emotion_manager_load_from_nvs();
    if (ret == ESP_OK) {
        emotion_zone_t zone = emotion_manager_get_zone();
        ESP_LOGI(TAG, "✓ 情绪状态已加载: %s", emotion_manager_get_zone_name(zone));
    }
    
    // 初始化触发检测器
    ESP_LOGI(TAG, "初始化触发检测器...");
    ret = trigger_detector_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "触发检测器初始化失败");
        return ret;
    }
    
    // 注册触发事件回调
    ret = trigger_detector_register_callback(on_trigger_event, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "触发回调注册失败");
        return ret;
    }
    
    // 配置触发检测器
    trigger_detector_config_t config;
    trigger_detector_get_config(&config);
    config.touch_enabled = true;         // 启用触摸检测
    config.approach_enabled = true;      // 启用接近检测
    config.voice_enabled = true;         // 启用语音检测
    config.auto_enabled = false;         // 禁用自动触发
    config.min_interval_ms = 1000;       // 最小间隔1秒
    config.debounce_ms = 200;            // 去抖动200ms
    trigger_detector_update_config(&config);
    
    // 启动触发检测器
    ret = trigger_detector_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "触发检测器启动失败");
        return ret;
    }
    
    // 初始化动作执行器
    ESP_LOGI(TAG, "初始化动作执行器...");
    ret = action_executor_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "动作执行器初始化失败");
        return ret;
    }
    
    ESP_LOGI(TAG, "✓ Application layer initialized");
    ESP_LOGI(TAG, "✓ 触发检测器已启动");
    ESP_LOGI(TAG, "✓ 动作执行器已启动");
    return ESP_OK;
}

static esp_err_t pet_robot_start_services(void)
{
    esp_err_t ret = ESP_OK;

    // 配置BLE主机回调
    ble_hs_cfg.reset_cb = nimble_host_reset_cb;
    ble_hs_cfg.sync_cb = nimble_host_sync_cb;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // 初始化BLE存储配置
    ble_store_config_init();
    ESP_LOGI(TAG, "✓ BLE storage config initialized");

    // 启动NimBLE主机任务（优先级3，低于NFC的5和麦克风的4）
    xTaskCreate(nimble_host_task, "nimble_host", 4*1024, NULL, 3, NULL);
    ESP_LOGI(TAG, "✓ NimBLE host task started (priority 3)");

    // 启动心率任务（优先级2，最低）
    xTaskCreate(heart_rate_task, "heart_rate", 5*1024, NULL, 2, NULL);
    ESP_LOGI(TAG, "✓ Heart rate task started (priority 2)");

    ESP_LOGI(TAG, "✓ All services started successfully");
    return ret;
}

/*=============================================================================
 * 任务函数实现 (Task Functions)
 *=============================================================================*/

static void nimble_host_reset_cb(int reason)
{
    ESP_LOGI(TAG, "NimBLE stack reset, reason: %d", reason);
}

static void nimble_host_sync_cb(void)
{
    ESP_LOGI(TAG, "NimBLE stack synced, starting advertising");
    adv_init();
}

static void nimble_host_task(void* param)
{
    ESP_LOGI(TAG, "NimBLE host task running (priority 3, yields to NFC)");
    
    // nimble_port_run()是阻塞式的，虽然内部使用队列等待
    // 但在处理事件时可能长时间占用CPU
    // 
    // 为确保NFC任务(优先级5)能及时运行，不直接调用nimble_port_run()
    // 而是手动处理事件循环 + 主动让出CPU
    
    while (1) {
        // 处理NimBLE事件（非阻塞，处理所有待处理事件）
        // 注意：nimble_port_run()内部是死循环，不能直接用
        // 需要使用底层API
        
        // 方案1：使用ble_npl_eventq_get()非阻塞获取事件
        // 但这需要访问内部API，不太稳定
        
        // 方案2：使用nimble_port_run()但在外部添加延迟
        // 但nimble_port_run()本身是死循环，无法返回
        
        // 方案3（最佳）：使用vTaskDelay在nimble_port_run前后
        // 但由于nimble_port_run不会返回，需要修改
        
        // 实际上nimble_port_run()内部已经使用了FreeRTOS队列等待
        // 理论上优先级3的任务会自动让出CPU给优先级5的NFC
        // 
        // 如果仍有问题，说明nimble在处理事件时占用时间过长
        // 解决方案：在BLE事件处理后主动yield
        
        // 简单方案：每隔一段时间主动让出CPU
        // 但nimble_port_run()是阻塞的，无法插入delay
        
        // 最终方案：不修改nimble_port_run，依赖优先级抢占
        // NFC优先级5 > BLE优先级3，应该能自动抢占
        nimble_port_run();
        
        // 注：如果上述方案不work，需要修改为事件轮询模式
        break;  // nimble_port_run()永不返回，这行不会执行
    }
    
    vTaskDelete(NULL);
}

uint32_t run_count = 0;


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/uart_select.h"
#include "driver/gpio.h"


/* 引脚和串口定义 */
#define USART_UX            UART_NUM_0
#define USART_TX_GPIO_PIN   GPIO_NUM_43
#define USART_RX_GPIO_PIN   GPIO_NUM_44

/* 串口接收相关定义 */
#define RX_BUF_SIZE         1024    /* 环形缓冲区大小 */

QueueHandle_t uart_queue;

void usart_init(uint32_t baudrate)
{
    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    /* 配置uart参数 */
    ESP_ERROR_CHECK(uart_param_config(USART_UX, &uart_config));

    /* 配置uart引脚 */
    ESP_ERROR_CHECK(uart_set_pin(USART_UX, USART_TX_GPIO_PIN, USART_RX_GPIO_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    /* 安装串口驱动 */
    uart_driver_install(USART_UX, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 10, &uart_queue, 0);
}

uint32_t test = 0;
static void heart_rate_task(void* param)
{
    ESP_LOGI(TAG, "Heart rate task running");
    usart_init(115200); 
    while (1) {
        // 使用新架构的驱动更新心率
        heart_rate_driver_update_simulation();
        
        // B11协议: 可以在这里上报传感器数据或触发动作
        // 例如: b11_report_sensor_data(B11_SENSOR_VOLUME, get_volume_level());

        // B11协议: 可以在这里上报传感器数据或触发动作
        // 例如: b11_report_sensor_data(B11_SENSOR_VOLUME, get_volume_level());
        unsigned char data[RX_BUF_SIZE] = {0};
        esp_err_t ret;
        uint8_t len = 0;
        uint16_t times = 0;

        uart_get_buffered_data_len(USART_UX, (size_t*) &len);                           /* 获取环形缓冲区数据长度 */
        static uint32_t cnt = 0;
        cnt++;
        ESP_LOGI(TAG, "time %d queue_cnt %d %d\r\n",cnt, action_executor_get_queue_count(), test); 
        if (len > 0)                                                                    /* 判断数据长度 */
        {
            memset(data, 0, RX_BUF_SIZE);                                               /* 对缓冲区清零 */

            uart_read_bytes(USART_UX, data, len, 100);   
            ESP_LOGI(TAG, "您发送的消息为:%d \r\n", data[0]);                               /* 读数据 */
            //uart_write_bytes(USART_UX, (const char*)data, strlen((const char*)data));   /* 写数据 */
            action_executor_execute_combo( data[0]);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*=============================================================================
 * 辅助函数实现 (Helper Functions)
 *=============================================================================*/

static void pet_robot_print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "System Information:");
    ESP_LOGI(TAG, "  Chip: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "  Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "  Revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "  Features: %s Flash", 
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Embedded" : "External");
    ESP_LOGI(TAG, "  Free Heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "  Min Free Heap: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());
}

static void pet_robot_system_monitor_task(void* pvParameters)
{
    ESP_LOGI(TAG, "System monitor task started");

    const uint32_t monitor_interval_ms = 60000; // 60秒监控一次 (从30秒延长)
    uint32_t last_free_heap = esp_get_free_heap_size();
    uint32_t print_cycle = 0;

    // 为任务统计分配缓冲区
    char *task_list_buffer = NULL;
    char *runtime_stats_buffer = NULL;
    
    while (1) {
        uint32_t current_free_heap = esp_get_free_heap_size();
        uint32_t min_free_heap = esp_get_minimum_free_heap_size();

        // 检查内存泄漏
        if (current_free_heap < last_free_heap * 0.9) { // 内存减少超过10%
            ESP_LOGW(TAG, "Potential memory leak: %lu -> %lu bytes", 
                     (unsigned long)last_free_heap, (unsigned long)current_free_heap);
        }

        // 检查严重内存不足
        if (current_free_heap < 10240) { // 小于10KB
            ESP_LOGE(TAG, "Critical low memory: %lu bytes", (unsigned long)current_free_heap);
        }

        // 打印系统状态（包含版本号）
        ESP_LOGI(TAG, "System Status [v%s] - Free: %lu, Min: %lu bytes", 
                 get_firmware_version(),
                 (unsigned long)current_free_heap, (unsigned long)min_free_heap);

        // 每2分钟（4个周期）打印一次详细的任务统计
        print_cycle++;
        if (print_cycle >= 4) {
            print_cycle = 0;
            
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "      Task Statistics Report");
            ESP_LOGI(TAG, "========================================");
            
            // 打印任务列表（状态、优先级、栈使用）
            if (task_list_buffer == NULL) {
                task_list_buffer = (char *)malloc(2048);
                if (task_list_buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate task_list_buffer");
                }
            }
            
            if (task_list_buffer != NULL) {
                vTaskList(task_list_buffer);
                
                // 转换状态字母为全称
                char *enhanced_buffer = (char *)malloc(4096);
                if (enhanced_buffer != NULL) {
                    char *src = task_list_buffer;
                    char *dst = enhanced_buffer;
                    int line_pos = 0;
                    
                    while (*src && (dst - enhanced_buffer < 4000)) {
                        if (*src == '\t' || *src == ' ') {
                            // 复制空白字符
                            *dst++ = *src++;
                            line_pos++;
                        } else if (*src == '\n' || *src == '\r') {
                            // 换行重置位置
                            *dst++ = *src++;
                            line_pos = 0;
                        } else if (line_pos > 15 && line_pos < 25) {
                            // 任务状态位置（大约在第15-25个字符）
                            if (*src == 'R' && (*(src+1) == '\t' || *(src+1) == ' ')) {
                                const char *state = "Running  ";
                                while (*state) *dst++ = *state++;
                                src++;
                            } else if (*src == 'B' && (*(src+1) == '\t' || *(src+1) == ' ')) {
                                const char *state = "Blocked  ";
                                while (*state) *dst++ = *state++;
                                src++;
                            } else if (*src == 'S' && (*(src+1) == '\t' || *(src+1) == ' ')) {
                                const char *state = "Suspended";
                                while (*state) *dst++ = *state++;
                                src++;
                            } else if (*src == 'D' && (*(src+1) == '\t' || *(src+1) == ' ')) {
                                const char *state = "Deleted  ";
                                while (*state) *dst++ = *state++;
                                src++;
                            } else {
                                *dst++ = *src++;
                            }
                            line_pos++;
                        } else {
                            *dst++ = *src++;
                            line_pos++;
                        }
                    }
                    *dst = '\0';
                    
                    // 逐行打印，避免超过LOG_LINE_MAX_LEN（256字节）
                    ESP_LOGI(TAG, "📋 Task List (State/Priority/Stack):");
                    ESP_LOGI(TAG, "Task Name          State      Prio  Stack  Num");
                    ESP_LOGI(TAG, "------------------------------------------------");
                    
                    // 按行分割输出
                    char *line = enhanced_buffer;
                    char *next_line;
                    while ((next_line = strchr(line, '\n')) != NULL) {
                        *next_line = '\0';
                        if (strlen(line) > 0) {
                            ESP_LOGI(TAG, "%s", line);
                        }
                        line = next_line + 1;
                    }
                    if (strlen(line) > 0) {
                        ESP_LOGI(TAG, "%s", line);
                    }
                    
                    free(enhanced_buffer);
                } else {
                    // 回退：逐行打印原始输出
                    ESP_LOGI(TAG, "📋 Task List (State/Priority/Stack):");
                    ESP_LOGI(TAG, "Task Name          State  Prio  Stack  Num");
                    ESP_LOGI(TAG, "----------------------------------------------");
                    
                    char *line = task_list_buffer;
                    char *next_line;
                    while ((next_line = strchr(line, '\n')) != NULL) {
                        *next_line = '\0';
                        if (strlen(line) > 0) {
                            ESP_LOGI(TAG, "%s", line);
                        }
                        line = next_line + 1;
                    }
                    if (strlen(line) > 0) {
                        ESP_LOGI(TAG, "%s", line);
                    }
                }
            } else {
                ESP_LOGW(TAG, "Failed to allocate buffer for task list");
            }
            
            // 打印CPU运行时间统计
            if (runtime_stats_buffer == NULL) {
                runtime_stats_buffer = (char *)malloc(2048);
            }
            
            if (runtime_stats_buffer != NULL) {
                vTaskGetRunTimeStats(runtime_stats_buffer);
                
                // 逐行打印CPU统计
                ESP_LOGI(TAG, "⏱️  CPU Runtime Statistics:");
                ESP_LOGI(TAG, "Task Name          Abs Time       %% Time");
                ESP_LOGI(TAG, "----------------------------------------------");
                
                char *line = runtime_stats_buffer;
                char *next_line;
                while ((next_line = strchr(line, '\n')) != NULL) {
                    *next_line = '\0';
                    if (strlen(line) > 0) {
                        ESP_LOGI(TAG, "%s", line);
                    }
                    line = next_line + 1;
                }
                if (strlen(line) > 0) {
                    ESP_LOGI(TAG, "%s", line);
                }
            } else {
                ESP_LOGW(TAG, "Failed to allocate buffer for runtime stats");
            }
            
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "Legend:");
            ESP_LOGI(TAG, "  State: Running/Blocked/Suspended/Deleted");
            ESP_LOGI(TAG, "  Prio:  5=NFC(Highest), 4=Mic/Motor, 3=BLE, 2=HeartRate, 1=SysMonitor");
            ESP_LOGI(TAG, "  Stack: Free stack in words (4 bytes/word)");
            ESP_LOGI(TAG, "========================================");
        }

        last_free_heap = current_free_heap;

        vTaskDelay(pdMS_TO_TICKS(monitor_interval_ms));
    }
}

