# B11通信协议规范 v2.4
**ESP32宠物机器人 ↔ 手机APP 通信协议**

---

## 📋 文档信息

| 项目 | 信息 |
|------|------|
| **协议版本** | v2.4 |
| **更新日期** | 2025年12月3日 |
| **适用固件** | ESP32-S3 NimBLE GATT Server |
| **编码格式** | 小端序（Little Endian） |
| **音频格式** | IMA ADPCM 4-bit (4:1压缩比) |

---

## 🔌 BLE连接参数

### **设备信息**
- **设备名称**: `Blinko`
- **传输方式**: BLE GATT
- **连接间隔**: 7.5ms (最优化)
- **MTU大小**: 244字节有效载荷

### **服务发现**
```
扫描目标: 设备名 = "Blinko"
MAC地址: 90:E5:B1:AE:E4:56 (示例)
服务数量: 4个标准服务 + 1个B11自定义服务
```

---

## 🏗️ GATT服务架构

### **B11主服务 (Primary Service)**
```
服务UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
特征值数量: 5个
服务类型: PRIMARY
```

### **特征值定义**

| 序号 | 特征值名称 | UUID (末4位) | 权限 | 说明 |
|------|-----------|-------------|------|------|
| 1 | **Control** | 0002 | Write | 手机→设备控制命令 |
| 2 | **Notify** | 0003 | Notify | 设备→手机状态通知 |
| 3 | **Audio** | 0004 | Notify | 音频流传输 (ADPCM) |
| 4 | **NFC** | 0006 | Read/Write/Notify | NFC名片管理 |
| 5 | **Voice** | 0007 | Notify | 语音唤醒通知 |

---

## 📤 Control特征值 (手机→设备)

**UUID**: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`  
**权限**: Write + Write_No_Response  
**用途**: 发送控制命令到设备

### **命令格式**
```
[命令字 1字节] [参数 N字节]
```

### **命令定义**

#### **1. 情绪区间切换 (0x10)**
```
发送: [0x10] [区间号]
功能: 切换宠物情绪区间
参数: 
  - 0x00: S区间 (睡眠)
  - 0x01: A区间 (愤怒)  
  - 0x02: B区间 (悲伤)
  - 0x03: C区间 (日常)
  - 0x04: D区间 (开心)
响应: 无直接响应
状态保存: 自动保存到NVS持久化存储
动作表: 自动加载对应区间的动作组合
```

#### **2. 录音控制 (0x40)**
```
发送: [0x40] [开关]
功能: 控制INMP441麦克风录音
参数:
  - 0x00: 停止录音
  - 0x01: 开始录音
音频流: 自动通过Audio特征值传输 (IMA ADPCM压缩格式)
压缩比: 4:1 (16-bit PCM → 4-bit ADPCM)
数据率: ~4 KB/s (压缩后)
```

#### **3. 播放测试命令 (0x70)**
```
发送: [0x70] [WAV_ID]
功能: 播放指定WAV文件并同时录音(测试用)
参数: WAV_ID (1-23)
用途: 测试音频播放和录音同步功能
响应: 如果命令无效，通过Notify返回错误码0x60 0x01
```

#### **4. 充电器控制 (0x90)**
```
发送: [0x90] [控制字]
功能: 控制充电器开关
参数:
  - 0x00: 恢复充电 (RESUME)
  - 0x01: 关闭充电 (SHUTDOWN)
响应: 如果参数无效，返回错误码0x60 0x01
```

---

## 📥 Notify特征值 (设备→手机)

**UUID**: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`  
**权限**: Notify  
**用途**: 设备状态变化通知

### **通知格式**
```
[命令字 1字节] [数据 N字节]
```

### **通知类型**

#### **1. 触发事件上报 (0x20)**
```
格式: [0x20] [触发类型] [动作组合ID]
触发类型:
  - 0x01: 触摸触发 (TTP223触摸传感器)
  - 0x02: 接近触发 (PIR人体红外传感器)
  - 0x03: 声音触发 (语音关键词检测)
动作组合ID: 执行的动作组合编号 (0x00-0xFF)
时机: 当触发器检测到事件并执行动作时自动上报
示例: [0x20] [0x01] [0x1E] = 触摸触发，执行动作组合30号
```

#### **2. 传感器数据 (0x30)**
```
格式: [0x30] [数据类型] [数值]
数据类型:
  - 0x01: 心率数据 (模拟)
  - 0x02: 电池电量
  - 0x03: 温度传感器
数值: 8位无符号整数 (0-255)
注意: 当前版本仅支持单字节数值
```

#### **3. 设备状态 (0x50)**
```
格式: [0x50] [状态类型] [状态值]
状态类型:
  - 0x00-0x04: 电池相关状态
  - 0x07-0x08: 充电状态
状态值: 8位无符号整数
用途: 上报设备运行状态变化
```

#### **4. 错误报告 (0x60)**
```
格式: [0x60] [错误码]
错误码:
  - 0x01: 无效命令 (CMD_INVALID)
  - 0x02: 传感器故障 (SENSOR_FAULT)
  - 0x03: 电量不足 (LOW_BATTERY)
触发: 当收到无效控制命令或设备出现异常时上报
```

---

## 🎵 Audio特征值 (音频流)

**UUID**: `6E400004-B5A3-F393-E0A9-E50E24DCCA9E`  
**权限**: Notify  
**用途**: PCM音频流传输

### **音频规格**
- **采样率**: 8000 Hz
- **位深**: 16-bit PCM → 4-bit ADPCM
- **声道**: 单声道 (Mono)
- **编码**: IMA ADPCM (4:1 压缩比)
- **数据率**: ~4 KB/s (压缩后)

### **数据包格式**
```
帧头 (4字节):
  [0-1]: 样本数 (uint16_t, big-endian)
  [2-3]: 原始PCM字节数 (uint16_t, big-endian)

ADPCM数据 (N字节):
  [4..N]: IMA ADPCM压缩数据 (每字节包含2个4-bit样本)
```

### **重要说明**
**✅ 当前版本使用 IMA ADPCM 压缩**

固件实现（`AUDIO_USE_ADPCM=1`）：
- 16-bit PCM采样后立即压缩为 4-bit ADPCM
- 压缩比 4:1，大幅降低BLE带宽需求
- 每个BLE通知包含：4字节header + ADPCM数据
- 数据长度根据 MTU 自动分包（典型 244 字节/包）

### **数据接收流程**
1. **启用通知**: 订阅Audio特征值
2. **接收ADPCM**: 接收压缩数据包（header + ADPCM）
3. **解码**: 使用IMA ADPCM解码器还原为16-bit PCM
4. **WAV保存**: 以8kHz/16bit/Mono参数保存
5. **播放**: 使用音频播放器播放解码后的PCM数据

### **Python接收示例**
```python
import wave
import struct
from host.ble_audio_recorder_8khz import ADPCMDecoder  # 使用项目提供的解码器

# 全局缓冲区
adpcm_buffer = bytearray()
pcm_buffer = []
adpcm_decoder = ADPCMDecoder()

def audio_notification_handler(sender, data):
    """BLE音频通知处理"""
    # 累积ADPCM数据
    adpcm_buffer.extend(data)
    
    # 解析header（首个包的前4字节）
    if len(adpcm_buffer) >= 4:
        sample_count = struct.unpack('>H', adpcm_buffer[0:2])[0]
        pcm_bytes = struct.unpack('>H', adpcm_buffer[2:4])[0]
        
        # 解码ADPCM为PCM
        for byte in adpcm_buffer[4:]:
            high_nibble = (byte >> 4) & 0x0F
            low_nibble = byte & 0x0F
            pcm_buffer.append(adpcm_decoder.decode_sample(high_nibble))
            pcm_buffer.append(adpcm_decoder.decode_sample(low_nibble))
    
    print(f"Received {len(data)} bytes, decoded: {len(pcm_buffer)} samples")

# 保存WAV文件
def save_wav(filename, sample_rate=8000):
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        # 将PCM样本转换为字节
        pcm_bytes = struct.pack(f'<{len(pcm_buffer)}h', *pcm_buffer)
        wav_file.writeframes(pcm_bytes)
    print(f"Saved {len(pcm_buffer)} samples to {filename}")
```

### **性能参数**
- **BLE有效吞吐量**: ~22.75 KB/s
- **音频数据率（未压缩）**: 16 KB/s (8kHz × 2 bytes)
- **音频数据率（ADPCM）**: ~4 KB/s (4:1压缩比)
- **带宽利用率**: ~17.6% (4/22.75)
- **传输效率**: 实时传输无压力
- **延迟**: <100ms（取决于BLE连接间隔）

---

## 💳 NFC特征值 (名片管理)

**UUID**: `6E400006-B5A3-F393-E0A9-E50E24DCCA9E`  
**权限**: Read + Write + Notify  
**用途**: 宠物NFC名片交换管理

### **名片数据结构 (140字节)**
```c
typedef struct {
    // === Core Identity (70字节) ===
    uint8_t pet_name[20];        // 宠物名称 (UTF-8)
    uint8_t uuid[16];            // 唯一标识符 (UUID/Hash)
    uint8_t core_reserved[34];   // 预留空间 (填充零)
    
    // === Social Profile (70字节) ===
    uint8_t personality[40];     // 性格特征 ("playful,loyal,shy,curious,brave")
    uint8_t catchphrase[30];     // 座右铭/格言
} nfc_card_t;  // 总计140字节
```

**字段说明:**
- **pet_name**: 宠物名称，最多19个字符+结束符
- **uuid**: 16字节唯一标识符，用于防重复交换
- **core_reserved**: 34字节预留空间，填充零值
- **personality**: 40字节性格描述，推荐格式："trait1,trait2,trait3"
- **catchphrase**: 30字节座右铭，宠物的个性化短语

### **NFC命令**

#### **1. 设置我的名片 (0x20)**
```
发送: [0x20] [140字节名片数据]
数据格式: 
  - 总长度: 141字节 (1字节命令 + 140字节数据)
  - [20字节宠物名] [16字节UUID] [34字节预留] [40字节性格] [30字节座右铭]
响应: 
  - [0x00] = 成功 (NFC_STATUS_OK)
  - [0x01] = 失败 (NFC_STATUS_ERROR)
存储: 自动保存到Flash持久化存储 (/flash/nfc/my_card.bin)
示例: "小白" + UUID + 零填充 + "playful,loyal,smart" + "Life is an adventure!"
```

#### **2. 读取我的名片 (0x21)**
```
发送: [0x21]
响应: 
  - [0x00] [140字节名片数据] = 成功
  - [0x01] = 无名片数据 (NFC_STATUS_ERROR)
数据解析: 
  - 字节0: 状态码
  - 字节1-20: 宠物名
  - 字节21-36: UUID
  - 字节37-70: 预留空间
  - 字节71-110: 性格特征
  - 字节111-140: 座右铭
```

#### **3. 读取好友名片 (0x22)**
```
发送: [0x22]  
响应: 
  - [0x00] [140字节好友名片] = 有新好友 (NFC_STATUS_OK)
  - [0x02] = 无好友数据 (NFC_STATUS_NO_FRIEND)
功能: 读取最新接收的好友名片
副作用: 读取后自动清除"新好友"标志
存储: 从Flash读取 (/flash/nfc/friend_card.bin)
注意: 好友名片格式与我的名片相同（140字节完整结构）
```

#### **4. 获取好友数量 (0x23)**
```
发送: [0x23]
响应: 
  - [0x00] [好友数量] = 成功
    - 好友数量为1字节: 0或1 (当前版本仅支持1个好友槽位)
示例响应: [0x00] [0x01] = 有1个新好友
         [0x00] [0x00] = 无新好友
```

#### **5. 名片交换通知 (0x30)**
```
格式: [0x30] [140字节好友名片]
触发条件: NFC硬件检测到名片交换（物理NFC通信完成）
数据内容: 完整的好友名片数据（140字节）
通知时机: 实时通知，无需手机查询
存储: 名片已在indatachange.c中自动保存到Flash
用途: 让手机APP实时知道有新好友，可立即读取显示
```

### **NFC状态码**
```c
#define NFC_STATUS_OK         0x00  // 操作成功
#define NFC_STATUS_ERROR      0x01  // 操作失败/无数据
#define NFC_STATUS_NO_FRIEND  0x02  // 无好友名片
```

### **Flash存储路径**
```
/flash/nfc/my_card.bin      - 我的名片 (140字节)
/flash/nfc/friend_card.bin  - 好友名片 (140字节)
/flash/nfc/.friend_new      - 新好友标志文件
```

---

## 🎤 Voice特征值 (语音唤醒)

**UUID**: `6E400007-B5A3-F393-E0A9-E50E24DCCA9E`  
**权限**: Notify  
**用途**: 语音关键词检测通知

### **功能说明**
**⚠️ 当前版本：语音检测已集成到触发检测器**

语音唤醒功能通过GPIO44轮询检测实现，触发事件通过**Notify特征值 (0x20命令)** 上报，不使用独立的Voice特征值通知。

### **实际工作流程**
1. **硬件检测**: GPIO44低电平 = 检测到关键词
2. **触发处理**: trigger_detector轮询GPIO44状态
3. **事件上报**: 通过Notify特征值发送 `[0x20] [0x03] [动作ID]`
   - 0x20 = 触发事件命令
   - 0x03 = 语音触发类型
   - 动作ID = 执行的动作组合编号

### **开发者注意事项**
- Voice特征值保留用于兼容性，但不发送通知
- 语音事件请监听Notify特征值 (UUID: 6E400003)
- 触发类型 0x03 表示语音关键词检测
- 防抖时间: 200ms

### **示例：监听语音事件**
```python
def notification_handler(sender, data):
    if len(data) >= 3 and data[0] == 0x20:
        trigger_type = data[1]
        action_id = data[2]
        
        if trigger_type == 0x03:
            print(f"语音唤醒检测! 执行动作: {action_id}")
```

---

## 🔧 开发指南

### **Android BLE客户端实现**

#### **1. 服务发现**
```java
// 扫描目标设备
BluetoothLeScanner scanner = bluetoothAdapter.getBluetoothLeScanner();
ScanFilter filter = new ScanFilter.Builder()
    .setDeviceName("Blinko")
    .build();
scanner.startScan(Arrays.asList(filter), scanSettings, scanCallback);

// 连接GATT服务
BluetoothGatt gatt = device.connectGatt(context, false, gattCallback);
```

#### **2. 特征值操作**
```java
// B11服务UUID
UUID B11_SERVICE = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
UUID CONTROL_CHAR = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
UUID NOTIFY_CHAR = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");
UUID AUDIO_CHAR = UUID.fromString("6E400004-B5A3-F393-E0A9-E50E24DCCA9E");

// 发送控制命令
BluetoothGattCharacteristic controlChar = service.getCharacteristic(CONTROL_CHAR);
byte[] command = {0x10, 0x03}; // 切换到C区间
controlChar.setValue(command);
gatt.writeCharacteristic(controlChar);

// 订阅通知
BluetoothGattCharacteristic notifyChar = service.getCharacteristic(NOTIFY_CHAR);
gatt.setCharacteristicNotification(notifyChar, true);
```

#### **3. 音频流处理**
```java
// PCM音频直接接收（无压缩）
public class PCMAudioReceiver {
    private ByteArrayOutputStream audioBuffer = new ByteArrayOutputStream();
    
    public void handleAudioData(byte[] data) {
        // 直接累积PCM数据
        audioBuffer.write(data, 0, data.length);
        
        // 实时播放或保存
        playOrSavePCM(data);
    }
    
    public void saveToWav(String filename) throws IOException {
        // WAV文件头 (8kHz, 16-bit, Mono)
        byte[] header = createWavHeader(audioBuffer.size(), 8000, 1, 16);
        
        FileOutputStream fos = new FileOutputStream(filename);
        fos.write(header);
        fos.write(audioBuffer.toByteArray());
        fos.close();
    }
    
    private byte[] createWavHeader(int dataSize, int sampleRate, 
                                   int channels, int bitsPerSample) {
        byte[] header = new byte[44];
        // RIFF header
        header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
        int fileSize = dataSize + 36;
        header[4] = (byte)(fileSize & 0xFF);
        header[5] = (byte)((fileSize >> 8) & 0xFF);
        header[6] = (byte)((fileSize >> 16) & 0xFF);
        header[7] = (byte)((fileSize >> 24) & 0xFF);
        // WAVE format
        header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
        // fmt subchunk
        header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
        header[16] = 16; // Subchunk1Size (16 for PCM)
        header[20] = 1;  // AudioFormat (1 = PCM)
        header[22] = (byte)channels;
        header[24] = (byte)(sampleRate & 0xFF);
        header[25] = (byte)((sampleRate >> 8) & 0xFF);
        header[26] = (byte)((sampleRate >> 16) & 0xFF);
        header[27] = (byte)((sampleRate >> 24) & 0xFF);
        int byteRate = sampleRate * channels * bitsPerSample / 8;
        header[28] = (byte)(byteRate & 0xFF);
        header[29] = (byte)((byteRate >> 8) & 0xFF);
        header[30] = (byte)((byteRate >> 16) & 0xFF);
        header[31] = (byte)((byteRate >> 24) & 0xFF);
        header[32] = (byte)(channels * bitsPerSample / 8); // BlockAlign
        header[34] = (byte)bitsPerSample;
        // data subchunk
        header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
        header[40] = (byte)(dataSize & 0xFF);
        header[41] = (byte)((dataSize >> 8) & 0xFF);
        header[42] = (byte)((dataSize >> 16) & 0xFF);
        header[43] = (byte)((dataSize >> 24) & 0xFF);
        return header;
    }
}

// 音频通知处理
@Override
public void onCharacteristicChanged(BluetoothGatt gatt, 
                                   BluetoothGattCharacteristic characteristic) {
    if (AUDIO_CHAR.equals(characteristic.getUuid())) {
        byte[] pcmData = characteristic.getValue();
        audioReceiver.handleAudioData(pcmData);
    }
}
```

---

## ⚡ 性能参数

### **数据传输效率**
- **PCM音频传输**: 实时传输 (8kHz × 2 bytes = 16 KB/s)
- **BLE吞吐量**: ~22.75 KB/s 可用带宽
- **连接间隔**: 7.5ms (低延迟)
- **MTU大小**: 244字节有效载荷

### **音频性能**
- **录音格式**: 8kHz/16-bit/Mono PCM
- **传输格式**: 原始PCM (无压缩)
- **数据率**: 16 KB/s
- **带宽利用率**: 70.3% (16/22.75)
- **延迟**: <100ms

### **电源管理**
- **录音功耗**: ~45mA (@3.3V)
- **待机功耗**: ~12mA (BLE连接保持)
- **深度睡眠**: <1mA (断开BLE)

### **Flash存储**
- **分区表**:
  - NVS (0x9000, 16KB) - BLE配置
  - OTA Data (0xD000, 8KB) - OTA状态
  - PHY Init (0xF000, 4KB) - RF校准
  - Factory (0x10000, 1MB) - 主固件
  - OTA_0 (0x110000, 1MB) - OTA分区1
  - OTA_1 (0x210000, 1MB) - OTA分区2
  - Flash FAT (0x310000, 4MB) - 音频文件
- **音频存储**: /flash/audio/*.wav (23个WAV文件)
- **NFC存储**: /data/nfc_*.dat (名片数据)

---

## 🐛 故障排除

### **常见问题**

#### **1. 连接失败**
- 检查设备名称是否为"Blinko"
- 确认BLE未被其他应用占用
- 重启ESP32设备重新广播
- 验证BLE服务UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E

#### **2. 音频流中断**
- 确认Audio特征值通知已启用
- 检查BLE连接稳定性 (连接间隔7.5ms)
- 音频数据为ADPCM压缩格式，需要IMA ADPCM解码器
- 使用项目提供的`ble_audio_recorder_8khz.py`录制和解码

#### **3. NFC功能异常**
- 确认NFC特征值通知已启用
- 检查名片数据格式（140字节完整结构）
- 验证UTF-8编码兼容性
- 检查Flash分区是否挂载 (/data目录)

#### **4. 语音唤醒不响应**
- 语音事件通过Notify特征值上报 (0x20命令)
- 触发类型为0x03表示语音检测
- 检查GPIO44硬件连接
- 确认Notify特征值已订阅

#### **5. 控制命令无响应**
- 检查命令格式: [命令字][参数...]
- 验证参数有效性 (区间号0x00-0x04, 录音开关0x00-0x01)
- 无效命令会收到错误报告: [0x60][错误码]
- 情绪区间切换无直接响应，通过触发事件间接确认

---

## 📜 版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| **v2.4** | 2025-12-03 | **修正音频编码说明** |
|          |            | - 确认当前固件使用 IMA ADPCM 压缩（`AUDIO_USE_ADPCM=1`） |
|          |            | - 修正v2.1中"不使用ADPCM"的错误描述 |
|          |            | - 更新音频数据包格式（header + ADPCM数据） |
|          |            | - 添加ADPCM解码Python示例 |
|          |            | - 更新数据率为4KB/s（压缩后） |
| **v2.3** | 2025-12-03 | 添加完整的BLE OTA升级文档 |
|          |            | - 详细的OTA服务定义和特征值说明 |
|          |            | - OTA数据包格式和CRC16校验 |
|          |            | - 状态码和错误处理 |
| **v2.2** | 2025-11-30 | 添加Python测试工具文档 |
|          |            | - 新增通用GATT通知监听器文档 (gatt_notification_monitor.py) |
|          |            | - 添加工具选择指南和使用场景 |
|          |            | - 完善各工具的使用方法和输出示例 |
|          |            | - 更新开发者备注包含工具更新说明 |
| **v2.1** | 2025-11-26 | 根据实际代码更新协议文档 |
|          |            | - ⚠️ 错误：文档误报"移除ADPCM"，实际固件一直使用ADPCM |
|          |            | - 添加播放测试命令 (0x70) |
|          |            | - 添加充电器控制命令 (0x90) |
|          |            | - 修正触发事件格式 (动作ID代替区间号) |
|          |            | - 更新NFC命令详细说明和Flash存储路径 |
|          |            | - 语音检测集成到触发检测器 |
|          |            | - 添加Flash分区表信息 |
| **v2.0** | 2025-11-20 | 添加ADPCM音频压缩支持 |
| v1.5 | 2025-11-15 | 增加语音唤醒特征值 |  
| v1.0 | 2025-11-10 | 初始版本，基础GATT服务 |

---

## 🛠️ Python测试工具

### **1. 通用GATT通知监听器** ⭐ (推荐用于调试)

**文件**: `host/gatt_notification_monitor.py`

**功能**:
- ✅ 自动订阅所有支持notify/indicate的特征值
- ✅ 实时显示所有BLE通知
- ✅ 智能解析B11协议命令 (0x10-0x90)
- ✅ 统计通知频率和数量
- ✅ 支持服务过滤和自定义解析器

**使用方法**:
```bash
# 扫描附近的BLE设备
python host/gatt_notification_monitor.py --scan

# 监听Blinko设备的所有通知
python host/gatt_notification_monitor.py --name Blinko

# 列出所有服务和特征值
python host/gatt_notification_monitor.py --name Blinko --list

# 只监听B11主服务 (6E400001)
python host/gatt_notification_monitor.py --name Blinko \
  --service 6E400001-B5A3-F393-E0A9-E50E24DCCA9E

# 定时监听60秒后自动停止
python host/gatt_notification_monitor.py --name Blinko --duration 60
```

**支持的协议解析**:
- `0x20` - 触发事件上报 (触摸/接近/语音)
- `0x30` - 传感器数据 (心率/电量/温度等)
- `0x40` - 心率数据通知
- `0x50` - 触摸事件通知
- `0x60` - 电量变化通知
- NFC名片交换事件 (0x30)

**输出示例**:
```
========================================
📡 开始监听通知...
   按 Ctrl+C 停止监听
========================================

[14:30:25.123] B11 TX (设备→手机):
  🎬 动作完成通知: 动作ID=5

[14:30:30.456] B11 NFC:
  👥 NFC交换名片通知:
    姓名: 小白
    UUID: 04a1b2c3...

[14:30:35.789] B11 TX (设备→手机):
  ❤️  心率数据通知: 72 bpm (质量: 95)

========================================
📊 通知统计:
========================================
   总通知数: 156
   持续时间: 60.12秒
   平均速率: 2.60 通知/秒

   各特征通知数:
     B11 Audio: 120
     B11 TX (设备→手机): 25
     B11 NFC: 8
     Battery Level: 3
========================================
```

**测试场景**:
```bash
# 场景1: 测试触摸事件
python host/gatt_notification_monitor.py --name Blinko
# 触摸设备额头/肚子，观察 0x20 通知

# 场景2: 测试NFC名片交换
python host/gatt_notification_monitor.py --name Blinko
# 两个设备NFC模块靠近10cm，观察 0x30 通知

# 场景3: 测试电池状态
python host/gatt_notification_monitor.py --name Blinko
# 插拔充电器，观察 0x50 通知

# 场景4: 测试语音唤醒
python host/gatt_notification_monitor.py --name Blinko
# 说出关键词，观察 0x20 0x03 通知
```

---

### **2. 音频录音工具**

**文件**: `host/ble_audio_recorder_8khz.py`

**功能**:
- ✅ 发送录音控制命令 (0x40)
- ✅ 接收PCM音频流 (8kHz/16bit/Mono)
- ✅ 保存为标准WAV文件
- ✅ 实时进度显示和音频统计

**使用方法**:
```bash
# 录制5秒音频
python host/ble_audio_recorder_8khz.py --name Blinko --duration 5

# 自定义输出文件名
python host/ble_audio_recorder_8khz.py --name Blinko \
  --output my_recording.wav --duration 10

# 扫描设备
python host/ble_audio_recorder_8khz.py --scan
```

**输出示例**:
```
Recording... [##########----------] 51.2% (2.5s / 5.0s) | 10.24 KB
Audio stats: min=-12543, max=11298, avg=-15
✓ Saved to: recording_20251130_120000.wav
```

---

### **3. NFC名片管理工具**

**文件**: `host/nfc_ble_manager.py`

**功能**:
- ✅ 设置我的名片 (0x20命令)
- ✅ 读取我的名片 (0x21命令)
- ✅ 读取好友名片 (0x22命令)
- ✅ 获取好友数量 (0x23命令)
- ✅ 监听名片交换事件 (0x30通知)

**使用方法**:
```bash
# 启动NFC管理器
python host/nfc_ble_manager.py --name Blinko

# 扫描设备
python host/nfc_ble_manager.py --scan
```

**交互式命令**:
```python
# 示例：设置我的名片
NFC Manager> set
Enter pet name: 小白
Enter UUID (16 bytes hex, or press Enter for random): 
Enter personality traits: playful,loyal,smart
Enter catchphrase: Life is an adventure!
✓ My card set successfully

# 读取我的名片
NFC Manager> get_my
Pet Name: 小白
UUID: 04a1b2c3d4e5f6...
Personality: playful,loyal,smart
Catchphrase: Life is an adventure!

# 读取好友名片
NFC Manager> get_friend
Friend Card:
  Pet Name: 小黑
  UUID: 11223344556677...
  Personality: brave,curious
  
# 获取好友数量
NFC Manager> count
Friend count: 1
```

---

### **4. OTA固件升级工具**

**文件**: `host/ble_ota_tester.py`

**功能**:
- ✅ 通过BLE进行固件升级
- ✅ 支持ESP-IDF OTA分区（双分区A/B切换）
- ✅ 实时进度显示
- ✅ CRC16-CCITT校验和错误检测
- ✅ 4KB扇区传输，自动MTU适配
- ✅ 固件完整性验证

**OTA服务定义**:
```
服务UUID: 00008018-0000-1000-8000-00805f9b34fb

特征值:
  1. RECV_FW (0x8020)    - Write固件数据，Notify接收ACK
  2. PROGRESS (0x8021)   - Notify进度更新
  3. COMMAND (0x8022)    - Write命令，Notify接收ACK
  4. CUSTOMER (0x8023)   - 自定义数据（预留）
```

**OTA升级流程**:
```
1. 发送START_OTA命令（0x0001）+ 固件大小
2. 按4KB扇区传输固件数据
   - 每个扇区分多个包传输（根据MTU）
   - 最后一包带CRC16校验码
   - ESP32响应ACK确认
3. 发送END_OTA命令（0x0002）
4. 设备验证固件并重启
```

**数据包格式**:
```
命令包（20字节）:
  [命令ID(2)] [载荷(16)] [CRC16(2)]

固件包（MTU自适应）:
  [扇区索引(2)] [包序号(1)] [数据(N)] [CRC16(2,仅最后包)]
  
ACK响应（20字节）:
  [扇区索引(2)] [状态码(2)] [预留(14)] [CRC16(2)]
  
状态码:
  0x0000 - 成功
  0x0001 - CRC错误
  0x0002 - 扇区索引错误
  0x0003 - 数据长度错误
```

**使用方法**:
```bash
# OTA升级固件
python host/ble_ota_tester.py --name Blinko \
  --firmware build/NimBLE_GATT_Server.bin

# 通过MAC地址连接
python host/ble_ota_tester.py --address AA:BB:CC:DD:EE:FF \
  --firmware firmware.bin

# 扫描设备
python host/ble_ota_tester.py --scan
```

**输出示例**:
```
========================================
BLE OTA 固件升级测试
========================================

[步骤 1/4] 正在连接到设备: Blinko
✅ 连接成功

[步骤 2/4] 启用BLE通知...
✅ 通知已启用

[步骤 3/4] 开始上传固件: NimBLE_GATT_Server.bin
Firmware: NimBLE_GATT_Server.bin
Size: 819200 bytes (800.00 KB)
Sectors: 200

Uploading sector 1/200...
Progress: 0.5% | Speed: 45.23 KB/s
...
✅ 固件上传成功！
Total time: 80.45s | Average speed: 42.31 KB/s

[步骤 4/4] OTA升级完成
设备将重启并运行新固件...
```

---

## 🎯 工具选择指南

| 需求 | 推荐工具 | 原因 |
|------|---------|------|
| 🧪 **调试所有BLE通知** | `gatt_notification_monitor.py` | 全面通用，即插即用 |
| 🎵 **录音保存WAV文件** | `ble_audio_recorder_8khz.py` | 专业音频处理 |
| 👥 **管理NFC名片** | `nfc_ble_manager.py` | 完整CRUD功能 |
| 📦 **固件OTA升级** | `ble_ota_tester.py` | 安全可靠的升级 |
| 🔍 **快速查看设备信息** | `--list` 参数 | 列出所有服务特征 |

---

## 📝 开发者备注

### **重要变更说明**

1. **音频格式变更**: 
   - 文档v2.0描述使用ADPCM压缩，但实际固件实现为PCM原始格式
   - 开发者应使用PCM格式接收和保存音频数据
   - 参考 `ble_audio_recorder_8khz.py` 获取完整实现示例

2. **触发事件上报**:
   - 格式为 `[0x20] [触发类型] [动作ID]`
   - 动作ID表示实际执行的动作组合编号
   - 不再上报当前情绪区间

3. **NFC命令响应**:
   - 所有NFC命令通过Notify特征值异步响应
   - Write操作不返回直接响应
   - 需订阅Notify特征值接收响应数据

4. **测试工具更新**:
   - v2.3: 完善OTA工具协议说明，添加详细数据包格式文档
   - v2.3: OTA支持MTU自适应和扇区CRC验证
   - v2.2: 添加完整工具文档章节和使用指南
   - v2.1: 新增通用GATT通知监听器 (`gatt_notification_monitor.py`)
   - v2.1: 更新OTA工具支持ESP-IDF v5.5 (`ble_ota_tester.py`)
   - 所有工具统一使用设备名 "Blinko"
   - 完善错误处理和日志输出

5. **参考实现**:
   - Python BLE客户端: `host/ble_audio_recorder_8khz.py`
   - NFC管理工具: `host/nfc_ble_manager.py`
   - 通用监听器: `host/gatt_notification_monitor.py`
   - OTA升级工具: `host/ble_ota_tester.py`
   - 自动化测试: `test/b11_auto_test.py`

