/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#ifndef GATT_SVR_H
#define GATT_SVR_H

/* Includes */
/* NimBLE GATT APIs */
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

/* NimBLE GAP APIs */
#include "host/ble_gap.h"

/* Public function declarations */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_subscribe_cb(struct ble_gap_event *event);
void gatt_svr_disconnect_cb(void);  // Reset state on disconnect
int gatt_svc_init(void);

/* ==================== B11通信协议接口 ==================== */

/**
 * @brief 发送通知数据到手机
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return int 0成功，其他失败
 */
int b11_send_notification(const uint8_t *data, uint16_t len);

/**
 * @brief 发送音频数据到手机
 * 
 * @param data PCM音频数据指针
 * @param len 数据长度
 * @return int 0成功，其他失败
 */
int b11_send_audio_data(const uint8_t *data, uint16_t len);

/**
 * @brief 上报触发动作
 * 
 * @param trigger_type 触发类型 (0x01:触摸, 0x02:接近, 0x03:语音)
 * @param combo_num 组合号 (0x00-0x08)
 * @return int 0成功，其他失败
 */
int b11_report_trigger_action(uint8_t trigger_type, uint8_t combo_num);

/**
 * @brief 上报传感器数据
 * 
 * @param data_type 数据类型 (0x01:音量, 0x02:触摸, 0x03:距离, 0x04:光线)
 * @param value 数据值
 * @return int 0成功，其他失败
 */
int b11_report_sensor_data(uint8_t data_type, uint8_t value);

/**
 * @brief 上报设备状态
 * 
 * @param status_type 状态类型 (0x00-0x04:电量, 0x07-0x08:充电)
 * @param value 状态值
 * @return int 0成功，其他失败
 */
int b11_report_device_status(uint8_t status_type, uint8_t value);

/**
 * @brief 上报错误
 * 
 * @param error_code 错误码 (0x01:命令错误, 0x02:传感器故障, 0x03:电池低压)
 * @return int 0成功，其他失败
 */
int b11_report_error(uint8_t error_code);

/* ==================== 日志服务接口 ==================== */

/**
 * @brief 发送日志数据（实时流）
 * 
 * @param data 日志数据
 * @param len 数据长度
 * @return int 0成功，其他失败
 */
int log_send_notification(const uint8_t *data, uint16_t len);

/* ==================== 自检服务接口 ==================== */

/**
 * @brief 触发设备自检
 * @return int 0=成功开始
 */
int gatt_svc_trigger_self_test(void);

#endif // GATT_SVR_H
