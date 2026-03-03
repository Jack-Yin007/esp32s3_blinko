#!/usr/bin/env pwsh
<#
.SYNOPSIS
    初始化Flash音频文件 - 通过BLE上传所有WAV文件到ESP32
    
.DESCRIPTION
    此脚本执行以下操作：
    1. 检查ESP32设备是否已烧录固件
    2. 通过BLE连接到ESP32
    3. 将audio目录下的所有WAV文件上传到/flash/audio
    
.PARAMETER Port
    ESP32连接的串口（默认: COM3）
    
.PARAMETER Address
    ESP32的BLE MAC地址（可选，会自动扫描）

.EXAMPLE
    .\init_flash_audio.ps1
    .\init_flash_audio.ps1 -Port COM5
    .\init_flash_audio.ps1 -Address "AA:BB:CC:DD:EE:FF"
#>

param(
    [string]$Port = "COM3",
    [string]$Address = ""
)

# 设置错误处理
$ErrorActionPreference = "Stop"

# 项目路径
$ProjectRoot = $PSScriptRoot
$AudioDir = Join-Path $ProjectRoot "audio"
$HostDir = Join-Path $ProjectRoot "host"
$UploaderScript = Join-Path $HostDir "ble_flash_audio_uploader.py"

Write-Host "=" -NoNewline -ForegroundColor Cyan
Write-Host ("=" * 79) -ForegroundColor Cyan
Write-Host "  Flash Audio Initialization" -ForegroundColor White
Write-Host "  初始化Flash音频文件系统" -ForegroundColor White
Write-Host ("=" * 80) -ForegroundColor Cyan
Write-Host ""

# 检查audio目录
if (-not (Test-Path $AudioDir)) {
    Write-Host "❌ 音频目录不存在: $AudioDir" -ForegroundColor Red
    exit 1
}

$WavFiles = Get-ChildItem -Path $AudioDir -Filter "*.wav"
Write-Host "📁 找到 $($WavFiles.Count) 个WAV文件" -ForegroundColor Green
Write-Host ""

# 检查Python环境
Write-Host "🔍 检查Python环境..." -ForegroundColor Yellow
try {
    $PythonVersion = python --version 2>&1
    Write-Host "  ✓ Python: $PythonVersion" -ForegroundColor Green
} catch {
    Write-Host "  ❌ 未找到Python，请先安装Python 3.7+" -ForegroundColor Red
    exit 1
}

# 检查依赖包
Write-Host "🔍 检查依赖包..." -ForegroundColor Yellow
$RequiredPackages = @("bleak")
foreach ($Package in $RequiredPackages) {
    $Installed = pip show $Package 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ⚠️ 缺少依赖包: $Package" -ForegroundColor Yellow
        Write-Host "  📦 正在安装 $Package..." -ForegroundColor Cyan
        pip install $Package
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  ❌ 安装失败" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "  ✓ $Package 已安装" -ForegroundColor Green
    }
}
Write-Host ""

# 提示用户确保设备已连接
Write-Host "⚡ 请确保：" -ForegroundColor Yellow
Write-Host "  1. ESP32已通过USB连接到 $Port" -ForegroundColor White
Write-Host "  2. ESP32已烧录最新固件" -ForegroundColor White
Write-Host "  3. ESP32蓝牙已启动（设备名包含'ESP32'或'nimble'）" -ForegroundColor White
Write-Host ""
Write-Host "按任意键继续..." -ForegroundColor Cyan
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
Write-Host ""

# 执行上传
Write-Host "=" -NoNewline -ForegroundColor Cyan
Write-Host ("=" * 79) -ForegroundColor Cyan
Write-Host "  开始上传音频文件" -ForegroundColor White
Write-Host ("=" * 80) -ForegroundColor Cyan
Write-Host ""

try {
    if ($Address) {
        Write-Host "📱 使用指定的BLE地址: $Address" -ForegroundColor Cyan
        python $UploaderScript --address $Address --audio-dir $AudioDir
    } else {
        Write-Host "🔍 自动扫描BLE设备..." -ForegroundColor Cyan
        python $UploaderScript --audio-dir $AudioDir
    }
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host ""
        Write-Host "=" -NoNewline -ForegroundColor Green
        Write-Host ("=" * 79) -ForegroundColor Green
        Write-Host "  ✅ Flash音频文件初始化成功！" -ForegroundColor Green
        Write-Host ("=" * 80) -ForegroundColor Green
        Write-Host ""
        Write-Host "现在可以使用以下工具查看文件：" -ForegroundColor White
        Write-Host "  python host/ble_file_explorer.py" -ForegroundColor Cyan
    } else {
        Write-Host ""
        Write-Host "❌ 上传失败，退出码: $LASTEXITCODE" -ForegroundColor Red
        exit 1
    }
    
} catch {
    Write-Host ""
    Write-Host "❌ 上传过程中出错: $_" -ForegroundColor Red
    exit 1
}
