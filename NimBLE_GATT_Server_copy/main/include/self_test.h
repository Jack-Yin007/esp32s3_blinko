/**
 * @file self_test.h
 * @brief 设备自检模块 - 测试所有硬件和传感器
 */

#ifndef SELF_TEST_H
#define SELF_TEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 测试结果状态
typedef enum {
    TEST_STATUS_NOT_RUN = 0,    // 未运行
    TEST_STATUS_RUNNING,         // 测试中
    TEST_STATUS_PASS,            // 通过
    TEST_STATUS_FAIL,            // 失败
    TEST_STATUS_WARNING,         // 警告（部分功能异常）
    TEST_STATUS_SKIP,            // 跳过（未安装）
} test_status_t;

// 设备类型（根据实际硬件）
typedef enum {
    DEVICE_AW9535_GPIO = 0,      // GPIO扩展器
    DEVICE_LED,                  // LED (通过AW9535)
    DEVICE_VIBRATOR,             // 震动马达 (GPIO10)
    DEVICE_CHARGER,              // 充电管理 (CW6305)
    DEVICE_BATTERY,              // 电池监控 (ADC)
    DEVICE_I2S_AUDIO,            // I2S音频
    DEVICE_WAV_PLAYER,           // WAV播放器
    DEVICE_MIC,                  // 麦克风
    DEVICE_MOTOR_SERVO,          // 舵机 (GPIO8)
    DEVICE_NFC,                  // NFC (PN532)
    DEVICE_TOUCH_SENSOR,         // 触摸传感器 (TTP223)
    DEVICE_VOICE_RECOG,          // 语音识别 (GPIO44)
    DEVICE_FLASH_FAT,            // Flash FAT文件系统
    DEVICE_BLE,                  // BLE
    DEVICE_COUNT                 // 设备总数
} device_type_t;

// 单个设备测试结果
typedef struct {
    device_type_t device;
    test_status_t status;
    char name[32];               // 设备名称
    char details[128];           // 详细信息
    uint32_t test_time_ms;       // 测试耗时
} device_test_result_t;

// 自检报告
typedef struct {
    uint32_t start_time;
    uint32_t total_time_ms;
    uint8_t total_devices;
    uint8_t passed;
    uint8_t failed;
    uint8_t warnings;
    uint8_t skipped;
    device_test_result_t results[DEVICE_COUNT];
} self_test_report_t;

/**
 * @brief 初始化自检模块
 */
void self_test_init(void);

/**
 * @brief 运行完整的设备自检
 * @param report 输出的测试报告
 * @return true=成功, false=失败
 */
bool self_test_run_all(self_test_report_t *report);

/**
 * @brief 测试单个设备
 * @param device 设备类型
 * @param result 输出的测试结果
 * @return true=通过, false=失败
 */
bool self_test_run_single(device_type_t device, device_test_result_t *result);

/**
 * @brief 获取最近一次测试报告
 * @return 测试报告指针
 */
const self_test_report_t* self_test_get_last_report(void);

/**
 * @brief 打印测试报告到日志
 * @param report 测试报告
 */
void self_test_print_report(const self_test_report_t *report);

#ifdef __cplusplus
}
#endif

#endif // SELF_TEST_H
