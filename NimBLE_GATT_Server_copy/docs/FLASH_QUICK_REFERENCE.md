# Flash音频文件 - 快速参考

## ✅ 当前状态：已配置完成

### 一句话总结
**编译时自动生成包含23个WAV文件的Flash镜像，一次烧录完成，无需额外配置。**

---

## 🚀 快速使用

### 编译烧录
```powershell
# 一键完成（推荐）
.\rebuild.ps1

# 或手动
idf.py build    # 自动生成 build/flash.bin (4MB)
idf.py flash    # 自动烧录到 0x310000
```

### 验证结果
```powershell
# 使用BLE文件浏览器查看
python host\ble_file_explorer.py

# 应该看到 /flash/audio/ 下有23个WAV文件
```

---

## 📁 目录结构

```
flash_image/
├── audio/              # ✅ 23个WAV文件（已就绪）
│   ├── 1.wav
│   ├── 2.wav
│   └── ... (23个文件，共1.5MB)
└── nfc/                # NFC文件（可选）
```

---

## 🔧 工作原理

```mermaid
graph LR
    A[flash_image/audio/*.wav] --> B[idf.py build]
    B --> C[wl_fatfsgen.py]
    C --> D[build/flash.bin 4MB]
    D --> E[idf.py flash]
    E --> F[ESP32 @ 0x310000]
    F --> G[/flash/audio/*.wav]
```

### 编译时
1. ESP-IDF检测到 `flash_image/` 目录
2. 调用 `fatfsgen.py` 创建FAT镜像
3. 生成 `build/flash.bin` (4MB)
4. 添加到烧录配置

### 烧录时
- 自动烧录 `flash.bin` 到地址 `0x310000`
- 包含在标准烧录流程中

---

## 📊 分区配置

| 项目 | 值 |
|------|-----|
| 分区名 | flash |
| 类型 | data (FAT) |
| 起始地址 | 0x310000 |
| 大小 | 0x400000 (4MB) |
| 挂载点 | /flash |
| 子目录 | /flash/audio, /flash/nfc |

---

## 🔄 更新音频文件

```powershell
# 1. 替换文件
Copy-Item new_audio\*.wav flash_image\audio\ -Force

# 2. 重新编译烧录
.\rebuild.ps1
```

---

## 📝 常见命令

### 查看镜像文件
```powershell
# 检查flash.bin
Get-Item build\flash.bin

# 查看音频文件
Get-ChildItem flash_image\audio\*.wav
```

### 仅重新生成镜像
```powershell
idf.py build
# flash.bin会自动更新
```

### 仅烧录Flash分区
```powershell
esptool.py -p COM3 write_flash 0x310000 build\flash.bin
```

---

## 💡 优势对比

| 方案 | 优势 | 缺点 |
|-----|------|------|
| **镜像烧录** | 一次完成，快速，生产友好 | 更新需要重新烧录 |
| BLE上传 | 灵活更新 | 需要额外步骤，较慢 |
| SD卡复制 | 离线操作 | 需要SD卡硬件 |

---

## ⚠️ 重要提示

1. ✅ **音频文件已就绪** - flash_image/audio 已包含23个WAV文件
2. ✅ **自动编译** - 每次 `idf.py build` 自动生成镜像
3. ✅ **自动烧录** - `idf.py flash` 自动烧录镜像
4. ⚠️ **容量限制** - Flash分区4MB，当前使用1.5MB，剩余2.5MB

---

## 📚 详细文档

- [flash_image/README.md](../flash_image/README.md) - 镜像使用说明
- [docs/FLASH_IMAGE_IMPLEMENTATION.md](FLASH_IMAGE_IMPLEMENTATION.md) - 实施细节
- [docs/FLASH_AUDIO_DEPLOYMENT.md](FLASH_AUDIO_DEPLOYMENT.md) - 方案对比

---

**更新时间**: 2025-12-28  
**状态**: ✅ 生产就绪
