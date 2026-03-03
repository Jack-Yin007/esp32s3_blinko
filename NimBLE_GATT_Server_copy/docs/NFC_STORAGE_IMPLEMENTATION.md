# NFC名片存储实现文档

## 📋 阶段1完成情况

### ✅ 已实现的文件

1. **`main/include/nfc_card.h`** - NFC名片数据结构定义
   - 简洁格式：140字节（固定大小）
   - Core Identity: 70字节（pet_name 20 + uuid 16 + reserved 34）
   - Social Profile: 70字节（personality 40 + catchphrase 30）
   - 无Metadata字段，结构简单直接

2. **`main/include/nfc_storage.h`** - 存储接口定义
   - 基于文件系统（不使用NVS）
   - 存储路径：`/data/nfc/`
   - 文件：my_card.bin, friend_card.bin, .friend_new

3. **`main/src/nfc_storage.c`** - 存储功能实现
   - 文件系统读写
   - 基础数据校验（检查pet_name非空）
   - 新朋友标志管理

4. **`main/test_nfc_storage.c`** - 测试程序
   - 测试保存/读取本机名片
   - 测试保存/读取朋友名片
   - 测试覆盖逻辑
   - 测试删除操作

5. **`main/CMakeLists.txt`** - 已更新
   - 添加 nfc_storage.c 编译配置

---

## 📊 数据结构详情

### NFC名片格式 (nfc_card_t)

```c
typedef struct __attribute__((packed)) {
    // Core Identity (70 bytes - FIXED)
    uint8_t pet_name[20];       // 宠物名称 (UTF-8)
    uint8_t uuid[16];           // 唯一标识符
    uint8_t core_reserved[34];  // 保留字段（填充0）
    
    // Social Profile (70 bytes)
    uint8_t personality[40];    // 性格特征："playful,loyal,shy,curious,brave"
    uint8_t catchphrase[30];    // 标语/口号
} nfc_card_t;
```

**总大小：140字节**（固定大小，简洁结构，无metadata）

---

## 🗂️ 文件系统布局

```
/data/                          ← 用户数据分区挂载点（由存储团队配置）
└── nfc/                        ← NFC存储目录
    ├── my_card.bin             ← 本机名片（140字节）
    ├── friend_card.bin         ← 朋友名片（140字节，只保存最新1条）
    └── .friend_new             ← 新朋友标志文件（文本"1"）
```

---

## 🔧 API使用说明

### 1. 初始化

```c
#include "nfc_storage.h"

// 必须在使用前调用（检查文件系统并创建目录）
if (!nfc_storage_init()) {
    ESP_LOGE(TAG, "File system not ready!");
    // 需要等待存储团队挂载 /data 分区
    return;
}
```

### 2. 保存本机名片

```c
nfc_card_t my_card;
nfc_card_init(&my_card);

// 设置名片内容
nfc_card_set_name(&my_card, "Blinko");
memcpy(my_card.uuid, my_uuid, 16);
nfc_card_set_personality(&my_card, "playful,loyal,curious");
nfc_card_set_catchphrase(&my_card, "Let's play!");

// 保存到文件系统
if (nfc_storage_save_my_card(&my_card)) {
    ESP_LOGI(TAG, "My card saved");
}
```

### 3. 读取本机名片

```c
nfc_card_t my_card;
if (nfc_storage_load_my_card(&my_card)) {
    ESP_LOGI(TAG, "My name: %.*s", NFC_CARD_PET_NAME_LEN, my_card.pet_name);
    // 自动验证 magic 和 CRC32
}
```

### 4. 保存朋友名片（NFC接收）

```c
nfc_card_t friend_card;
// ... 从NFC接收数据填充 friend_card ...

// 保存（会覆盖旧朋友，并设置"新朋友"标志）
if (nfc_storage_save_friend_card(&friend_card)) {
    ESP_LOGI(TAG, "Friend card saved");
    
    // 如果BLE已连接，立即通知APP
    if (ble_is_connected()) {
        send_friend_to_app(&friend_card);
    }
}
```

### 5. BLE重连时检查新朋友

```c
void on_ble_connected() {
    // APP重新连接时，检查是否有未读的朋友名片
    if (nfc_storage_has_new_friend()) {
        nfc_card_t friend_card;
        if (nfc_storage_load_friend_card(&friend_card)) {
            send_friend_to_app(&friend_card);
            nfc_storage_clear_new_friend_flag();
        }
    }
}
```

---

## ⚠️ 重要前提条件

### 存储团队需要提供：

1. **格式化并挂载用户数据分区**
   - 挂载点：`/data`
   - 文件系统：FAT32 或 LittleFS
   - 权限：可读写

2. **分区大小建议**
   - 最小：1MB（足够存储数千张名片）
   - 推荐：4MB（预留未来扩展）

3. **初始化顺序**
   ```c
   // 1. 存储团队的代码先执行
   storage_init();           // 格式化并挂载 /data
   
   // 2. NFC存储初始化
   nfc_storage_init();       // 创建 /data/nfc/ 目录
   ```

---

## 🧪 测试方法

### 方法1：使用测试程序

1. 修改 `main/CMakeLists.txt`：
   ```cmake
   set(MAIN_PROGRAM "test_nfc_storage.c")
   ```

2. 编译并烧录：
   ```bash
   idf.py build flash monitor
   ```

3. 查看测试输出：
   - ✅ 保存/读取测试
   - ✅ 数据校验测试
   - ✅ 覆盖逻辑测试
   - ✅ 删除操作测试

### 方法2：手动测试

```bash
# 连接串口
idf.py monitor

# 在ESP32上执行（通过shell命令或代码）
ls -la /data/nfc/
cat /data/nfc/my_card.bin | hexdump -C
```

---

## 🔄 与NFC协议集成（下一步）

阶段2将实现：

```c
// NFC碰触事件处理
void nfc_on_card_exchange(const uint8_t *nfc_data, size_t len) {
    nfc_card_t friend_card;
    
    // 1. 解析NFC帧，提取朋友名片
    if (nfc_parse_frame(nfc_data, len, &friend_card)) {
        
        // 2. 保存到文件系统
        nfc_storage_save_friend_card(&friend_card);
        
        // 3. 如果BLE已连接，立即通知APP
        if (ble_is_connected()) {
            ble_notify_friend_card(&friend_card);
        }
    }
}
```

---

## 📝 API参考快速索引

| 函数 | 功能 | 返回值 |
|------|------|--------|
| `nfc_storage_init()` | 初始化存储（创建目录） | bool |
| `nfc_storage_save_my_card()` | 保存本机名片 | bool |
| `nfc_storage_load_my_card()` | 读取本机名片 | bool |
| `nfc_storage_save_friend_card()` | 保存朋友名片 | bool |
| `nfc_storage_load_friend_card()` | 读取朋友名片 | bool |
| `nfc_storage_has_new_friend()` | 检查是否有新朋友 | bool |
| `nfc_storage_clear_new_friend_flag()` | 清除新朋友标志 | void |
| `nfc_storage_delete_my_card()` | 删除本机名片 | bool |
| `nfc_storage_delete_friend_card()` | 删除朋友名片 | bool |

---

## ✅ 阶段1完成清单

- [x] 定义新的140字节名片格式
- [x] 实现基于文件系统的存储
- [x] CRC32数据完整性校验
- [x] 新朋友标志管理
- [x] 测试程序
- [x] 文档说明

## 🔜 下一步（阶段2）

- [ ] NFC数据帧格式定义
- [ ] 序列化/反序列化
- [ ] 与现有NFC驱动集成
- [ ] 碰触检测和数据交换逻辑

---

**状态：阶段1已完成，等待存储团队挂载 `/data` 分区后可进行测试**
