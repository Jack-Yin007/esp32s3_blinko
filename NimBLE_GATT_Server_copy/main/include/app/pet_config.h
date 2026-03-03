#ifndef PET_CONFIG_H
#define PET_CONFIG_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== B11通信协议定义 ==================== */

/* BLE服务UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E */
#define B11_SERVICE_UUID            0x6E400001, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E

/* 特征值UUID */
/* 控制特征值UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (手机→设备) */
#define B11_CONTROL_CHR_UUID        0x6E400002, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E

/* 通知特征值UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (设备→手机) */
#define B11_NOTIFY_CHR_UUID         0x6E400003, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E

/* 音频特征值UUID: 6E400004-B5A3-F393-E0A9-E50E24DCCA9E (设备→手机) */
#define B11_AUDIO_CHR_UUID          0x6E400004, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E

/* ==================== 命令字定义 ==================== */

/* 命令字: 情绪区间设置 */
#define B11_CMD_EMOTION_ZONE        0x10
/* 区间号定义 */
#define B11_ZONE_S                  0x00  // S区间（亢奋）-> 粉色灯+强震
#define B11_ZONE_A                  0x01  // A区间（兴奋）-> 橙色灯+双震
#define B11_ZONE_B                  0x02  // B区间（积极）-> 黄色灯+单震
#define B11_ZONE_C                  0x03  // C区间（日常）-> 蓝色灯+双震
#define B11_ZONE_D                  0x04  // D区间（消极）-> 红色灯+强震

/* 命令字: 触发动作上报 */
#define B11_CMD_TRIGGER_ACTION      0x20
/* 触发类型定义 */
#define B11_TRIGGER_TOUCH           0x01  // 触摸（额头/肚子）
#define B11_TRIGGER_APPROACH        0x02  // 接近（人靠近）
#define B11_TRIGGER_VOICE           0x03  // 语音唤醒

/* 命令字: 传感器数据 */
#define B11_CMD_SENSOR_DATA         0x30
/* 数据类型定义 */
#define B11_SENSOR_VOLUME           0x01  // 音量强度 (0-255)
#define B11_SENSOR_TOUCH            0x02  // 触摸强度 (0-100)
#define B11_SENSOR_DISTANCE         0x03  // 接近距离 (0-100)
#define B11_SENSOR_LIGHT            0x04  // 环境光 (0-255)

/* 命令字: 录音控制 */
#define B11_CMD_RECORD_CONTROL      0x40
/* 开关定义 */
#define B11_RECORD_OFF              0x00  // 关闭麦克风
#define B11_RECORD_ON               0x01  // 开启麦克风（16kHz/16bit/PCM）

/* 命令字: 设备状态 */
#define B11_CMD_DEVICE_STATUS       0x50
/* 状态类型定义 */
#define B11_STATUS_BATTERY_0        0x00  // 电池电量 0%
#define B11_STATUS_BATTERY_20       0x01  // 电池电量 20%
#define B11_STATUS_BATTERY_40       0x02  // 电池电量 40%
#define B11_STATUS_BATTERY_60       0x03  // 电池电量 60%
#define B11_STATUS_BATTERY_100      0x04  // 电池电量 100%
#define B11_STATUS_NOT_CHARGING     0x07  // 未充电
#define B11_STATUS_CHARGING         0x08  // 充电中

/* 命令字: 错误上报 */
#define B11_CMD_ERROR_REPORT        0x60
/* 错误码定义 */
#define B11_ERROR_CMD_INVALID       0x01  // 命令错误
#define B11_ERROR_SENSOR_FAULT      0x02  // 传感器故障
#define B11_ERROR_LOW_BATTERY       0x03  // 电池低压

/* ==================== 设备配置 ==================== */

/* 设备名称 */
#define B11_DEVICE_NAME             "Blinko"

/* 音频参数 */
#define B11_AUDIO_SAMPLE_RATE       16000  // 16kHz
#define B11_AUDIO_BITS_PER_SAMPLE   16     // 16bit
#define B11_AUDIO_FORMAT            0      // PCM

/**
 * @brief 初始化宠物机器人配置系统
 * 
 * @return esp_err_t 
 */
esp_err_t pet_config_init(void);

#ifdef __cplusplus
}
#endif

#endif // PET_CONFIG_H