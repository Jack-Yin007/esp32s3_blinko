# GATT通知监听工具使用指南

## 📋 功能概述

`gatt_notification_monitor.py` 是一个通用的BLE通知监听工具，用于测试ESP32主动推送的各种事件：

### ✅ 已支持的通知类型

1. **动作事件** (0x12)
   - 动作完成通知
   - 自动解析动作ID

2. **NFC名片交换** (0x30)
   - 实时接收NFC交换的名片
   - 自动解析姓名、电话等信息

3. **心率数据** (0x40)
   - 实时心率监测
   - 信号质量指示

4. **触摸事件** (0x50)
   - 单击、双击、长按、滑动
   - 实时触摸反馈

5. **电量变化** (0x60)
   - 电池电量变化通知
   - 充电状态

6. **音频数据** (Audio特征)
   - 实时音频流
   - ADPCM/PCM数据

7. **OTA进度** (OTA服务)
   - 固件升级进度
   - ACK确认

## 🚀 快速开始

### 1. 扫描设备
```bash
python gatt_notification_monitor.py --scan
```

### 2. 监听所有通知（推荐）
```bash
# 通过设备名连接
python gatt_notification_monitor.py --name Blinko

# 通过MAC地址连接
python gatt_notification_monitor.py --address AA:BB:CC:DD:EE:FF
```

### 3. 列出所有服务和特征
```bash
python gatt_notification_monitor.py --name Blinko --list
```

### 4. 定时监听
```bash
# 监听60秒后自动停止
python gatt_notification_monitor.py --name Blinko --duration 60
```

### 5. 只监听特定服务
```bash
# 只监听B11主服务
python gatt_notification_monitor.py --name Blinko \
  --service 6e400001-b5a3-f393-e0a9-e50e24dcca9e

# 监听多个服务
python gatt_notification_monitor.py --name Blinko \
  --service 6e400001-b5a3-f393-e0a9-e50e24dcca9e \
  --service 0000180f-0000-1000-8000-00805f9b34fb
```

## 📊 示例输出

### 动作完成通知
```
[14:30:25.123] B11 TX (设备→手机):
  🎬 动作完成通知: 动作ID=5
```

### NFC名片交换
```
[14:30:30.456] B11 NFC:
  👥 NFC交换名片通知:
    姓名: 张三
    电话: +8613800138000
```

### 心率数据
```
[14:30:35.789] B11 TX (设备→手机):
  ❤️  心率数据通知: 72 bpm (质量: 95)
```

### 触摸事件
```
[14:30:40.012] B11 TX (设备→手机):
  👆 触摸事件通知: 双击
```

### 电量变化
```
[14:30:45.345] B11 TX (设备→手机):
  🔋 电量变化通知: 85%
```

### 监听结束统计
```
======================================================================
📊 通知统计:
======================================================================
   总通知数: 156
   持续时间: 60.12秒
   平均速率: 2.60 通知/秒

   各特征通知数:
     B11 Audio: 120
     B11 TX (设备→手机): 25
     B11 NFC: 8
     Battery Level: 3
======================================================================
```

## 🧪 测试场景

### 场景1: 测试动作触发通知
1. 启动监听器：
   ```bash
   python gatt_notification_monitor.py --name Blinko
   ```

2. 在另一个终端触发动作（或使用手机APP）

3. 观察监听器输出的动作完成通知

### 场景2: 测试NFC名片交换
1. 启动监听器：
   ```bash
   python gatt_notification_monitor.py --name Blinko
   ```

2. 用ESP32设备触碰另一个NFC设备

3. 观察实时接收到的名片信息

### 场景3: 测试心率监测
1. 启动监听器：
   ```bash
   python gatt_notification_monitor.py --name Blinko
   ```

2. 手指放在心率传感器上

3. 观察实时心率数据流

### 场景4: 测试触摸事件
1. 启动监听器：
   ```bash
   python gatt_notification_monitor.py --name Blinko
   ```

2. 单击/双击/长按触摸传感器

3. 观察触摸事件通知

## 🔧 高级用法

### 自定义解析器

如果需要解析自定义协议的通知，可以注册解析器：

```python
from gatt_notification_monitor import GATTNotificationMonitor

# 自定义解析函数
def my_parser(sender, data):
    # 解析逻辑
    return f"自定义数据: {data.hex()}"

# 创建监听器
monitor = GATTNotificationMonitor(device_name="Blinko")

# 注册解析器
monitor.register_parser(
    "6e400002-b5a3-f393-e0a9-e50e24dcca9e",  # 特征UUID
    my_parser
)

# 连接并监听
await monitor.connect()
await monitor.subscribe_all_notifications()
await monitor.monitor()
```

### Python脚本集成

```python
import asyncio
from gatt_notification_monitor import GATTNotificationMonitor

async def test_notifications():
    monitor = GATTNotificationMonitor(device_name="Blinko")
    
    try:
        # 连接
        if not await monitor.connect():
            print("连接失败")
            return
        
        # 订阅所有通知
        await monitor.subscribe_all_notifications()
        
        # 监听10秒
        await monitor.monitor(duration=10)
        
    finally:
        await monitor.disconnect()

if __name__ == '__main__':
    asyncio.run(test_notifications())
```

## 📝 命令行参数

| 参数 | 说明 | 示例 |
|------|------|------|
| `--name` | 设备名称 | `--name Blinko` |
| `--address` | 设备MAC地址 | `--address AA:BB:CC:DD:EE:FF` |
| `--scan` | 扫描设备 | `--scan` |
| `--list` | 列出服务和特征 | `--list` |
| `--duration` | 监听时长（秒） | `--duration 60` |
| `--service` | 过滤服务UUID | `--service <UUID>` |

## 🆚 与现有脚本对比

| 功能 | nfc_ble_manager.py | ble_audio_recorder_8khz.py | gatt_notification_monitor.py |
|------|-------------------|---------------------------|----------------------------|
| NFC通知 | ✅ | ❌ | ✅ |
| 音频通知 | ❌ | ✅ | ✅ |
| 动作通知 | ❌ | ❌ | ✅ |
| 心率通知 | ❌ | ❌ | ✅ |
| 触摸通知 | ❌ | ❌ | ✅ |
| 电量通知 | ❌ | ❌ | ✅ |
| 通用监听 | ❌ | ❌ | ✅ |
| 协议解析 | ⚠️ 仅NFC | ⚠️ 仅音频 | ✅ 全部 |

## 🎯 建议使用场景

### 使用 `gatt_notification_monitor.py` 当你需要：
- ✅ 测试**所有类型**的通知
- ✅ 不确定会收到什么通知
- ✅ 调试新功能
- ✅ 查看实时事件流
- ✅ 统计通知频率

### 使用 `nfc_ble_manager.py` 当你需要：
- ✅ 专门测试NFC功能
- ✅ 设置/读取名片
- ✅ NFC业务逻辑测试

### 使用 `ble_audio_recorder_8khz.py` 当你需要：
- ✅ 录制音频
- ✅ 保存WAV文件
- ✅ 音频质量测试

## 💡 提示

1. **首次使用建议加 `--list` 参数**，查看设备支持哪些通知
2. **长时间监听时注意电池电量**
3. **Ctrl+C 随时停止监听**并查看统计
4. **可以同时运行多个监听器**监听不同设备
5. **Windows可能需要授予蓝牙权限**

## 🐛 故障排查

### 收不到通知
- 检查特征是否支持notify/indicate（使用 `--list` 查看）
- 确认设备端已启用通知发送
- 检查CCCD是否正确写入

### 连接失败
- 确认设备名称/地址正确
- 设备可能已连接到其他客户端
- 重启设备和蓝牙适配器

### 解析错误
- 检查协议版本是否匹配
- 使用十六进制查看原始数据
- 考虑注册自定义解析器

## 📚 相关文档

- [B11协议文档](B11_Protocol.md)
- [BLE音频协议](audio_section_adpcm.md)
- [NFC使用指南](nfc_ble_manager.py)
