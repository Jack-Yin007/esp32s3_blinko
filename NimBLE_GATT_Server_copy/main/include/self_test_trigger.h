/**
 * @file self_test_trigger.h
 * @brief BLE触发自检模式
 * 
 * 通过BLE命令触发自检，停止非必要任务释放内存，执行硬件测试
 */

#ifndef SELF_TEST_TRIGGER_H
#define SELF_TEST_TRIGGER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 触发自检模式
 * 
 * 此函数会：
 * 1. 停止所有非必要任务（NFC、音频、触发检测等）
 * 2. 释放大部分内存
 * 3. 执行硬件自检
 * 4. 通过BLE发送测试结果
 * 5. 重启设备
 * 
 * @return ESP_OK 成功触发（实际不会返回，会重启）
 */
esp_err_t trigger_selftest_via_ble(void);

/**
 * @brief 检查是否正在自检
 * @return true 正在自检
 */
bool is_selftest_running(void);

#ifdef __cplusplus
}
#endif

#endif // SELF_TEST_TRIGGER_H
