/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "gatt_svc.h"
#include "common.h"
#include "led_driver.h"
#include "app/emotion_manager.h"
#include "app/action_table.h"
#include "drivers/hardware_interface.h"
#include "drivers/indatachange.h"
#include "drivers/voice_recognition.h"
#include "drivers/playback_test.h"
#include "drivers/charger.h"
#include "audio.h"
#include "nfc_card.h"
#include "nfc_storage.h"
#include "self_test.h"
#include "self_test_trigger.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "os/os_mbuf.h"
#include "ble/ble_ota.h"
#include <string.h>

/* ==================== B11通信协议定义 (本地副本) ==================== */
/* 如果pet_config.h未正确包含，使用本地定义 */

/* 命令字定义 */
#define B11_CMD_EMOTION_ZONE        0x10
#define B11_CMD_TRIGGER_ACTION      0x20
#define B11_CMD_SENSOR_DATA         0x30
#define B11_CMD_RECORD_CONTROL      0x40
#define B11_CMD_DEVICE_STATUS       0x50
#define B11_CMD_ERROR_REPORT        0x60
#define B11_CMD_PLAYBACK_TEST       0x70  // 播放+录制测试命令
#define B11_CMD_SELF_TEST           0x80  // 设备自检命令
#define B11_CMD_CHARGER_CONTROL     0x90

/* 区间号定义 */
#define B11_ZONE_S                  0x00
#define B11_ZONE_A                  0x01
#define B11_ZONE_B                  0x02
#define B11_ZONE_C                  0x03
#define B11_ZONE_D                  0x04

/* 触发类型定义 */
#define B11_TRIGGER_TOUCH           0x01
#define B11_TRIGGER_APPROACH        0x02
#define B11_TRIGGER_VOICE           0x03

/* 录音控制 */
#define B11_RECORD_OFF              0x00
#define B11_RECORD_ON               0x01

#define B11_CHARGER_RESUME          0x00
#define B11_CHARGER_SHUTDOWN        0x01

/* 错误码定义 */
#define B11_ERROR_CMD_INVALID       0x01
#define B11_ERROR_SENSOR_FAULT      0x02
#define B11_ERROR_LOW_BATTERY       0x03

/* NFC命令定义 */
#define NFC_CMD_SET_MY_CARD         0x20  // 设置我的名片
#define NFC_CMD_GET_MY_CARD         0x21  // 读取我的名片
#define NFC_CMD_GET_FRIEND_CARD     0x22  // 读取好友名片
#define NFC_CMD_GET_FRIEND_COUNT    0x23  // 获取好友数量
#define NFC_CMD_CARD_EXCHANGED      0x30  // 名片交换事件(设备→手机)

/* NFC状态码 */
#define NFC_STATUS_OK               0x00
#define NFC_STATUS_ERROR            0x01
#define NFC_STATUS_NO_FRIEND        0x02

/* Private function declarations */
static int b11_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);
static int b11_notify_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static int b11_audio_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int b11_nfc_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);
static int b11_voice_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);
static void b11_handle_control_command(const uint8_t *data, uint16_t len);
static void audio_data_callback(const uint8_t *data, uint16_t len);
static void nfc_card_exchanged_callback(const uint8_t card[], void *user_data);

/* Log function declarations */
static int log_size_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int log_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int log_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);

/* OTA function declarations */
static int ota_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ota_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ota_progress_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);
static int ota_customer_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);

/* Private variables */
/* 
 * B11通信协议UUID定义 
 * 
 * 所有UUID都基于同一个基础UUID，但在第13-14字节位置使用不同的值来确保唯一性：
 * 
 * UUID格式: XXXXXXXX-B5A3-F393-E0A9-E50E24DCCA9E
 *           ^^^^^^^^ 这部分不同
 * 
 * 服务UUID:    6E400001 (字节: 0x01, 0x00)
 * 控制特征值:  6E400002 (字节: 0x02, 0x00)
 * 通知特征值:  6E400003 (字节: 0x03, 0x00)
 * 音频特征值:  6E400004 (字节: 0x04, 0x00)
 * 
 * BLE_UUID128_INIT使用小端序(Little Endian)，从右到左：
 * 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 * = {0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e}
 *    <-------------------------------------------------------------------  这里是01  ------>
 */

/* B11 Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E */
static const ble_uuid128_t b11_service_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

/* Control Characteristic UUID: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E (手机→设备) */
static const ble_uuid128_t b11_control_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);  // 注意这里是02
static uint16_t b11_control_chr_handle;

/* Notify Characteristic UUID: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E (设备→手机) */
static const ble_uuid128_t b11_notify_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);  // 注意这里是03
static uint16_t b11_notify_chr_handle;
static uint16_t b11_notify_conn_handle = 0;
static bool b11_notify_enabled = false;

/* Audio Characteristic UUID: 6E400004-B5A3-F393-E0A9-E50E24DCCA9E (设备→手机) */
static const ble_uuid128_t b11_audio_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x04, 0x00, 0x40, 0x6e);  // 注意这里是04
static uint16_t b11_audio_chr_handle;
static uint16_t b11_audio_conn_handle = 0;
static bool b11_audio_enabled = false;

/* NFC Characteristic UUID: 6E400006-B5A3-F393-E0A9-E50E24DCCA9E (双向通信) */
static const ble_uuid128_t b11_nfc_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x06, 0x00, 0x40, 0x6e);  // 注意这里是06
static uint16_t b11_nfc_chr_handle;
static uint16_t b11_nfc_conn_handle = 0;
static bool b11_nfc_notify_enabled = false;

/* Voice Characteristic UUID: 6E400007-B5A3-F393-E0A9-E50E24DCCA9E (Notify) */
static const ble_uuid128_t b11_voice_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x07, 0x00, 0x40, 0x6e);  // UUID 07 for voice
static uint16_t b11_voice_chr_handle;
static uint16_t b11_voice_conn_handle = 0;
static bool b11_voice_notify_enabled = false;

/* ==================== LOG Service UUIDs ==================== */
/* Log Service UUID: 6E400010-B5A3-F393-E0A9-E50E24DCCA9E */
static const ble_uuid128_t log_service_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x10, 0x00, 0x40, 0x6e);

/* Log Size Characteristic UUID: 6E400011 - Read current log size */
static const ble_uuid128_t log_size_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x11, 0x00, 0x40, 0x6e);
static uint16_t log_size_chr_handle;

/* Log Data Characteristic UUID: 6E400012 - Read/Notify log data */
static const ble_uuid128_t log_data_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x12, 0x00, 0x40, 0x6e);
static uint16_t log_data_chr_handle;
static uint16_t log_data_conn_handle = 0;
static bool log_data_notify_enabled = false;

/* Log Control Characteristic UUID: 6E400013 - Write commands */
static const ble_uuid128_t log_control_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x13, 0x00, 0x40, 0x6e);
static uint16_t log_control_chr_handle;

/* Log service state */
static size_t log_read_offset = 0;      // Current read offset for batch download
static size_t log_snapshot_size = 0;    // Snapshot of log size at READ_START
static size_t log_snapshot_write_pos = 0;  // Snapshot of write_pos at READ_START
static bool log_snapshot_wrapped = false;  // Snapshot of buffer_wrapped at READ_START

/* ==================== BLE OTA Service UUIDs ==================== */
/* Protocol matches Python ble_ota_tester.py */
/* OTA Service UUID: 00008018-0000-1000-8000-00805f9b34fb */
#define BLE_SVC_OTA_UUID16  0x8018

/* 0x8020: RECV_FW - Firmware data + ACK notify */
#define BLE_SVC_OTA_CHR_DATA_UUID16  0x8020
static uint16_t ota_data_chr_handle;
static uint16_t ota_data_conn_handle = 0;

/* OTA sector buffer for CRC verification */
#define OTA_SECTOR_SIZE 4096
static uint8_t *ota_sector_buffer = NULL;
static uint16_t ota_current_sector = 0xFFFF;
static uint16_t ota_sector_offset = 0;

/* 0x8021: PROGRESS_BAR - Progress updates */
#define BLE_SVC_OTA_CHR_PROGRESS_UUID16  0x8021
static uint16_t ota_progress_chr_handle;
static uint16_t ota_progress_conn_handle = 0;
static bool ota_progress_notify_enabled = false;

/* 0x8022: COMMAND - Control commands + ACK notify */
#define BLE_SVC_OTA_CHR_CONTROL_UUID16  0x8022
static uint16_t ota_control_chr_handle;
static uint16_t ota_control_conn_handle = 0;
static bool ota_control_notify_enabled = false;

/* 0x8023: CUSTOMER - Reserved */
#define BLE_SVC_OTA_CHR_CUSTOMER_UUID16  0x8023
static uint16_t ota_customer_chr_handle;

/* Current emotion zone */
static uint8_t current_emotion_zone = B11_ZONE_C;  // Default to C zone (daily)
static bool recording_enabled = false;

/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /* B11 Communication Service */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &b11_service_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* Control characteristic (Write) */
              .uuid = &b11_control_chr_uuid.u,
              .access_cb = b11_control_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              .val_handle = &b11_control_chr_handle},
             {/* Notify characteristic (Notify) */
              .uuid = &b11_notify_chr_uuid.u,
              .access_cb = b11_notify_chr_access,
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &b11_notify_chr_handle},
             {/* Audio characteristic (Notify) */
              .uuid = &b11_audio_chr_uuid.u,
              .access_cb = b11_audio_chr_access,
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &b11_audio_chr_handle},
             {/* NFC characteristic (Read/Write/Notify) */
              .uuid = &b11_nfc_chr_uuid.u,
              .access_cb = b11_nfc_chr_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &b11_nfc_chr_handle},
             {/* Voice characteristic (Notify) */
              .uuid = &b11_voice_chr_uuid.u,
              .access_cb = b11_voice_chr_access,
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &b11_voice_chr_handle},
             {
                 0, /* No more characteristics in this service. */
             }}},

    /* OTA Service - Protocol matches ble_ota_tester.py */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(BLE_SVC_OTA_UUID16),
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* 0x8020: RECV_FW - Firmware data (Write/Notify for ACK) */
              .uuid = BLE_UUID16_DECLARE(BLE_SVC_OTA_CHR_DATA_UUID16),
              .access_cb = ota_data_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &ota_data_chr_handle},
             {/* 0x8021: PROGRESS_BAR - Progress updates (Notify) */
              .uuid = BLE_UUID16_DECLARE(BLE_SVC_OTA_CHR_PROGRESS_UUID16),
              .access_cb = ota_progress_chr_access,
              .flags = BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &ota_progress_chr_handle},
             {/* 0x8022: COMMAND - Control commands (Write/Notify for ACK) */
              .uuid = BLE_UUID16_DECLARE(BLE_SVC_OTA_CHR_CONTROL_UUID16),
              .access_cb = ota_control_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &ota_control_chr_handle},
             {/* 0x8023: CUSTOMER - Reserved */
              .uuid = BLE_UUID16_DECLARE(BLE_SVC_OTA_CHR_CUSTOMER_UUID16),
              .access_cb = ota_customer_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &ota_customer_chr_handle},
             {
                 0, /* No more characteristics in this service. */
             }}},

    /* LOG Service - Remote log viewing and download */
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &log_service_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* Log Size - Read current log buffer size */
              .uuid = &log_size_chr_uuid.u,
              .access_cb = log_size_chr_access,
              .flags = BLE_GATT_CHR_F_READ,
              .val_handle = &log_size_chr_handle},
             {/* Log Data - Read/Notify log content */
              .uuid = &log_data_chr_uuid.u,
              .access_cb = log_data_chr_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &log_data_chr_handle},
             {/* Log Control - Write commands (clear, set level, stream on/off) */
              .uuid = &log_control_chr_uuid.u,
              .access_cb = log_control_chr_access,
              .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
              .val_handle = &log_control_chr_handle},
             {
                 0, /* No more characteristics in this service. */
             }}},

    {
        0, /* No more services. */
    },
};

/* Private functions */

/**
 * @brief Handle control command from phone
 */
static void b11_handle_control_command(const uint8_t *data, uint16_t len) {
    if (len < 1) {
        ESP_LOGW(TAG, "Empty control command");
        return;
    }

    uint8_t cmd = data[0];
    
    switch (cmd) {
    case B11_CMD_EMOTION_ZONE:
        if (len >= 2) {
            uint8_t zone = data[1];
            if (zone <= B11_ZONE_D) {
                // 更新情绪管理器
                emotion_manager_set_zone((emotion_zone_t)zone);
                
                // 加载对应的动作组合
                uint8_t combo_count = action_table_get_combo_count((emotion_zone_t)zone);
                ESP_LOGI(TAG, "Loaded %d action combos for zone %s", 
                         combo_count, 
                         emotion_manager_get_zone_name((emotion_zone_t)zone));
                
                // 保存到NVS (持久化)
                emotion_manager_save_to_nvs();
                
                current_emotion_zone = zone;
                ESP_LOGI(TAG, "Emotion zone set to: 0x%02X - %s", 
                         zone, 
                         emotion_manager_get_zone_name((emotion_zone_t)zone));
            } else {
                ESP_LOGW(TAG, "Invalid emotion zone: 0x%02X", zone);
                b11_report_error(B11_ERROR_CMD_INVALID);
            }
        }
        break;

    case B11_CMD_RECORD_CONTROL:
        if (len >= 2) {
            uint8_t enable = data[1];
            recording_enabled = (enable == B11_RECORD_ON);
            ESP_LOGI(TAG, "Recording %s", recording_enabled ? "enabled" : "disabled");
            
            // 启动/停止INMP441麦克风音频流
            if (recording_enabled) {
                ESP_LOGI(TAG, "Start audio streaming (8kHz/16bit/Mono/PCM)");
                hw_microphone_enable_streaming(audio_data_callback);
            } else {
                ESP_LOGI(TAG, "Stop audio streaming");
                hw_microphone_disable_streaming();
            }
        }
        break;

    case B11_CMD_PLAYBACK_TEST:
        // 播放+录制测试命令
        // 格式: 0x70 [WAV_ID]
        if (len >= 2) {
            uint8_t wav_id = data[1];
            ESP_LOGI(TAG, "🧪 Playback+Recording test requested: WAV ID=%d", wav_id);
            
            // 运行测试
            esp_err_t ret = playback_test_start(wav_id);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ Test started successfully");
            } else {
                ESP_LOGE(TAG, "❌ Failed to start test: %s", esp_err_to_name(ret));
                b11_report_error(B11_ERROR_CMD_INVALID);
            }
        } else {
            ESP_LOGW(TAG, "Invalid playback test command (missing WAV ID)");
            b11_report_error(B11_ERROR_CMD_INVALID);
        }
        break;

    case B11_CMD_SELF_TEST:
        // 设备自检命令
        // 格式: 0x80 [0x01=开始] 或 0x80 [0x00=停止]
        if (len >= 2) {
            uint8_t action = data[1];
            if (action == 0x01) {
                ESP_LOGI(TAG, "🔧 Self-test requested via BLE");
                esp_err_t ret = gatt_svc_trigger_self_test();
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ Self-test started");
                } else {
                    ESP_LOGE(TAG, "❌ Failed to start self-test");
                    b11_report_error(B11_ERROR_CMD_INVALID);
                }
            } else {
                ESP_LOGW(TAG, "Invalid self-test action: 0x%02X", action);
                b11_report_error(B11_ERROR_CMD_INVALID);
            }
        } else {
            ESP_LOGW(TAG, "Invalid self-test command (missing action)");
            b11_report_error(B11_ERROR_CMD_INVALID);
        }
        break;

    case B11_CMD_CHARGER_CONTROL:
	{
		if (len < 2) {
			ESP_LOGW(TAG, "Invalid charger control parameter");
			b11_report_error(B11_ERROR_CMD_INVALID);
			break;
		}

		bool shutdown = (data[1] == B11_CHARGER_SHUTDOWN);
		charger_shutdown(shutdown);
		break;
	}
    default:
        ESP_LOGW(TAG, "Unknown command: 0x%02X", cmd);
        b11_report_error(B11_ERROR_CMD_INVALID);
        break;
    }
}

/**
 * @brief Control characteristic access callback
 */
static int b11_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        ESP_LOGI(TAG, "Control write; conn_handle=%d attr_handle=%d len=%d",
                 conn_handle, attr_handle, ctxt->om->om_len);

        if (attr_handle == b11_control_chr_handle) {
            // Process control command
            uint8_t buffer[256];
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > sizeof(buffer)) {
                len = sizeof(buffer);
            }
            
            os_mbuf_copydata(ctxt->om, 0, len, buffer);
            
            // Handle command
            b11_handle_control_command(buffer, len);
            
            return 0;
        }
        break;

    default:
        ESP_LOGW(TAG, "Unexpected access op: %d", ctxt->op);
        break;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief Notify characteristic access callback
 */
static int b11_notify_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGD(TAG, "Notify chr access; conn_handle=%d op=%d", conn_handle, ctxt->op);
    return 0;
}

/**
 * @brief Audio characteristic access callback
 */
static int b11_audio_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGD(TAG, "Audio chr access; conn_handle=%d op=%d", conn_handle, ctxt->op);
    return 0;
}

/* Public functions */

/**
 * @brief Send notification data to phone
 */
int b11_send_notification(const uint8_t *data, uint16_t len) {
    if (!b11_notify_enabled || b11_notify_conn_handle == 0) {
        ESP_LOGW(TAG, "Notification not enabled");
        return -1;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf");
        return -1;
    }

    int rc = ble_gattc_notify_custom(b11_notify_conn_handle, b11_notify_chr_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification: %d", rc);
        return rc;
    }

    ESP_LOGD(TAG, "Notification sent (%d bytes)", len);
    return 0;
}

/**
 * @brief Send audio data to phone
 */
int b11_send_audio_data(const uint8_t *data, uint16_t len) {
    static uint32_t mbuf_fail_count = 0;
    
    if (!b11_audio_enabled || b11_audio_conn_handle == 0) {
        return -1;  // Silently fail if not enabled
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        mbuf_fail_count++;
        if (mbuf_fail_count % 100 == 1) {
            ESP_LOGW(TAG, "mbuf allocation failed! count=%lu, requested_len=%d", mbuf_fail_count, len);
        }
        return -1;
    }
    
    // 检查实际分配的mbuf大小
    uint16_t actual_len = OS_MBUF_PKTLEN(om);
    if (actual_len != len && mbuf_fail_count % 10 == 0) {
        ESP_LOGW(TAG, "mbuf size mismatch! requested=%d, actual=%d", len, actual_len);
    }

    int rc = ble_gattc_notify_custom(b11_audio_conn_handle, b11_audio_chr_handle, om);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * @brief 音频数据回调包装函数（从麦克风驱动调用）
 * 
 * 直接发送I2S缓冲区数据，观察实际通知率
 */
static void audio_data_callback(const uint8_t *data, uint16_t len) {
    static uint32_t call_count = 0;
    static uint32_t send_ok = 0;
    static uint32_t send_fail = 0;
    
    call_count++;
    int rc = b11_send_audio_data(data, len);
    
    if (rc == 0) {
        send_ok++;
    } else {
        send_fail++;
    }
    
    // 每100次打印统计
    if (call_count % 100 == 0) {
        ESP_LOGI(TAG, "Audio: calls=%lu, len=%d, ok=%lu (%.1f%%), fail=%lu", 
                 call_count, len, send_ok, 
                 (float)send_ok / call_count * 100, send_fail);
    }
}

/**
 * @brief NFC名片交换完成回调（从NFC驱动调用）
 * 
 * 当通过NFC接收到好友名片时，此回调被触发，
 * 通过BLE通知上位机有新名片接收
 * 
 * 注意：名片已经在indatachange.c中保存到Flash，这里只负责BLE通知
 */
static void nfc_card_exchanged_callback(const uint8_t card[], void *user_data) {
    ESP_LOGI(TAG, "📇 NFC card exchanged callback triggered");
    
    // 名片已在indatachange.c中保存，这里只负责BLE通知
    if (!b11_nfc_notify_enabled || b11_nfc_conn_handle == 0) {
        ESP_LOGW(TAG, "⚠️ BLE not connected, card saved to Flash only (will sync on reconnect)");
        return;
    }
    
    // 构造通知消息: [NFC_CMD_CARD_EXCHANGED(1)] + [140字节名片数据]
    uint8_t notify_data[1 + sizeof(nfc_card_t)];
    notify_data[0] = NFC_CMD_CARD_EXCHANGED;
    memcpy(&notify_data[1], card, sizeof(nfc_card_t));
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for NFC notification");
        return;
    }
    
    int rc = ble_gattc_notify_custom(b11_nfc_conn_handle, b11_nfc_chr_handle, om);
    if (rc == 0) {
        ESP_LOGI(TAG, "✅ Friend card notification sent via BLE");
    } else {
        ESP_LOGE(TAG, "❌ Failed to send NFC notification: %d", rc);
    }
}

/**
 * @brief NFC特性访问回调
 */
static int b11_nfc_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "NFC characteristic read");
            return 0;
            
        case BLE_GATT_ACCESS_OP_WRITE_CHR: {
            if (ctxt->om->om_len < 1) {
                ESP_LOGE(TAG, "Invalid NFC command length");
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            
            uint8_t cmd = ctxt->om->om_data[0];
            uint8_t response[128];
            uint16_t resp_len = 0;
            
            ESP_LOGI(TAG, "NFC command received: 0x%02X", cmd);
            
            switch (cmd) {
                case NFC_CMD_SET_MY_CARD: {
                    // 设置我的名片 (需要141字节: 1字节cmd + 140字节数据)
                    if (ctxt->om->om_len != (1 + sizeof(nfc_card_t))) {
                        ESP_LOGE(TAG, "Invalid card data length: %d (expected %d)", 
                                 ctxt->om->om_len, 1 + sizeof(nfc_card_t));
                        response[0] = NFC_STATUS_ERROR;
                        resp_len = 1;
                        break;
                    }
                    
                    esp_err_t ret = nfc_set_card(&ctxt->om->om_data[1]);
                    if (ret == ESP_OK) {
                        ESP_LOGI(TAG, "✅ My card updated successfully via BLE");
                        response[0] = NFC_STATUS_OK;
                    } else {
                        ESP_LOGE(TAG, "❌ Failed to set card");
                        response[0] = NFC_STATUS_ERROR;
                    }
                    resp_len = 1;
                    break;
                }
                
                case NFC_CMD_GET_MY_CARD: {
                    // 读取我的名片
                    nfc_card_t my_card;
                    if (nfc_storage_load_my_card(&my_card)) {
                        response[0] = NFC_STATUS_OK;
                        memcpy(&response[1], &my_card, sizeof(nfc_card_t));
                        resp_len = 1 + sizeof(nfc_card_t);
                        ESP_LOGI(TAG, "✅ My card read: %.20s", my_card.pet_name);
                    } else {
                        ESP_LOGW(TAG, "⚠️ My card not found in storage");
                        response[0] = NFC_STATUS_ERROR;
                        resp_len = 1;
                    }
                    break;
                }
                
                case NFC_CMD_GET_FRIEND_CARD: {
                    // 读取好友名片
                    nfc_card_t friend_card;
                    if (nfc_storage_load_friend_card(&friend_card)) {
                        response[0] = NFC_STATUS_OK;
                        memcpy(&response[1], &friend_card, sizeof(nfc_card_t));
                        resp_len = 1 + sizeof(nfc_card_t);
                        ESP_LOGI(TAG, "✅ Friend card read: %.20s", friend_card.pet_name);
                        
                        // 清除新朋友标志（APP已读取）
                        nfc_storage_clear_new_friend_flag();
                    } else {
                        ESP_LOGW(TAG, "⚠️ No friend card available");
                        response[0] = NFC_STATUS_NO_FRIEND;
                        resp_len = 1;
                    }
                    break;
                }
                
                case NFC_CMD_GET_FRIEND_COUNT: {
                    // 获取好友数量（当前固定为0或1）
                    bool has_friend = nfc_storage_has_new_friend();
                    
                    response[0] = NFC_STATUS_OK;
                    response[1] = has_friend ? 1 : 0;
                    resp_len = 2;
                    ESP_LOGI(TAG, "Friend count: %d", response[1]);
                    break;
                }
                
                default:
                    ESP_LOGW(TAG, "Unknown NFC command: 0x%02X", cmd);
                    response[0] = NFC_STATUS_ERROR;
                    resp_len = 1;
                    break;
            }
            
            // 通过notification发送响应
            if (b11_nfc_notify_enabled && b11_nfc_conn_handle != 0) {
                struct os_mbuf *om = ble_hs_mbuf_from_flat(response, resp_len);
                if (om != NULL) {
                    ble_gattc_notify_custom(b11_nfc_conn_handle, b11_nfc_chr_handle, om);
                }
            }
            
            return 0;
        }
        
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * @brief Voice wakeup callback (DEPRECATED - now handled by trigger_detector)
 * 
 * NOTE: Voice detection now uses GPIO44 polling in trigger_detector.
 * BLE notifications for voice events should be sent from the trigger callback.
 * This function is kept for reference but is no longer called.
 */
#if 0
static void voice_wakeup_callback(void *user_data) {
    (void)user_data;  // Unused
    
    ESP_LOGI(TAG, "Voice wakeup detected (P1.7 interrupt)");
    
    if (!b11_voice_notify_enabled || b11_voice_conn_handle == 0) {
        ESP_LOGW(TAG, "Voice notifications not enabled");
        return;
    }
    
    // Construct simple notification: [0xA1 = wakeup command]
    uint8_t notify_data[1];
    notify_data[0] = 0xA1;  // Wakeup command
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, 1);
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate mbuf for voice notification");
        return;
    }
    
    int rc = ble_gattc_notify_custom(b11_voice_conn_handle, b11_voice_chr_handle, om);
    if (rc == 0) {
        ESP_LOGI(TAG, "Voice wakeup notification sent via BLE");
    } else {
        ESP_LOGE(TAG, "Failed to send voice notification: %d", rc);
    }
}
#endif

/**
 * @brief Voice characteristic access callback
 */
static int b11_voice_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg) {
    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "Voice characteristic read");
            return 0;
            
        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

/**
 * @brief Report trigger action
 */
int b11_report_trigger_action(uint8_t trigger_type, uint8_t combo_num) {
    uint8_t data[3] = {B11_CMD_TRIGGER_ACTION, trigger_type, combo_num};
    ESP_LOGI(TAG, "Report trigger: type=0x%02X combo=0x%02X", trigger_type, combo_num);
    return b11_send_notification(data, sizeof(data));
}

/**
 * @brief Report sensor data
 */
int b11_report_sensor_data(uint8_t data_type, uint8_t value) {
    uint8_t data[3] = {B11_CMD_SENSOR_DATA, data_type, value};
    ESP_LOGD(TAG, "Report sensor: type=0x%02X value=%d", data_type, value);
    return b11_send_notification(data, sizeof(data));
}

/**
 * @brief Report device status
 */
int b11_report_device_status(uint8_t status_type, uint8_t value) {
    uint8_t data[3] = {B11_CMD_DEVICE_STATUS, status_type, value};
    ESP_LOGI(TAG, "Report status: type=0x%02X value=0x%02X", status_type, value);
    return b11_send_notification(data, sizeof(data));
}

/**
 * @brief Report error
 */
int b11_report_error(uint8_t error_code) {
    uint8_t data[2] = {B11_CMD_ERROR_REPORT, error_code};
    ESP_LOGE(TAG, "Report error: code=0x%02X", error_code);
    return b11_send_notification(data, sizeof(data));
}

/* ==================== OTA Characteristic Callbacks ==================== */

/**
 * @brief Calculate CRC16-CCITT
 */
static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc & 0xFFFF;
}

/**
 * @brief OTA Control Characteristic access callback
 */
static int ota_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        
        // Python tool sends 20-byte command packet: [CMD_ID(2)][Payload(16)][CRC(2)]
        if (om_len < 20) {
            ESP_LOGE(TAG, "OTA Control: invalid length %d (expected 20)", om_len);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t *data = malloc(om_len);
        if (data == NULL) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ble_hs_mbuf_to_flat(ctxt->om, data, om_len, NULL);
        
        // Parse command ID (little-endian, 2 bytes)
        uint16_t cmd_id = data[0] | (data[1] << 8);
        
        // Verify CRC (optional, but good practice)
        // uint16_t received_crc = data[18] | (data[19] << 8);
        // uint16_t calculated_crc = crc16_ccitt(data, 18);
        
        ESP_LOGD(TAG, "OTA Control: cmd_id=0x%04X, CRC OK", cmd_id);

        esp_err_t err = ESP_OK;
        
        // CMD_START_OTA = 0x0001, CMD_END_OTA = 0x0002
        if (cmd_id == 0x0001) {
            // Clean up any previous session
            if (ota_sector_buffer != NULL) {
                free(ota_sector_buffer);
                ota_sector_buffer = NULL;
            }
            ota_current_sector = 0xFFFF;
            ota_sector_offset = 0;
            
            // START_OTA: payload contains firmware size (4 bytes, little-endian)
            uint32_t fw_size = data[2] | (data[3] << 8) | (data[4] << 16) | (data[5] << 24);
            err = ble_ota_start(fw_size);
            ESP_LOGI(TAG, "OTA START: size=%lu, result=%d", fw_size, err);
        } 
        else if (cmd_id == 0x0002) {
            // END_OTA
            err = ble_ota_end();
            ESP_LOGI(TAG, "OTA END: result=%d", err);
            
            // Free sector buffer
            if (ota_sector_buffer != NULL) {
                free(ota_sector_buffer);
                ota_sector_buffer = NULL;
            }
            ota_current_sector = 0xFFFF;
            ota_sector_offset = 0;
        }
        else if (cmd_id == 0x0009) {
            // START_SELFTEST = 0x0009
            ESP_LOGI(TAG, "📋 Self-Test command received via BLE");
            err = trigger_selftest_via_ble();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ Self-test triggered successfully");
            } else {
                ESP_LOGE(TAG, "❌ Failed to trigger self-test: %d", err);
            }
        }
        else {
            ESP_LOGW(TAG, "OTA: Unknown command 0x%04X", cmd_id);
            free(data);
            return BLE_ATT_ERR_UNLIKELY;
        }

        free(data);

        // Save connection handle if not already set
        if (ota_control_conn_handle == 0) {
            ota_control_conn_handle = conn_handle;
            ESP_LOGI(TAG, "OTA Control: saved conn_handle=%d", conn_handle);
        }

        // Send ACK notification - Format: [CMD_ACK(2)][Reply_CMD(2)][Response(2)][Padding(12)][CRC(2)]
        // Always try to send ACK if we have a valid connection handle
        if (ota_control_conn_handle != 0) {
            ESP_LOGI(TAG, "Preparing ACK: notify_enabled=%d conn_handle=%d", 
                     ota_control_notify_enabled, ota_control_conn_handle);
            uint8_t ack[20] = {0};
            
            // Command ID = 0x0003 (CMD_ACK)
            ack[0] = 0x03;
            ack[1] = 0x00;
            
            // Reply Command ID = original command
            ack[2] = cmd_id & 0xFF;
            ack[3] = (cmd_id >> 8) & 0xFF;
            
            // Response: 0x0000 = success, 0x0001 = error
            uint16_t response = (err == ESP_OK) ? 0x0000 : 0x0001;
            ack[4] = response & 0xFF;
            ack[5] = (response >> 8) & 0xFF;
            
            // Padding bytes 6-17 already zero
            
            // Calculate CRC16 of first 18 bytes
            uint16_t crc = crc16_ccitt(ack, 18);
            ack[18] = crc & 0xFF;
            ack[19] = (crc >> 8) & 0xFF;
            
            struct os_mbuf *om = ble_hs_mbuf_from_flat(ack, sizeof(ack));
            if (om != NULL) {
                int rc = ble_gattc_notify_custom(ota_control_conn_handle, ota_control_chr_handle, om);
                ESP_LOGI(TAG, "OTA ACK sent: cmd=0x%04X response=0x%04X rc=%d", cmd_id, response, rc);
            } else {
                ESP_LOGE(TAG, "Failed to allocate mbuf for ACK");
            }
        } else {
            ESP_LOGW(TAG, "Cannot send ACK: conn_handle=%d", ota_control_conn_handle);
        }

        return 0;
    }
    
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        ota_control_conn_handle = conn_handle;
        ota_control_notify_enabled = true;
        ESP_LOGI(TAG, "OTA Control notifications enabled");
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief OTA Data Characteristic access callback (RECV_FW)
 * Packet format: [Sector_Index(2)][Packet_Seq(1)][Payload(n)]
 * Last packet has Packet_Seq = 0xFF and includes CRC16 at the end
 */
static int ota_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        
        // Minimum packet: 2 bytes sector + 1 byte seq + payload
        if (om_len < 4) {
            ESP_LOGE(TAG, "OTA Data: packet too short %d", om_len);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        
        uint8_t *data = malloc(om_len);
        if (data == NULL) {
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        ble_hs_mbuf_to_flat(ctxt->om, data, om_len, NULL);

        // Parse packet header
        uint16_t sector_index = data[0] | (data[1] << 8);
        uint8_t packet_seq = data[2];
        uint8_t *payload = &data[3];
        uint16_t payload_len = om_len - 3;
        
        // Save connection handle if not already set
        if (ota_data_conn_handle == 0) {
            ota_data_conn_handle = conn_handle;
        }
        
        esp_err_t err = ESP_OK;
        uint16_t ack_status = 0x0000;  // Success
        
        // Check if this is a new sector
        if (sector_index != ota_current_sector) {
            // Allocate buffer for new sector
            if (ota_sector_buffer == NULL) {
                ota_sector_buffer = malloc(OTA_SECTOR_SIZE);
                if (ota_sector_buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate sector buffer");
                    free(data);
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }
            }
            memset(ota_sector_buffer, 0xFF, OTA_SECTOR_SIZE);  // Pad with 0xFF
            ota_current_sector = sector_index;
            ota_sector_offset = 0;
            ESP_LOGI(TAG, "OTA: New sector %d", sector_index);
        }
        
        // Last packet (0xFF) includes CRC16 at the end
        if (packet_seq == 0xFF) {
            // Extract CRC from last 2 bytes of payload
            if (payload_len < 2) {
                ESP_LOGE(TAG, "Last packet too short: %d", payload_len);
                ack_status = 0x0003;  // Payload length error
                goto send_ack;
            }
            
            uint16_t received_crc = payload[payload_len - 2] | (payload[payload_len - 1] << 8);
            uint16_t actual_payload_len = payload_len - 2;
            
            // Copy payload (without CRC) to sector buffer
            if (ota_sector_offset + actual_payload_len <= OTA_SECTOR_SIZE) {
                memcpy(ota_sector_buffer + ota_sector_offset, payload, actual_payload_len);
                ota_sector_offset += actual_payload_len;
            }
            
            // Verify CRC of entire sector (always 4096 bytes, padded with 0xFF)
            uint16_t calculated_crc = crc16_ccitt(ota_sector_buffer, OTA_SECTOR_SIZE);
            
            if (calculated_crc != received_crc) {
                ESP_LOGE(TAG, "Sector %d CRC error: rx=0x%04X calc=0x%04X", 
                         sector_index, received_crc, calculated_crc);
                ack_status = 0x0001;  // CRC error
            } else {
                ESP_LOGD(TAG, "Sector %d OK (%d bytes)", sector_index, ota_sector_offset);
                // Write actual data to OTA partition (only received bytes, not padded 4096)
                err = ble_ota_write_data(ota_sector_buffer, ota_sector_offset);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
                    ack_status = 0x0001;
                }
            }
            
            // Reset for next sector
            ota_current_sector = 0xFFFF;
            ota_sector_offset = 0;
        } else {
            // Normal packet - just accumulate in buffer
            if (ota_sector_offset + payload_len <= OTA_SECTOR_SIZE) {
                memcpy(ota_sector_buffer + ota_sector_offset, payload, payload_len);
                ota_sector_offset += payload_len;
                ESP_LOGD(TAG, "OTA Data: sector=%d seq=%d offset=%d", sector_index, packet_seq, ota_sector_offset);
            } else {
                ESP_LOGE(TAG, "Sector buffer overflow!");
                ack_status = 0x0003;  // Payload length error
            }
            
            free(data);
            return 0;  // Don't send ACK for non-final packets
        }
        
send_ack:
        // Only send ACK for the last packet of each sector (packet_seq == 0xFF)
        if (ota_data_conn_handle != 0) {
            // Send ACK notification - Format: [Sector_Index(2)][ACK_Status(2)][Padding(14)][CRC(2)]
            uint8_t ack[20] = {0};
            ack[0] = sector_index & 0xFF;
            ack[1] = (sector_index >> 8) & 0xFF;
            ack[2] = ack_status & 0xFF;
            ack[3] = (ack_status >> 8) & 0xFF;
            // Padding bytes 4-17 already zero
            
            // Calculate CRC16
            uint16_t crc = crc16_ccitt(ack, 18);
            ack[18] = crc & 0xFF;
            ack[19] = (crc >> 8) & 0xFF;
            
            struct os_mbuf *om = ble_hs_mbuf_from_flat(ack, sizeof(ack));
            if (om != NULL) {
                int rc = ble_gattc_notify_custom(ota_data_conn_handle, ota_data_chr_handle, om);
                ESP_LOGI(TAG, "OTA Data ACK sent: sector=%d status=0x%04X rc=%d", 
                         sector_index, (uint16_t)ack[2], rc);
            } else {
                ESP_LOGE(TAG, "Failed to allocate mbuf for Data ACK");
            }
        }
        
        free(data);

        if (err != ESP_OK) {
            return BLE_ATT_ERR_UNLIKELY;
        }

        // Send progress notification
        if (ota_progress_notify_enabled && ota_progress_conn_handle != 0) {
            ble_ota_progress_t progress;
            ble_ota_get_progress(&progress);
            
            uint8_t progress_data[5];
            progress_data[0] = progress.percentage;
            memcpy(&progress_data[1], &progress.received_size, 4);
            
            struct os_mbuf *om = ble_hs_mbuf_from_flat(progress_data, sizeof(progress_data));
            if (om != NULL) {
                ble_gattc_notify_custom(ota_progress_conn_handle, ota_progress_chr_handle, om);
            }
        }

        return 0;
    }
    
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        ota_data_conn_handle = conn_handle;
        ESP_LOGI(TAG, "OTA Data (RECV_FW) notifications enabled");
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief OTA Progress Characteristic access callback (PROGRESS_BAR)
 */
static int ota_progress_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        ota_progress_conn_handle = conn_handle;
        ota_progress_notify_enabled = true;
        ESP_LOGI(TAG, "OTA Progress (PROGRESS_BAR) notifications enabled");
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief OTA Customer Characteristic access callback (Reserved)
 */
static int ota_customer_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGD(TAG, "OTA Customer characteristic write (reserved)");
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/* ==================== GATT Register/Subscribe Callbacks ==================== */

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "Registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG,
                 "Registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "Registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update notification/indication subscription status
 */
void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "Subscribe event; conn_handle=%d attr_handle=%d notify=%d indicate=%d",
                 event->subscribe.conn_handle, 
                 event->subscribe.attr_handle,
                 event->subscribe.cur_notify,
                 event->subscribe.cur_indicate);
    }

    /* Check which characteristic was subscribed */
    if (event->subscribe.attr_handle == b11_notify_chr_handle) {
        b11_notify_conn_handle = event->subscribe.conn_handle;
        b11_notify_enabled = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "B11 Notify %s", b11_notify_enabled ? "enabled" : "disabled");
    } else if (event->subscribe.attr_handle == b11_audio_chr_handle) {
        b11_audio_conn_handle = event->subscribe.conn_handle;
        b11_audio_enabled = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "B11 Audio %s", b11_audio_enabled ? "enabled" : "disabled");
        
        /* Start/stop audio recording based on subscription status */
        if (b11_audio_enabled) {
            ESP_LOGI(TAG, "Starting audio recording...");
            // Set connection info first, then enable notifications
            audio_set_connection_info(event->subscribe.conn_handle, event->subscribe.attr_handle);
            audio_set_notify_enable(true);  // This will auto-start recording
        } else {
            ESP_LOGI(TAG, "Stopping audio recording...");
            audio_set_notify_enable(false);  // This will auto-stop recording
            
            /* Clear connection handle on unsubscribe to prevent stale data sends */
            b11_audio_conn_handle = 0;
        }
    } else if (event->subscribe.attr_handle == b11_nfc_chr_handle) {
        b11_nfc_conn_handle = event->subscribe.conn_handle;
        b11_nfc_notify_enabled = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "B11 NFC %s", b11_nfc_notify_enabled ? "enabled" : "disabled");
        
        /* 当APP订阅NFC通知时，检查是否有离线交换的新朋友名片 */
        if (b11_nfc_notify_enabled && nfc_storage_has_new_friend()) {
            ESP_LOGI(TAG, "🔔 Detected offline friend card, sending to APP...");
            
            nfc_card_t friend_card;
            if (nfc_storage_load_friend_card(&friend_card)) {
                // 构造通知消息
                uint8_t notify_data[1 + sizeof(nfc_card_t)];
                notify_data[0] = NFC_CMD_CARD_EXCHANGED;
                memcpy(&notify_data[1], &friend_card, sizeof(nfc_card_t));
                
                struct os_mbuf *om = ble_hs_mbuf_from_flat(notify_data, sizeof(notify_data));
                if (om != NULL) {
                    int rc = ble_gattc_notify_custom(b11_nfc_conn_handle, b11_nfc_chr_handle, om);
                    if (rc == 0) {
                        ESP_LOGI(TAG, "✅ Offline friend card synced: %.20s", friend_card.pet_name);
                        // 注意：不在这里清除标志，等APP主动读取后再清除
                    } else {
                        ESP_LOGE(TAG, "❌ Failed to send offline card: %d", rc);
                    }
                }
            }
        }
    } else if (event->subscribe.attr_handle == b11_voice_chr_handle) {
        b11_voice_conn_handle = event->subscribe.conn_handle;
        b11_voice_notify_enabled = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "B11 Voice %s", b11_voice_notify_enabled ? "enabled" : "disabled");
    } else if (event->subscribe.attr_handle == log_data_chr_handle) {
        log_data_conn_handle = event->subscribe.conn_handle;
        log_data_notify_enabled = event->subscribe.cur_notify;
        
        if (log_data_notify_enabled) {
            ESP_LOGI(TAG, "📝 Log streaming enabled");
            
            // Send welcome message immediately
            const char *welcome = "\n========================================\n"
                                 "📱 BLE Log Stream Connected!\n"
                                 "   Device: Blinko\n"
                                 "========================================\n\n";
            log_send_notification((const uint8_t *)welcome, strlen(welcome));
        } else {
            ESP_LOGI(TAG, "📝 Log streaming disabled");
        }
    }
}

/* ==================== LOG Service Implementation ==================== */

#include "log_collector.h"

/* Log control commands */
#define LOG_CMD_CLEAR       0x01  // Clear log buffer
#define LOG_CMD_SET_LEVEL   0x02  // Set log level (arg: 0-5)
#define LOG_CMD_STREAM_ON   0x03  // Enable real-time streaming
#define LOG_CMD_STREAM_OFF  0x04  // Disable real-time streaming
#define LOG_CMD_READ_START  0x05  // Start batch read from beginning

/**
 * @brief Log Size characteristic access (Read)
 * Returns current log buffer size (uint32_t)
 */
static int log_size_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint32_t size = log_collector_get_size();
        int rc = os_mbuf_append(ctxt->om, &size, sizeof(size));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief Log Data characteristic access (Read/Notify)
 * Reads log data in chunks for batch download
 */
static int log_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Check if we've reached the snapshot boundary
        if (log_snapshot_size > 0 && log_read_offset >= log_snapshot_size) {
            ESP_LOGD(TAG, "Log read: reached snapshot boundary (%zu bytes)", log_snapshot_size);
            return 0;  // Return empty data - download complete
        }
        
        // Read chunk of log data using snapshot state
        uint8_t chunk[512];
        size_t remaining = (log_snapshot_size > 0) ? (log_snapshot_size - log_read_offset) : SIZE_MAX;
        size_t to_read = (remaining < sizeof(chunk)) ? remaining : sizeof(chunk);
        
        // Use snapshot-based read to get frozen data
        size_t read = log_collector_read_snapshot(log_read_offset, chunk, to_read,
                                                   log_snapshot_size, log_snapshot_write_pos,
                                                   log_snapshot_wrapped);
        
        if (read > 0) {
            int rc = os_mbuf_append(ctxt->om, chunk, read);
            if (rc == 0) {
                log_read_offset += read;
                ESP_LOGD(TAG, "Log read: offset=%zu/%zu, len=%zu", log_read_offset, log_snapshot_size, read);
            }
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else {
            // No more data at current offset
            ESP_LOGD(TAG, "Log read: no more data at offset %zu (snapshot: %zu)", log_read_offset, log_snapshot_size);
            return 0;  // Return empty data
        }
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief Log Control characteristic access (Write)
 * Handles log commands
 */
static int log_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len < 1) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        
        uint8_t cmd;
        os_mbuf_copydata(ctxt->om, 0, sizeof(cmd), &cmd);
        
        switch (cmd) {
        case LOG_CMD_CLEAR:
            log_collector_clear();
            log_read_offset = 0;
            ESP_LOGI(TAG, "📝 Log cleared via BLE");
            break;
            
        case LOG_CMD_SET_LEVEL:
            if (ctxt->om->om_len >= 2) {
                uint8_t level;
                os_mbuf_copydata(ctxt->om, 1, sizeof(level), &level);
                log_collector_set_level((log_level_t)level);
                ESP_LOGI(TAG, "📝 Log level set to %d via BLE", level);
            }
            break;
            
        case LOG_CMD_STREAM_ON:
            log_collector_set_streaming(true, conn_handle, log_data_chr_handle);
            ESP_LOGI(TAG, "📝 Real-time log streaming enabled");
            break;
            
        case LOG_CMD_STREAM_OFF:
            log_collector_set_streaming(false, 0, 0);
            ESP_LOGI(TAG, "📝 Real-time log streaming disabled");
            break;
            
        case LOG_CMD_READ_START:
            log_read_offset = 0;
            log_snapshot_size = log_collector_get_size();
            log_collector_get_snapshot(&log_snapshot_write_pos, &log_snapshot_wrapped);
            ESP_LOGI(TAG, "📝 Log read started: size=%zu, write_pos=%zu, wrapped=%d", 
                     log_snapshot_size, log_snapshot_write_pos, log_snapshot_wrapped);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown log command: 0x%02x", cmd);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief Send log notification (for real-time streaming)
 * Called by log_collector when streaming is enabled
 */
int log_send_notification(const uint8_t *data, uint16_t len)
{
    if (!log_data_notify_enabled || log_data_conn_handle == 0) {
        return -1;
    }
    
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return -1;
    }
    
    int rc = ble_gattc_notify_custom(log_data_conn_handle, log_data_chr_handle, om);
    return rc;
}

/**
 * @brief Handle BLE disconnection - Reset all connection states
 */
void gatt_svr_disconnect_cb(void) {
    ESP_LOGI(TAG, "Cleaning up connection state...");
    
    /* Stop audio recording if active */
    if (b11_audio_enabled) {
        ESP_LOGI(TAG, "Force stopping audio recording due to disconnect");
        audio_set_notify_enable(false);
    }
    
    /* Stop log streaming if active */
    if (log_data_notify_enabled) {
        log_collector_set_streaming(false, 0, 0);
    }
    
    /* Reset all connection states */
    b11_notify_conn_handle = 0;
    b11_notify_enabled = false;
    b11_audio_conn_handle = 0;
    b11_audio_enabled = false;
    b11_nfc_conn_handle = 0;
    b11_nfc_notify_enabled = false;
    b11_voice_conn_handle = 0;
    b11_voice_notify_enabled = false;
    log_data_conn_handle = 0;
    log_data_notify_enabled = false;
    log_read_offset = 0;
    
    ESP_LOGI(TAG, "✓ Connection state cleaned up");
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) {
    /* Local variables */
    int rc;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services: %d", rc);
        return rc;
    }

    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return rc;
    }

    /* 4. 初始化情绪系统 */
    rc = emotion_manager_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize emotion manager: %d", rc);
        return rc;
    }
    
    rc = action_table_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize action table: %d", rc);
        return rc;
    }
    
    /* 5. 注册NFC名片交换回调 */
    rc = nfc_register_exchange_callback(nfc_card_exchanged_callback, NULL);
    if (rc == ESP_OK) {
        ESP_LOGI(TAG, "✓ NFC card exchange callback registered");
    } else {
        ESP_LOGW(TAG, "Failed to register NFC callback (NFC may not be initialized yet)");
    }
    
    /* 6. 初始化BLE OTA模块 */
    rc = ble_ota_init();
    if (rc == ESP_OK) {
        ESP_LOGI(TAG, "✓ BLE OTA module initialized");
    } else {
        ESP_LOGW(TAG, "BLE OTA initialization failed: %d", rc);
    }
    
    /* 7. 语音唤醒检测（现在由trigger_detector轮询GPIO44处理） */
    ESP_LOGI(TAG, "✓ Voice detection handled by trigger_detector (GPIO44 polling)");

    ESP_LOGI(TAG, "✓ GATT server initialized successfully");
    ESP_LOGI(TAG, "Emotion system initialized");
    return 0;
}

/* ==================== 自检服务实现 ==================== */

static void self_test_task(void *arg) {
    ESP_LOGI(TAG, "🔧 Starting device self-test...");
    
    self_test_report_t report = {0};
    self_test_run_all(&report);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║      📊 Self-Test Complete Report      ║");
    ESP_LOGI(TAG, "╠════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║  ✅ Passed:  %2d devices               ║", report.passed);
    ESP_LOGI(TAG, "║  ❌ Failed:  %2d devices               ║", report.failed);
    ESP_LOGI(TAG, "║  ⚠️  Warnings: %2d devices            ║", report.warnings);
    ESP_LOGI(TAG, "╚════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    vTaskDelete(NULL);
}

int gatt_svc_trigger_self_test(void) {
    BaseType_t ret = xTaskCreate(
        self_test_task,
        "self_test",
        8192,  // 8KB栈空间
        NULL,
        5,     // 中等优先级
        NULL
    );
    
    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
