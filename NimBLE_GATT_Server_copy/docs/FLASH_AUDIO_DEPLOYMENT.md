# Flash音频文件部署指南

## 问题描述

烧录固件后，`/flash/audio` 目录为空，导致音频播放功能无法使用。

## 根本原因

ESP32的Flash分区（FAT文件系统）在首次烧录时是空的，需要手动将音频文件上传到设备中。

## 解决方案

### 方案1：通过BLE上传（推荐）✅

使用提供的Python脚本通过BLE文件系统服务上传所有WAV文件。

#### 步骤：

1. **准备环境**
   ```powershell
   # 安装Python依赖
   pip install bleak
   ```

2. **烧录固件**
   ```powershell
   # 如果还没烧录，先烧录固件
   .\rebuild.ps1
   ```

3. **运行初始化脚本**
   ```powershell
   # 自动扫描并上传（最简单）
   .\init_flash_audio.ps1
   
   # 或指定端口
   .\init_flash_audio.ps1 -Port COM5
   
   # 或指定BLE地址
   .\init_flash_audio.ps1 -Address "AA:BB:CC:DD:EE:FF"
   ```

4. **验证结果**
   ```powershell
   # 使用文件浏览器查看
   python host\ble_file_explorer.py
   ```

#### 手动使用Python脚本：

```powershell
# 直接使用上传脚本
python host\ble_flash_audio_uploader.py --audio-dir audio
```

#### 优点：
- ✅ 灵活方便，随时更新
- ✅ 无需重新烧录固件
- ✅ 可以选择性上传文件
- ✅ 支持自动扫描设备

#### 缺点：
- ⚠️ 需要BLE连接
- ⚠️ 上传速度较慢（取决于BLE传输速度）
- ⚠️ 首次上传需要几分钟

---

### 方案2：通过SD卡复制

如果设备支持SD卡，可以使用已有的 `flash_storage_copy_wav_files()` 函数。

#### 步骤：

1. **准备SD卡**
   - 格式化为FAT32
   - 创建 `audio/` 目录
   - 复制所有WAV文件到SD卡的 `audio/` 目录

2. **插入SD卡到ESP32**

3. **在固件中调用复制函数**
   ```c
   // 在 pet_main.c 的 app_main() 中添加：
   if (!flash_storage_has_wav_files()) {
       ESP_LOGI(TAG, "Flash audio empty, copying from SD card...");
       esp_err_t ret = flash_storage_copy_wav_files();
       if (ret == ESP_OK) {
           ESP_LOGI(TAG, "✅ Audio files copied to flash");
       } else {
           ESP_LOGW(TAG, "⚠️ Failed to copy audio files");
       }
   }
   ```

#### 优点：
- ✅ 自动化程度高
- ✅ 不需要电脑连接

#### 缺点：
- ⚠️ 需要SD卡硬件支持
- ⚠️ 需要修改固件代码

---

### 方案3：创建预填充的Flash分区镜像（高级）

创建包含音频文件的FAT分区镜像，并在烧录时一起烧录。

#### 步骤：

1. **创建FAT镜像**
   ```powershell
   # 使用mkfatfs工具（需要单独安装）
   mkfatfs -c audio -s 0x400000 flash.img
   ```

2. **修改烧录命令**
   在 `rebuild.ps1` 或手动烧录时添加：
   ```powershell
   esptool.py write_flash 0x310000 flash.img
   ```

#### 优点：
- ✅ 一次烧录完成
- ✅ 生产环境友好

#### 缺点：
- ⚠️ 需要额外工具
- ⚠️ 更新文件需要重新烧录
- ⚠️ 镜像文件较大（4MB）

---

## 推荐工作流程

### 开发阶段：
1. 烧录固件：`.\rebuild.ps1`
2. 初始化音频：`.\init_flash_audio.ps1`
3. 测试播放功能

### 更新音频文件：
```powershell
# 直接通过BLE上传新文件
python host\ble_flash_audio_uploader.py --audio-dir audio
```

### 验证Flash内容：
```powershell
python host\ble_file_explorer.py
```

---

## 常见问题

### Q1: 为什么audio目录是空的？
**A:** ESP32的Flash分区在首次烧录时是空白的，需要手动上传文件。

### Q2: 可以在固件中嵌入音频文件吗？
**A:** 理论上可以使用 `EMBED_FILES` 或 `target_add_binary_data`，但23个WAV文件会使固件体积过大（可能超过1MB），不推荐。

### Q3: BLE上传速度慢怎么办？
**A:** 
- 确保设备距离近（<1米）
- 调整MTU大小（已设置为512）
- 考虑使用SD卡方案

### Q4: 上传失败怎么办？
**A:** 
1. 检查BLE连接是否稳定
2. 确认固件中 `fs_service.c` 已启用
3. 检查Flash分区是否正确挂载
4. 查看ESP32串口日志

### Q5: 如何删除Flash中的文件？
**A:** 
```powershell
# 使用文件浏览器删除
python host\ble_file_explorer.py
# 或格式化Flash分区（会清空所有数据）
```

---

## 文件清单

- `audio/` - 23个WAV文件（1.wav ~ 23.wav）
- `host/ble_flash_audio_uploader.py` - BLE上传脚本
- `init_flash_audio.ps1` - 一键初始化脚本（PowerShell）
- `main/src/drivers/flash_storage.c` - Flash存储工具（SD卡复制）
- `main/src/drivers/sd_card.c` - Flash FAT初始化

---

## 技术细节

### Flash分区配置
```csv
# partitions.csv
flash, data, fat, 0x310000, 0x400000
```
- 起始地址：0x310000（3.1MB）
- 大小：0x400000（4MB）
- 文件系统：FAT
- 挂载点：`/flash`

### BLE文件系统服务
- Service UUID: `12345678-1234-5678-1234-56789abcdef0`
- 支持命令：LIST_DIR, READ_FILE, WRITE_FILE, DELETE_FILE, MKDIR
- MTU: 512字节
- 传输速率：约10-20 KB/s

### 音频文件格式
- 格式：WAV（PCM）
- 采样率：8 KHz
- 声道：双声道立体声
- 位深：16-bit
- 文件大小：约100-500 KB/文件

---

## 更新记录

- **2025-12-28**: 创建BLE上传方案和PowerShell初始化脚本
- 修复了 `/flash/audio` 为空的问题
- 提供了三种部署方案供选择
