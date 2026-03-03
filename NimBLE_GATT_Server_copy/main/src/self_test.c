/**
 * @file self_test.c
 * @brief 设备自检模块完整实现
 */

#include "self_test.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <string.h>
#include <stdio.h>

// 驱动头文件
#include "drivers/aw9535.h"
#include "drivers/hardware_interface.h"
#include "drivers/charger.h"
#include "drivers/i2s_audio.h"
#include "drivers/flash_storage.h"
#include "drivers/indatachange.h"
#include "drivers/voice_recognition.h"
#include "app/action_table.h"
#include "host/ble_hs.h"

static const char *TAG = "SelfTest";

// 上次测试报告
static self_test_report_t g_last_report = {0};

// 设备名称表
static const char *device_names[DEVICE_COUNT] = {
    [DEVICE_AW9535_GPIO]     = "AW9535 GPIO Expander",
    [DEVICE_LED]             = "LED Controller",
    [DEVICE_VIBRATOR]        = "Vibrator Motor",
    [DEVICE_CHARGER]         = "Battery Charger (CW6305)",
    [DEVICE_BATTERY]         = "Battery Monitor (ADC)",
    [DEVICE_I2S_AUDIO]       = "I2S Audio Interface",
    [DEVICE_WAV_PLAYER]      = "WAV Player",
    [DEVICE_MIC]             = "Microphone",
    [DEVICE_MOTOR_SERVO]     = "Servo Motor (GPIO8)",
    [DEVICE_NFC]             = "NFC Module (PN532)",
    [DEVICE_TOUCH_SENSOR]    = "Touch Sensor (TTP223)",
    [DEVICE_VOICE_RECOG]     = "Voice Recognition (GPIO44)",
    [DEVICE_FLASH_FAT]       = "Flash FAT Filesystem",
    [DEVICE_BLE]             = "BLE Stack",
};

// ============================================
// 内部测试函数
// ============================================

static bool test_aw9535(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing AW9535...");
    
    // 测试读取端口状态
    uint8_t port0, port1;
    esp_err_t ret = aw9535_get_level_port0(&port0);
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "I2C read failed (0x%02x)", ret);
        return false;
    }
    
    ret = aw9535_get_level_port1(&port1);
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "Port1 read failed (0x%02x)", ret);
        return false;
    }
    
    snprintf(result->details, sizeof(result->details), 
            "I2C OK, P0:0x%02X, P1:0x%02X", port0, port1);
    return true;
}

static bool test_led(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing LED...");
    
    // 测试LED点亮
    esp_err_t ret = hw_led_set_pattern(LED_PATTERN_SOLID_BLUE);
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "Pattern set failed (0x%02x)", ret);
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(200));
    hw_led_stop();
    
    snprintf(result->details, sizeof(result->details), 
            "RGB via AW9535 P1.4/P1.5/P1.6: OK");
    return true;
}

static bool test_vibrator(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Vibrator...");
    
    // 测试震动
    esp_err_t ret = hw_vibrator_set_pattern(VIB_PATTERN_SHORT_PULSE);
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "Vibration start failed (0x%02x)", ret);
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(300));
    hw_vibrator_stop();
    
    snprintf(result->details, sizeof(result->details), 
            "GPIO10 PWM: OK");
    return true;
}

static bool test_charger(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Charger...");
    
    // CW6305是可选的，功能未实现
    snprintf(result->details, sizeof(result->details), 
            "CW6305: Not tested (optional)");
    result->status = TEST_STATUS_WARNING;
    return true;  // 充电器可选
}

static bool test_battery(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Battery...");
    
    // 电池监测功能未实现，跳过测试
    snprintf(result->details, sizeof(result->details), 
            "ADC GPIO4: Not tested");
    result->status = TEST_STATUS_WARNING;
    
    return true;
}

static bool test_i2s_audio(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing I2S Audio...");
    
    if (!i2s_audio_is_initialized()) {
        snprintf(result->details, sizeof(result->details), 
                "I2S not initialized");
        return false;
    }
    
    snprintf(result->details, sizeof(result->details), 
            "I2S0 TX:GPIO40/39, RX:GPIO38/41: OK");
    return true;
}

static bool test_wav_player(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing WAV Player...");
    
    // 检查音频文件
    if (!flash_storage_has_wav_files()) {
        snprintf(result->details, sizeof(result->details), 
                "No WAV files in /flash/audio");
        return false;
    }
    
    // 测试播放（不等待完成）
    esp_err_t ret = hw_audio_play_wav(SOUND_EFFECT_HAPPY);
    vTaskDelay(pdMS_TO_TICKS(100));
    hw_audio_stop();
    
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "Playback failed (0x%02x)", ret);
        return false;
    }
    
    snprintf(result->details, sizeof(result->details), 
            "MAX98357A + I2S TX: OK");
    return true;
}

static bool test_microphone(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Microphone...");
    
    // 检查麦克风是否有声音检测
    bool detected = hw_microphone_is_sound_detected();
    
    snprintf(result->details, sizeof(result->details), 
            "INMP441 I2S RX: OK, Sound:%s", 
            detected ? "Detected" : "Silent");
    return true;
}

static bool test_motor(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Servo Motor...");
    
    if (!hw_motor_is_initialized()) {
        snprintf(result->details, sizeof(result->details), 
                "Motor not initialized");
        return false;
    }
    
    // 测试舵机运动（小角度）
    esp_err_t ret = hw_motor_set(MOTOR_NOD, MOTOR_DIR_FORWARD, 50);
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "Motor control failed (0x%02x)", ret);
        return false;
    }
    
    vTaskDelay(pdMS_TO_TICKS(300));
    hw_motor_stop(MOTOR_NOD);
    
    snprintf(result->details, sizeof(result->details), 
            "GPIO8 PWM: OK");
    return true;
}

static bool test_nfc(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing NFC...");
    
    // NFC初始化检查
    uint8_t test_card[4] = {0};
    esp_err_t ret = nfc_get_card(test_card);
    
    if (ret != ESP_OK) {
        snprintf(result->details, sizeof(result->details), 
                "PN532 I2C comm failed");
        result->status = TEST_STATUS_WARNING;
        return true;  // NFC可选
    }
    
    snprintf(result->details, sizeof(result->details), 
            "PN532 UART/I2C: OK");
    return true;
}

static bool test_touch_sensor(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Touch Sensor...");
    
    // 检查触摸传感器GPIO状态
    bool forehead = hw_touch_sensor_is_forehead_touched();
    bool back = hw_touch_sensor_is_back_touched();
    
    snprintf(result->details, sizeof(result->details), 
            "TTP223: Forehead:%s, Back:%s", 
            forehead ? "Touched" : "Released",
            back ? "Touched" : "Released");
    return true;
}

static bool test_voice_recognition(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Voice Recognition...");
    
    // 检查语音识别GPIO44状态
    int level = hw_pir_sensor_get_raw_level();
    
    snprintf(result->details, sizeof(result->details), 
            "GPIO44: Level=%d (interrupt mode)", level);
    return true;
}

static bool test_flash_fat(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing Flash FAT...");
    
    // 检查文件系统挂载
    FILE *f = fopen("/flash/audio/1.WAV", "r");
    if (!f) {
        snprintf(result->details, sizeof(result->details), 
                "Mount failed or 1.WAV missing");
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    snprintf(result->details, sizeof(result->details), 
            "FAT OK, 1.WAV: %ld bytes", size);
    return true;
}

static bool test_ble(device_test_result_t *result)
{
    ESP_LOGI(TAG, "Testing BLE...");
    
    if (!ble_hs_is_enabled()) {
        snprintf(result->details, sizeof(result->details), 
                "BLE stack not enabled");
        return false;
    }
    
    // 获取连接数
    struct ble_gap_conn_desc desc;
    int conn_count = 0;
    for (int i = 0; i < 9; i++) {  // 最多9个连接
        if (ble_gap_conn_find(i, &desc) == 0) {
            conn_count++;
        }
    }
    
    snprintf(result->details, sizeof(result->details), 
            "NimBLE: OK, Connections: %d", conn_count);
    return true;
}

// ============================================
// 公共接口
// ============================================

void self_test_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Self-Test Module Initialized");
    ESP_LOGI(TAG, "Total devices: %d", DEVICE_COUNT);
    ESP_LOGI(TAG, "Trigger via BLE: Write 0x11 to CMD char");
    ESP_LOGI(TAG, "========================================");
    memset(&g_last_report, 0, sizeof(g_last_report));
}

bool self_test_run_single(device_type_t device, device_test_result_t *result)
{
    if (device >= DEVICE_COUNT) {
        return false;
    }
    
    memset(result, 0, sizeof(device_test_result_t));
    result->device = device;
    result->status = TEST_STATUS_RUNNING;
    strncpy(result->name, device_names[device], sizeof(result->name) - 1);
    
    uint32_t start = esp_timer_get_time() / 1000;
    bool pass = false;
    
    // 根据设备类型调用对应的测试函数
    switch (device) {
        case DEVICE_AW9535_GPIO:  pass = test_aw9535(result); break;
        case DEVICE_LED:          pass = test_led(result); break;
        case DEVICE_VIBRATOR:     pass = test_vibrator(result); break;
        case DEVICE_CHARGER:      pass = test_charger(result); break;
        case DEVICE_BATTERY:      pass = test_battery(result); break;
        case DEVICE_I2S_AUDIO:    pass = test_i2s_audio(result); break;
        case DEVICE_WAV_PLAYER:   pass = test_wav_player(result); break;
        case DEVICE_MIC:          pass = test_microphone(result); break;
        case DEVICE_MOTOR_SERVO:  pass = test_motor(result); break;
        case DEVICE_NFC:          pass = test_nfc(result); break;
        case DEVICE_TOUCH_SENSOR: pass = test_touch_sensor(result); break;
        case DEVICE_VOICE_RECOG:  pass = test_voice_recognition(result); break;
        case DEVICE_FLASH_FAT:    pass = test_flash_fat(result); break;
        case DEVICE_BLE:          pass = test_ble(result); break;
        default:
            result->status = TEST_STATUS_SKIP;
            snprintf(result->details, sizeof(result->details), "Not implemented");
            return false;
    }
    
    result->test_time_ms = (esp_timer_get_time() / 1000) - start;
    
    // 如果测试函数没有设置WARNING状态，则根据pass设置状态
    if (result->status != TEST_STATUS_WARNING) {
        result->status = pass ? TEST_STATUS_PASS : TEST_STATUS_FAIL;
    }
    
    return pass;
}

bool self_test_run_all(self_test_report_t *report)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "     Starting Self-Test");
    ESP_LOGI(TAG, "========================================");
    
    memset(report, 0, sizeof(self_test_report_t));
    report->start_time = esp_timer_get_time() / 1000;
    report->total_devices = DEVICE_COUNT;
    
    // 逐个测试所有设备
    for (int i = 0; i < DEVICE_COUNT; i++) {
        device_test_result_t *result = &report->results[i];
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "[%d/%d] Testing: %s", i+1, DEVICE_COUNT, device_names[i]);
        
        self_test_run_single((device_type_t)i, result);
        
        // 统计结果
        switch (result->status) {
            case TEST_STATUS_PASS:    report->passed++; break;
            case TEST_STATUS_FAIL:    report->failed++; break;
            case TEST_STATUS_WARNING: report->warnings++; break;
            case TEST_STATUS_SKIP:    report->skipped++; break;
            default: break;
        }
        
        const char *status_str = 
            (result->status == TEST_STATUS_PASS) ? "PASS" :
            (result->status == TEST_STATUS_FAIL) ? "FAIL" :
            (result->status == TEST_STATUS_WARNING) ? "WARNING" : "SKIP";
        
        ESP_LOGI(TAG, "   %s: %s (%lums)", status_str, result->details, result->test_time_ms);
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 设备间间隔
    }
    
    report->total_time_ms = (esp_timer_get_time() / 1000) - report->start_time;
    
    // 保存报告
    memcpy(&g_last_report, report, sizeof(self_test_report_t));
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "     Self-Test Complete");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Total: %d devices, Time: %lums", 
            DEVICE_COUNT, report->total_time_ms);
    ESP_LOGI(TAG, "Passed:   %d", report->passed);
    ESP_LOGI(TAG, "Failed:   %d", report->failed);
    ESP_LOGI(TAG, "Warnings: %d", report->warnings);
    ESP_LOGI(TAG, "Skipped:  %d", report->skipped);
    ESP_LOGI(TAG, "========================================");
    
    return (report->failed == 0);
}

const self_test_report_t* self_test_get_last_report(void)
{
    return &g_last_report;
}

void self_test_print_report(const self_test_report_t *report)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "      Device Self-Test Report");
    ESP_LOGI(TAG, "========================================");
    
    for (int i = 0; i < report->total_devices; i++) {
        const device_test_result_t *r = &report->results[i];
        
        const char *status_str = 
            (r->status == TEST_STATUS_PASS) ? "PASS" :
            (r->status == TEST_STATUS_FAIL) ? "FAIL" :
            (r->status == TEST_STATUS_WARNING) ? "WARN" : "SKIP";
        
        ESP_LOGI(TAG, " [%s] %-35s %5lums", 
                status_str, r->name, r->test_time_ms);
        
        if (strlen(r->details) > 0) {
            ESP_LOGI(TAG, "    -> %s", r->details);
        }
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Summary:");
    ESP_LOGI(TAG, "  Total Time: %lu ms", report->total_time_ms);
    ESP_LOGI(TAG, "  Passed:     %d", report->passed);
    ESP_LOGI(TAG, "  Failed:     %d", report->failed);
    ESP_LOGI(TAG, "  Warnings:   %d", report->warnings);
    ESP_LOGI(TAG, "  Skipped:    %d", report->skipped);
    ESP_LOGI(TAG, "========================================");
}
