# 自动更新版本号并重新编译脚本
# 版本号格式：YYYYMMDD-branch-commit（每次编译自动更新）

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  自动版本号编译脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. 设置ESP-IDF环境
Write-Host "[1/4] 加载ESP-IDF环境..." -ForegroundColor Yellow
$env:IDF_PATH = 'C:\Espressif\frameworks\esp-idf-v5.5.1'

# 2. 清理旧的编译文件
Write-Host "[2/4] 清理旧的编译文件..." -ForegroundColor Yellow
if (Test-Path "build") {
    Remove-Item -Path "build" -Recurse -Force
    Write-Host "  ✓ build目录已清理" -ForegroundColor Green
} else {
    Write-Host "  ✓ 无需清理（build目录不存在）" -ForegroundColor Green
}

# 3. 显示即将生成的版本号信息
Write-Host ""
Write-Host "[3/4] 版本号信息（将在编译时自动生成）：" -ForegroundColor Yellow

$date = Get-Date -Format "yyyyMMdd"
Write-Host "  日期: $date" -ForegroundColor Cyan

try {
    $branch = git rev-parse --abbrev-ref HEAD 2>$null
    $commit = git rev-parse --short=6 HEAD 2>$null
    
    if ($branch -and $commit) {
        Write-Host "  分支: $branch" -ForegroundColor Cyan
        Write-Host "  提交: $commit" -ForegroundColor Cyan
        Write-Host "  版本: v${date}-${branch}-${commit}" -ForegroundColor Green
    } else {
        Write-Host "  ⚠️  Git信息获取失败，将使用默认值" -ForegroundColor Red
    }
} catch {
    Write-Host "  ⚠️  无法获取Git信息" -ForegroundColor Red
}

Write-Host ""
Write-Host "[4/4] 开始编译..." -ForegroundColor Yellow
Write-Host "  提示：版本号会在编译过程中自动嵌入固件" -ForegroundColor Gray
Write-Host ""

# 4. 执行编译（请在ESP-IDF PowerShell中手动运行 idf.py build）
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  请在ESP-IDF PowerShell终端中执行：" -ForegroundColor Yellow
Write-Host ""
Write-Host "  idf.py build" -ForegroundColor Green
Write-Host ""
Write-Host "  或者使用完整命令：" -ForegroundColor Yellow
Write-Host "  idf.py build && idf.py flash monitor" -ForegroundColor Green
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "💡 提示：" -ForegroundColor Cyan
Write-Host "  - 版本号在每次编译时自动更新" -ForegroundColor Gray
Write-Host "  - 无需手动修改任何文件" -ForegroundColor Gray
Write-Host "  - 查看版本：启动后在日志中搜索 'Firmware Version'" -ForegroundColor Gray
Write-Host ""
