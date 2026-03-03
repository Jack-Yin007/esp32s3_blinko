## 🎵 Audio特征值 (音频流)

**UUID**: `6E400004-B5A3-F393-E0A9-E50E24DCCA9E`  
**权限**: Notify  
**用途**: IMA ADPCM压缩音频流传输

### **音频规格**
- **采样率**: 8000 Hz
- **位深**: 16-bit PCM → 4-bit ADPCM  
- **声道**: 单声道 (Mono)
- **压缩算法**: IMA ADPCM
- **压缩比**: 4:1
- **数据率**: ~4 KB/s (压缩后)

### **数据包格式**
```
[样本数高字节] [样本数低字节] [原始字节数高] [原始字节数低] [ADPCM数据...]
     ^2字节样本数^              ^2字节PCM大小^         ^压缩音频^
```

**帧头说明** (4字节):
- **字节0-1**: 样本数 (大端序, 16-bit unsigned)
- **字节2-3**: 原始PCM字节数 (大端序, 16-bit unsigned)
- **字节4+**: ADPCM压缩数据

### **数据接收流程**
1. **启用通知**: 订阅Audio特征值
2. **接收数据**: 每个BLE通知包含最多244字节
3. **解析帧头**: 读取前4字节获取元数据
4. **ADPCM解码**: 使用IMA ADPCM算法解压
5. **PCM重建**: 输出16-bit PCM样本
6. **WAV保存**: 以8kHz采样率保存

### **Python解码示例**
```python
import wave
import struct

class ADPCMDecoder:
    """IMA ADPCM解码器"""
    
    # IMA ADPCM索引表
    INDEX_TABLE = [
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8
    ]
    
    # 步进表
    STEP_TABLE = [
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
        19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
        130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
        337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    ]
    
    def __init__(self):
        self.predicted_sample = 0
        self.step_index = 0
    
    def decode_nibble(self, nibble):
        """解码单个4-bit样本"""
        step = self.STEP_TABLE[self.step_index]
        
        # 计算差分值
        diff = step >> 3
        if nibble & 4:
            diff += step
        if nibble & 2:
            diff += step >> 1
        if nibble & 1:
            diff += step >> 2
        
        # 应用符号位
        if nibble & 8:
            self.predicted_sample -= diff
        else:
            self.predicted_sample += diff
        
        # 限幅到16-bit范围
        if self.predicted_sample > 32767:
            self.predicted_sample = 32767
        elif self.predicted_sample < -32768:
            self.predicted_sample = -32768
        
        # 更新步进索引
        self.step_index += self.INDEX_TABLE[nibble & 0x07]
        if self.step_index < 0:
            self.step_index = 0
        elif self.step_index > 88:
            self.step_index = 88
        
        return self.predicted_sample
    
    def decode(self, adpcm_data):
        """解码ADPCM数据到PCM"""
        pcm_samples = []
        
        for byte in adpcm_data:
            # 每个字节包含2个4-bit样本
            # 低4位在前，高4位在后
            nibble1 = byte & 0x0F
            nibble2 = (byte >> 4) & 0x0F
            
            pcm_samples.append(self.decode_nibble(nibble1))
            pcm_samples.append(self.decode_nibble(nibble2))
        
        return pcm_samples

# 音频接收缓冲区
audio_buffer = bytearray()
decoder = ADPCMDecoder()
pcm_samples = []

def audio_notification_handler(sender, data):
    """BLE音频通知处理"""
    global audio_buffer, decoder, pcm_samples
    
    # 累积接收数据
    audio_buffer.extend(data)
    
    # 如果是第一个包，解析帧头
    if len(audio_buffer) >= 4:
        sample_count = (data[0] << 8) | data[1]
        pcm_bytes = (data[2] << 8) | data[3]
        adpcm_data = data[4:]
        
        # 解码ADPCM数据
        decoded = decoder.decode(adpcm_data)
        pcm_samples.extend(decoded[:sample_count])
        
        print(f"Received {len(data)} bytes, decoded {len(decoded)} samples, total: {len(pcm_samples)}")

# 保存WAV文件
def save_wav(filename, sample_rate=8000):
    """保存PCM样本为WAV文件"""
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(1)  # Mono
        wav_file.setsampwidth(2)  # 16-bit
        wav_file.setframerate(sample_rate)
        
        # 转换为字节
        pcm_bytes = struct.pack(f'{len(pcm_samples)}h', *pcm_samples)
        wav_file.writeframes(pcm_bytes)
    
    print(f"Saved {len(pcm_samples)} samples to {filename}")
```

### **性能参数**
- **原始PCM数据率**: 16 KB/s (8kHz × 2 bytes)
- **ADPCM压缩后**: ~4 KB/s (4:1压缩)
- **BLE有效吞吐量**: ~22.75 KB/s
- **带宽节省**: 73.4%
- **传输效率**: 实时传输无压力
- **延迟**: <100ms（取决于BLE连接间隔）
