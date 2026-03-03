# NFC Pet Card Storage - Implementation Guide

## 📋 Overview

NFC宠物名片存储模块实现了宠物身份卡片的持久化存储和交换功能。

### 新的名片格式 (118字节)

```
┌────────────────────────────────────┐
│ Core Identity (36 bytes)           │
│  - Pet Name: 20 bytes (UTF-8)      │
│  - UUID: 16 bytes                  │
├────────────────────────────────────┤
│ Social Profile (70 bytes)          │
│  - Personality: 40 bytes           │
│    (5 traits: "playful,loyal,...")│
│  - Catchphrase: 30 bytes           │
│    ("Always happy!")               │
├────────────────────────────────────┤
│ Metadata (12 bytes)                │
│  - Magic: 0x504554C0               │
│  - Timestamp: Unix time            │
│  - CRC32: Data checksum            │
└────────────────────────────────────┘
```

## 📁 File Structure

```
main/
├── include/drivers/
│   ├── nfc_card.h          # 名片数据结构定义
│   └── nfc_storage.h       # Flash存储接口
├── src/drivers/
│   └── nfc_storage.c       # NVS Flash实现
└── test_nfc_storage.c      # 单元测试
```

## 🚀 Quick Start

### 1. 初始化存储

```c
#include "nfc_storage.h"

void app_main(void) {
    // Initialize NVS flash
    if (!nfc_storage_init()) {
        ESP_LOGE(TAG, "Storage init failed");
        return;
    }
}
```

### 2. 保存本机名片 (来自APP)

```c
// 当APP通过BLE写入用户信息时
void on_user_info_received_from_app(const char *name, const uint8_t *uuid) {
    nfc_card_t my_card = NFC_CARD_INIT();
    
    // 填充名片信息
    snprintf((char*)my_card.pet_name, NFC_CARD_PET_NAME_LEN, "%s", name);
    memcpy(my_card.uuid, uuid, NFC_CARD_UUID_LEN);
    snprintf((char*)my_card.personality, NFC_CARD_PERSONALITY_LEN, 
             "playful,loyal,shy,brave,cute");
    snprintf((char*)my_card.catchphrase, NFC_CARD_CATCHPHRASE_LEN, 
             "Always happy!");
    
    // 保存到Flash (自动计算CRC和时间戳)
    if (nfc_storage_save_my_card(&my_card)) {
        ESP_LOGI(TAG, "My card saved successfully");
    }
}
```

### 3. NFC碰触发送本机名片

```c
// 在NFC主动模式任务中
void nfc_initiator_on_device_detected(void) {
    nfc_card_t my_card;
    
    // 从Flash读取本机名片
    if (!nfc_storage_load_my_card(&my_card)) {
        ESP_LOGW(TAG, "No card to send");
        return;
    }
    
    // 通过NFC发送名片
    nfc_send_data((uint8_t*)&my_card, sizeof(nfc_card_t));
}
```

### 4. NFC碰触接收对方名片

```c
// 在NFC被动模式任务中
void nfc_target_on_data_received(const uint8_t *data, size_t len) {
    if (len != sizeof(nfc_card_t)) {
        ESP_LOGW(TAG, "Invalid card size");
        return;
    }
    
    nfc_card_t *friend_card = (nfc_card_t*)data;
    
    // 保存朋友名片 (会自动设置new_friend标志)
    if (nfc_storage_save_friend_card(friend_card)) {
        ESP_LOGI(TAG, "Friend card saved: %s", friend_card->pet_name);
        
        // 如果BLE已连接，立即通知APP
        if (ble_is_connected()) {
            ble_notify_new_friend(friend_card);
        }
    }
}
```

### 5. BLE重连时检查新朋友

```c
// 在BLE连接建立后
void on_ble_connected(void) {
    // 检查是否有新朋友信息待上传
    if (nfc_storage_has_new_friend()) {
        nfc_card_t friend_card;
        
        if (nfc_storage_load_friend_card(&friend_card)) {
            // 延迟500ms确保连接稳定
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // 通知APP
            ble_notify_new_friend(&friend_card);
            
            // 清除标志
            nfc_storage_clear_new_friend_flag();
        }
    }
}
```

### 6. APP读取朋友名片

```c
// 当APP主动读取朋友信息时
void on_app_read_friend_request(void) {
    nfc_card_t friend_card;
    
    if (nfc_storage_load_friend_card(&friend_card)) {
        // 发送给APP
        ble_send_friend_info(&friend_card);
        
        // 清除新朋友标志
        nfc_storage_clear_new_friend_flag();
    } else {
        // 没有朋友信息
        ble_send_error("No friend card available");
    }
}
```

## 🧪 Testing

运行单元测试：

```c
#include "test_nfc_storage.c"

void app_main(void) {
    // Run all tests
    test_nfc_storage_run_all();
}
```

测试覆盖：
- ✅ 保存/读取本机名片
- ✅ 保存/读取朋友名片
- ✅ CRC数据完整性验证
- ✅ 朋友名片覆盖 (只保存最新1条)
- ✅ 新朋友标志操作

## 📊 Storage Layout

NVS Flash存储键：

| Key | Type | Description |
|-----|------|-------------|
| `my_card` | blob (118B) | 本机宠物名片 |
| `friend_card` | blob (118B) | 朋友宠物名片 |
| `friend_new` | uint8 | 新朋友标志 (0/1) |

## 🔒 Data Integrity

### CRC32校验

- 自动计算：保存时自动计算CRC32
- 自动验证：读取时自动验证CRC32
- 范围：覆盖整个结构体（除crc32字段本身）

### Magic Number

- 值：`0x504554C0` ("PET" + version 0xC0)
- 用途：快速验证数据有效性

## 🎯 API Reference

### Initialization

```c
bool nfc_storage_init(void);
```

### My Card

```c
bool nfc_storage_save_my_card(nfc_card_t *card);
bool nfc_storage_load_my_card(nfc_card_t *card);
bool nfc_storage_has_my_card(void);
```

### Friend Card

```c
bool nfc_storage_save_friend_card(const nfc_card_t *card);
bool nfc_storage_load_friend_card(nfc_card_t *card);
bool nfc_storage_has_friend_card(void);
```

### New Friend Flag

```c
bool nfc_storage_has_new_friend(void);
bool nfc_storage_clear_new_friend_flag(void);
```

### Utilities

```c
bool nfc_storage_erase_all(void);
uint32_t nfc_storage_calculate_crc(const nfc_card_t *card);
bool nfc_storage_verify_card(const nfc_card_t *card);
```

## 📝 Example Card Data

```c
nfc_card_t example_card = {
    .pet_name = "Max",
    .uuid = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
             0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88},
    .personality = "playful,loyal,shy,brave,cute",
    .catchphrase = "Always happy!",
    .magic = 0x504554C0,
    .timestamp = 1700000000,
    .crc32 = 0  // Will be calculated automatically
};
```

## 🚨 Error Handling

所有API返回`bool`：
- `true`: 操作成功
- `false`: 操作失败（检查日志获取详细错误）

常见错误：
- `ESP_ERR_NVS_NOT_INITIALIZED`: 未调用`nfc_storage_init()`
- `ESP_ERR_NVS_NOT_FOUND`: 未找到存储的数据
- CRC mismatch: 数据损坏
- Invalid magic: 数据格式错误

## 📈 Next Steps

阶段1完成！继续实现：

- **阶段2**: NFC协议层（数据帧格式）
- **阶段3**: NFC数据交换逻辑
- **阶段4**: BLE接口扩展
- **阶段5**: 集成测试

## 💡 Tips

1. **断电保护**: NVS数据自动持久化，断电不丢失
2. **线程安全**: 所有API内部使用NVS互斥锁保护
3. **性能**: 读写操作耗时约10-50ms
4. **容量**: 每个名片118字节，NVS分区通常有几KB空间
