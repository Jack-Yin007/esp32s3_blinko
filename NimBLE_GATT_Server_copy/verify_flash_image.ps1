#!/usr/bin/env pwsh
<#
.SYNOPSIS
    验证Flash镜像配置和编译状态
    
.DESCRIPTION
    检查flash_image目录结构、音频文件、编译输出和烧录配置
    
.EXAMPLE
    .\verify_flash_image.ps1
#>

$ErrorActionPreference = "Continue"

Write-Host "=" -NoNewline -ForegroundColor Cyan
Write-Host ("=" * 79) -ForegroundColor Cyan
Write-Host "  Flash镜像配置验证工具" -ForegroundColor White
Write-Host ("=" * 80) -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = $PSScriptRoot
$FlashImageDir = Join-Path $ProjectRoot "flash_image"
$AudioDir = Join-Path $FlashImageDir "audio"
$NfcDir = Join-Path $FlashImageDir "nfc"
$BuildDir = Join-Path $ProjectRoot "build"
$FlashBin = Join-Path $BuildDir "flash.bin"
$FlasherArgs = Join-Path $BuildDir "flasher_args.json"

$AllGood = $true

# 1. 检查flash_image目录结构
Write-Host "📁 检查目录结构..." -ForegroundColor Yellow
if (Test-Path $FlashImageDir) {
    Write-Host "  ✓ flash_image/ 存在" -ForegroundColor Green
    
    if (Test-Path $AudioDir) {
        Write-Host "  ✓ flash_image/audio/ 存在" -ForegroundColor Green
    } else {
        Write-Host "  ✗ flash_image/audio/ 不存在" -ForegroundColor Red
        $AllGood = $false
    }
    
    if (Test-Path $NfcDir) {
        Write-Host "  ✓ flash_image/nfc/ 存在" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ flash_image/nfc/ 不存在（可选）" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ✗ flash_image/ 不存在" -ForegroundColor Red
    $AllGood = $false
}
Write-Host ""

# 2. 检查音频文件
Write-Host "🎵 检查音频文件..." -ForegroundColor Yellow
if (Test-Path $AudioDir) {
    $WavFiles = Get-ChildItem -Path $AudioDir -Filter "*.wav" -ErrorAction SilentlyContinue
    $WavCount = $WavFiles.Count
    
    if ($WavCount -gt 0) {
        Write-Host "  ✓ 找到 $WavCount 个WAV文件" -ForegroundColor Green
        
        # 计算总大小
        $TotalSize = ($WavFiles | Measure-Object -Property Length -Sum).Sum
        $TotalSizeMB = [math]::Round($TotalSize / 1MB, 2)
        Write-Host "  ✓ 总大小: $TotalSizeMB MB" -ForegroundColor Green
        
        # 显示部分文件
        Write-Host "  📄 文件列表（前10个）:" -ForegroundColor Cyan
        $WavFiles | Select-Object -First 10 | ForEach-Object {
            $sizekb = [math]::Round($_.Length / 1KB, 1)
            Write-Host "     - $($_.Name) ($sizekb KB)" -ForegroundColor Gray
        }
        
        if ($WavCount -gt 10) {
            Write-Host "     ... 还有 $($WavCount - 10) 个文件" -ForegroundColor Gray
        }
        
        # 检查是否有23个标准文件
        $Expected = @(1..23 | ForEach-Object { "$_.wav" })
        $Missing = $Expected | Where-Object { -not (Test-Path (Join-Path $AudioDir $_)) }
        if ($Missing.Count -eq 0) {
            Write-Host "  ✓ 标准音频文件完整 (1.wav ~ 23.wav)" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ 缺少部分标准文件: $($Missing -join ', ')" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ✗ 未找到WAV文件" -ForegroundColor Red
        $AllGood = $false
    }
} else {
    Write-Host "  ✗ audio目录不存在" -ForegroundColor Red
    $AllGood = $false
}
Write-Host ""

# 3. 检查CMakeLists.txt配置
Write-Host "⚙️  检查CMakeLists.txt配置..." -ForegroundColor Yellow
$CMakeLists = Join-Path $ProjectRoot "CMakeLists.txt"
if (Test-Path $CMakeLists) {
    $Content = Get-Content $CMakeLists -Raw
    if ($Content -match "fatfs_create_spiflash_image") {
        Write-Host "  ✓ fatfs_create_spiflash_image 已配置" -ForegroundColor Green
    } else {
        Write-Host "  ✗ 未找到 fatfs_create_spiflash_image 配置" -ForegroundColor Red
        $AllGood = $false
    }
} else {
    Write-Host "  ✗ CMakeLists.txt 不存在" -ForegroundColor Red
    $AllGood = $false
}
Write-Host ""

# 4. 检查编译输出
Write-Host "🔨 检查编译输出..." -ForegroundColor Yellow
if (Test-Path $FlashBin) {
    $FlashBinFile = Get-Item $FlashBin
    $FlashBinSizeMB = [math]::Round($FlashBinFile.Length / 1MB, 2)
    Write-Host "  ✓ build/flash.bin 已生成" -ForegroundColor Green
    Write-Host "  ✓ 镜像大小: $FlashBinSizeMB MB ($($FlashBinFile.Length) bytes)" -ForegroundColor Green
    Write-Host "  ✓ 上次编译: $($FlashBinFile.LastWriteTime)" -ForegroundColor Green
    
    # 验证大小（应该是4MB）
    if ($FlashBinFile.Length -eq 4194304) {
        Write-Host "  ✓ 镜像大小正确 (4MB = 分区大小)" -ForegroundColor Green
    } else {
        Write-Host "  ⚠ 镜像大小异常（预期4MB）" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ⚠ build/flash.bin 不存在（需要先编译）" -ForegroundColor Yellow
    Write-Host "    运行: .\rebuild.ps1 或 idf.py build" -ForegroundColor Cyan
}
Write-Host ""

# 5. 检查烧录配置
Write-Host "📤 检查烧录配置..." -ForegroundColor Yellow
if (Test-Path $FlasherArgs) {
    try {
        $FlasherConfig = Get-Content $FlasherArgs -Raw | ConvertFrom-Json
        
        if ($FlasherConfig.flash_files."0x310000") {
            $FlashFile = $FlasherConfig.flash_files."0x310000"
            Write-Host "  ✓ flash.bin 已添加到烧录配置" -ForegroundColor Green
            Write-Host "    地址: 0x310000 (3.1MB)" -ForegroundColor Gray
            Write-Host "    文件: $FlashFile" -ForegroundColor Gray
        } else {
            Write-Host "  ✗ flash.bin 未在烧录配置中" -ForegroundColor Red
            $AllGood = $false
        }
        
        # 显示所有烧录项
        Write-Host "  📋 完整烧录列表:" -ForegroundColor Cyan
        $FlasherConfig.flash_files.PSObject.Properties | ForEach-Object {
            Write-Host "     $($_.Name) -> $($_.Value)" -ForegroundColor Gray
        }
    } catch {
        Write-Host "  ✗ 无法解析 flasher_args.json: $_" -ForegroundColor Red
        $AllGood = $false
    }
} else {
    Write-Host "  ⚠ flasher_args.json 不存在（需要先编译）" -ForegroundColor Yellow
}
Write-Host ""

# 6. 检查分区表
Write-Host "🗂️  检查分区表..." -ForegroundColor Yellow
$Partitions = Join-Path $ProjectRoot "partitions.csv"
if (Test-Path $Partitions) {
    $Content = Get-Content $Partitions
    $FlashPartition = $Content | Where-Object { $_ -match "^flash" }
    
    if ($FlashPartition) {
        Write-Host "  ✓ Flash分区已定义" -ForegroundColor Green
        Write-Host "    $FlashPartition" -ForegroundColor Gray
        
        # 解析分区信息
        if ($FlashPartition -match "0x310000.*0x400000") {
            Write-Host "  ✓ 分区配置正确 (起始:0x310000, 大小:0x400000)" -ForegroundColor Green
        } else {
            Write-Host "  ⚠ 分区配置可能不匹配" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ✗ 未找到Flash分区定义" -ForegroundColor Red
        $AllGood = $false
    }
} else {
    Write-Host "  ✗ partitions.csv 不存在" -ForegroundColor Red
    $AllGood = $false
}
Write-Host ""

# 最终结果
Write-Host "=" -NoNewline -ForegroundColor Cyan
Write-Host ("=" * 79) -ForegroundColor Cyan
if ($AllGood) {
    Write-Host "  ✅ 配置验证通过！" -ForegroundColor Green
    Write-Host ""
    Write-Host "📝 后续步骤:" -ForegroundColor White
    Write-Host "  1. 编译项目: .\rebuild.ps1 或 idf.py build" -ForegroundColor Cyan
    Write-Host "  2. 烧录固件: .\rebuild.ps1 或 idf.py flash" -ForegroundColor Cyan
    Write-Host "  3. 验证Flash: python host\ble_file_explorer.py" -ForegroundColor Cyan
} else {
    Write-Host "  ⚠️ 发现配置问题，请检查上述错误" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "💡 修复建议:" -ForegroundColor White
    Write-Host "  1. 确保 flash_image/audio/ 目录存在并包含WAV文件" -ForegroundColor Cyan
    Write-Host "  2. 运行: Copy-Item audio\*.wav flash_image\audio\" -ForegroundColor Cyan
    Write-Host "  3. 重新编译: .\rebuild.ps1" -ForegroundColor Cyan
}
Write-Host ("=" * 80) -ForegroundColor Cyan
Write-Host ""
