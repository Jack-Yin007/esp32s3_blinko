# Flash FAT分区镜像构建说明

## ✅ 已配置完成 - 开箱即用！

**flash_image/audio/** 目录已包含23个WAV文件，编译时会自动生成镜像并烧录。

## 目录结构
```
flash_image/
├── audio/          # ✅ 已包含23个WAV文件 (1.wav ~ 23.wav)
│   ├── 1.wav       # 64.7 KB
│   ├── 2.wav       # 72.9 KB
│   └── ...         # (共23个文件)
└── nfc/            # NFC相关文件（可选，默认为空）
```

## 使用方法

### ⚡ 一键编译烧录（推荐）

```powershell
# 使用项目提供的脚本（自动编译+烧录）
.\rebuild.ps1
```

**说明**：
- ✅ 自动编译固件和Flash镜像
- ✅ 自动烧录所有分区（包括flash.bin）
- ✅ WAV文件会自动写入ESP32的 /flash/audio 目录
- ✅ 无需额外步骤，烧录后直接可用

### 🔧 手动编译（ESP-IDF命令）

```bash
# 编译项目
idf.py build
```

编译输出：
- `build/nimble_gatt_server.bin` - 主程序固件
- `build/flash.bin` - Flash分区镜像（4MB，包含audio文件）

### 📤 手动烧录

```bash
# 烧录所有分区（包括flash.bin）
idf.py flash
```

或使用ESP-IDF烧录工具：
```bash
idf.py -p COM3 flash
```

### 🛠️ 高级：分区单独烧录

```bash
# 仅烧录Flash分区镜像
esptool.py --chip esp32s3 --port COM3 write_flash 0x310000 build/flash.bin
```

## 🔍 验证安装

烧录完成后，使用BLE文件浏览器查看Flash内容：

```powershell
# 查看/flash/audio目录
python host\ble_file_explorer.py
```

应该看到23个WAV文件已经存在于 `/flash/audio/` 目录中。

## 📝 分区信息

- **分区名称**: `flash`
- **分区类型**: data (FAT)
- **起始地址**: 0x310000 (3.1MB)
- **分区大小**: 0x400000 (4MB)
- **挂载点**: `/flash`
- **子目录**: 
  - `/flash/audio/` - 音频文件（23个WAV文件，约1.5MB）
  - `/flash/nfc/` - NFC文件（预留）

## 🔄 更新音频文件

如需更新音频文件：

### 方法1：重新编译烧录
```powershell
# 1. 替换 flash_image/audio/ 中的文件
Copy-Item new_audio\*.wav flash_image\audio\ -Force

# 2. 重新编译烧录
.\rebuild.ps1
```

### 方法2：通过BLE上传（无需重新烧录）
```powershell
# 使用BLE上传工具更新单个文件
python host\ble_flash_audio_uploader.py --audio-dir new_audio
```

## 💡 技术细节

### 镜像生成过程

编译时，ESP-IDF的 `fatfs_create_spiflash_image()` 会：
1. 读取 `flash_image/` 目录内容
2. 创建FAT文件系统镜像
3. 将所有文件写入镜像
4. 生成 `build/flash.bin`（4MB）
5. 自动添加到烧录配置

### CMakeLists.txt配置

```cmake
fatfs_create_spiflash_image(flash ${CMAKE_SOURCE_DIR}/flash_image FLASH_IN_PROJECT)
```

- `flash` - 对应 partitions.csv 中的分区名
- `FLASH_IN_PROJECT` - 自动添加到烧录任务

### 烧录配置

查看 `build/flasher_args.json`：
```json
{
  "flash_files": {
    "0x310000": "flash.bin"
  }
}
```

## 🎯 最佳实践

### 开发阶段
- ✅ 使用 `.\rebuild.ps1` 快速编译烧录
- ✅ 音频文件变化不频繁时，使用镜像方式

### 调试阶段  
- ✅ 使用BLE上传更新单个文件
- ✅ 避免频繁重新烧录整个固件

### 量产阶段
- ✅ 确保 `flash_image/audio/` 包含最终音频
- ✅ 使用统一的镜像烧录
- ✅ 一次烧录，开箱即用

## ⚠️ 注意事项

1. **Flash空间限制**
   - Flash分区总大小：4MB
   - 当前音频文件：约1.5MB
   - 剩余可用：约2.5MB

2. **文件格式要求**
   - 格式：WAV (PCM)
   - 采样率：8 KHz
   - 声道：立体声
   - 位深：16-bit

3. **文件命名**
   - 建议使用数字命名：1.wav, 2.wav, ...
   - 避免中文或特殊字符
   - 大小写不敏感（FAT文件系统）

## 🚀 快速开始

```powershell
# 1. 确认音频文件已就绪（已包含23个文件）
ls flash_image\audio\*.wav

# 2. 一键编译烧录
.\rebuild.ps1

# 3. 验证结果
python host\ble_file_explorer.py

# 完成！音频播放功能可用
```

---

**更新日期**: 2025-12-28  
**状态**: ✅ 已配置完成，开箱即用
- **类型**: FAT
- **偏移地址**: 0x310000
- **大小**: 4MB (0x400000)
- **挂载点**: `/flash`
- **子目录**:
  - `/flash/audio/` - 音频文件存储
  - `/flash/nfc/` - NFC数据存储

## 注意事项

1. **文件命名**: WAV文件必须命名为 `1.wav`, `2.wav`, ..., `23.wav` (与代码中的ID对应)
2. **文件大小**: 确保所有文件总大小 < 4MB
3. **文件格式**: 8kHz/16kHz, 16-bit PCM, 单声道/立体声
4. **首次烧录**: 建议先将所有WAV文件放入 `flash_image/audio/` 后再编译烧录
5. **更新文件**: 如需更新WAV文件，重新编译并烧录 `build/flash.bin` 到地址 0x310000

## 验证

烧录后通过串口监控查看日志：
```
I (609) example: ✅ Flash FAT mounted at /flash
I (619) example:   /flash/audio - Audio files
I (629) example:   /flash/nfc   - NFC files
```

播放测试时应能成功打开文件：
```
I (12429) WAVPlayer: Opening: /flash/audio/19.wav
```
