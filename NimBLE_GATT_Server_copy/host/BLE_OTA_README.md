# BLE OTA 固件升级测试工具

基于ESP-IDF BLE OTA协议的Python测试脚本，用于通过蓝牙升级ESP32固件。

## 功能特点

- ✅ 完整实现ESP-IDF BLE OTA协议
- ✅ 支持自动设备扫描和连接
- ✅ 支持4KB扇区分块传输
- ✅ 自动CRC16校验
- ✅ 实时进度显示
- ✅ 错误重试机制
- ✅ 详细日志输出

## 协议说明

### BLE Service & Characteristics

| 特征 | UUID | 权限 | 说明 |
|------|------|------|------|
| OTA Service | 0x8018 | - | OTA服务 |
| RECV_FW_CHAR | 0x8020 | Write, Notify | 固件接收，返回ACK |
| PROGRESS_BAR_CHAR | 0x8021 | Read, Notify | 进度条通知 |
| COMMAND_CHAR | 0x8022 | Write, Notify | 命令下发与ACK |
| CUSTOMER_CHAR | 0x8023 | Write, Notify | 用户自定义数据 |

### OTA升级流程

1. **连接设备** - 扫描并连接到目标BLE设备
2. **启用通知** - 订阅ACK和进度通知
3. **发送START命令** - 发送0x0001命令，携带固件总大小
4. **分块传输** - 以4KB为单位传输固件数据
   - 每个扇区包含多个数据包
   - 最后一包包含CRC16校验
5. **等待ACK** - 每个扇区完成后等待设备确认
6. **发送END命令** - 发送0x0002命令完成升级
7. **设备重启** - 设备自动重启并运行新固件

### 命令包格式

```
[Command_ID(2)] [Payload(16)] [CRC16(2)]  // 总共20字节
```

**命令类型：**
- `0x0001`: START_OTA - Payload前4字节为固件大小（小端序）
- `0x0002`: END_OTA - Payload全部填充0
- `0x0003`: ACK - 设备返回的确认包

### 固件包格式

```
[Sector_Index(2)] [Packet_Seq(1)] [Payload(MTU_size-4)]
```

- `Sector_Index`: 扇区号，从0开始
- `Packet_Seq`: 
  - `0x00-0xFE`: 普通数据包序号
  - `0xFF`: 最后一包（包含CRC16）
- `Payload`: 
  - 普通包：固件数据
  - 最后一包：数据 + padding + CRC16(2字节)

### ACK状态码

| 状态码 | 说明 |
|--------|------|
| 0x0000 | 成功 |
| 0x0001 | CRC错误 |
| 0x0002 | 扇区索引错误 |
| 0x0003 | Payload长度错误 |

## 安装依赖

```bash
cd host
pip install -r requirements.txt
```

主要依赖：
- `bleak>=0.20.0` - BLE通信库
- `numpy>=1.21.0` - 数值计算（可选）

## 使用方法

### 1. 扫描BLE设备

```bash
# 方法1：使用示例脚本
python ble_ota_example.py scan

# 方法2：使用完整脚本
python ble_ota_tester.py --scan
```

### 2. 简单OTA升级（推荐）

编辑 `ble_ota_example.py` 配置设备名和固件路径：

```python
DEVICE_NAME = "Blinko"  # 你的设备名称
FIRMWARE_PATH = "../build/NimBLE_GATT_Server.bin"  # 固件路径
```

然后运行：

```bash
python ble_ota_example.py
```

### 3. 完整命令行OTA升级

```bash
# 通过设备名连接
python ble_ota_tester.py --name Blinko --firmware ../build/NimBLE_GATT_Server.bin

# 通过MAC地址连接
python ble_ota_tester.py --address AA:BB:CC:DD:EE:FF --firmware firmware.bin

# 查看帮助
python ble_ota_tester.py --help
```

## 示例输出

```
============================================================
BLE OTA 固件升级测试
============================================================

[步骤 1/4] 正在连接到设备: Blinko
2024-11-30 10:30:15 - INFO - Searching for device: Blinko
2024-11-30 10:30:18 - INFO - Found device at AA:BB:CC:DD:EE:FF
2024-11-30 10:30:19 - INFO - Connected successfully!
2024-11-30 10:30:19 - INFO - MTU size: 247
✅ 连接成功

[步骤 2/4] 启用BLE通知...
2024-11-30 10:30:20 - INFO - ✓ Enabled RECV_FW_CHAR notifications
2024-11-30 10:30:20 - INFO - ✓ Enabled COMMAND_CHAR notifications
2024-11-30 10:30:20 - INFO - ✓ Enabled PROGRESS_BAR_CHAR notifications
✅ 通知已启用

[步骤 3/4] 开始上传固件: ../build/NimBLE_GATT_Server.bin
2024-11-30 10:30:21 - INFO - Firmware: NimBLE_GATT_Server.bin
2024-11-30 10:30:21 - INFO - Size: 819200 bytes (800.00 KB)
2024-11-30 10:30:21 - INFO - Sectors: 200
2024-11-30 10:30:21 - INFO - Sending START_OTA command...
2024-11-30 10:30:22 - INFO - ✓ Command START_OTA ACK: ACCEPTED
2024-11-30 10:30:22 - INFO - Uploading sector 1/200...
2024-11-30 10:30:22 - INFO - Sector 0: 4096 bytes, CRC16=0x1234
2024-11-30 10:30:23 - INFO - ✓ Sector 0 ACK: SUCCESS
2024-11-30 10:30:23 - INFO - Progress: 0.5% | Speed: 45.23 KB/s
...
2024-11-30 10:31:40 - INFO - Uploading sector 200/200...
2024-11-30 10:31:41 - INFO - ✓ Sector 199 ACK: SUCCESS
2024-11-30 10:31:41 - INFO - Progress: 100.0% | Speed: 42.31 KB/s
2024-11-30 10:31:41 - INFO - Sending END_OTA command...
2024-11-30 10:31:42 - INFO - ✓ Command END_OTA ACK: ACCEPTED
2024-11-30 10:31:42 - INFO - ✓ Firmware uploaded successfully!
2024-11-30 10:31:42 - INFO - Total time: 80.45s | Average speed: 42.31 KB/s
2024-11-30 10:31:42 - INFO - Device will restart with new firmware...
✅ 固件上传成功！

[步骤 4/4] OTA升级完成
设备将重启并运行新固件...

已断开连接
```

## 文件说明

- `ble_ota_tester.py` - 完整的BLE OTA测试工具类
- `requirements.txt` - Python依赖包列表

## ESP32设备端配置

确保你的ESP32设备已经启用BLE OTA功能：

```c
// menuconfig配置
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y  // 或 CONFIG_BT_BLUEDROID_ENABLED=y

// 添加BLE OTA组件
#include "ble_ota.h"

// 初始化BLE OTA
esp_ble_ota_init();
esp_ble_ota_start();
```

## 故障排查

### 1. 找不到设备
- 确认设备已开机且BLE广播已启动
- 确认设备名称正确（区分大小写）
- 使用 `--scan` 参数扫描附近所有设备

### 2. 连接失败
- 检查蓝牙适配器是否正常
- Windows: 确保蓝牙服务已启动
- Linux: 确保有权限访问蓝牙 (`sudo usermod -a -G bluetooth $USER`)
- macOS: 授予Python蓝牙权限

### 3. CRC错误
- 固件文件可能损坏，重新编译
- 传输过程中断，重新上传
- MTU协商失败，检查设备端配置

### 4. 传输速度慢
- 增大MTU (默认23，最大可到247)
- 减少数据包间延迟（修改代码中的 `asyncio.sleep(0.01)`）
- 检查蓝牙信号强度

## 技术参考

- [ESP-IDF BLE OTA 示例](https://github.com/espressif/esp-iot-solution/tree/master/examples/bluetooth/ble_ota)
- [Bleak 文档](https://bleak.readthedocs.io/)
- [BLE GATT 协议](https://www.bluetooth.com/specifications/specs/gatt-specification-supplement/)
