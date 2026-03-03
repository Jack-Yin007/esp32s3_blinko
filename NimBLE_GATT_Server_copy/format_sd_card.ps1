# SD Card FAT32 Format Script
# Run as Administrator

Write-Host "🔍 检查SD卡..." -ForegroundColor Yellow

# Get USB disk (SD card reader)
$disk = Get-Disk | Where-Object {$_.BusType -eq 'USB'}

if (-not $disk) {
    Write-Host "❌ 未发现USB存储设备（SD卡读卡器）" -ForegroundColor Red
    exit 1
}

Write-Host "✅ 发现设备: $($disk.FriendlyName)" -ForegroundColor Green
Write-Host "   磁盘编号: $($disk.Number)" -ForegroundColor Cyan
Write-Host "   容量: $([math]::Round($disk.Size / 1GB, 2)) GB" -ForegroundColor Cyan

# Confirm before formatting
Write-Host "`n⚠️  警告: 此操作将清除SD卡上的所有数据！" -ForegroundColor Yellow
$confirm = Read-Host "确认格式化磁盘 $($disk.Number) 为FAT32文件系统? (yes/no)"

if ($confirm -ne 'yes') {
    Write-Host "❌ 操作已取消" -ForegroundColor Red
    exit 0
}

Write-Host "`n🔧 开始格式化..." -ForegroundColor Yellow

try {
    # Step 1: Clear disk
    Write-Host "1️⃣ 清除磁盘..." -ForegroundColor Cyan
    Clear-Disk -Number $disk.Number -RemoveData -Confirm:$false
    
    # Step 2: Initialize disk with MBR
    Write-Host "2️⃣ 初始化磁盘 (MBR)..." -ForegroundColor Cyan
    Initialize-Disk -Number $disk.Number -PartitionStyle MBR
    
    # Step 3: Create partition
    Write-Host "3️⃣ 创建分区..." -ForegroundColor Cyan
    $partition = New-Partition -DiskNumber $disk.Number -UseMaximumSize -IsActive
    
    # Step 4: Format as FAT32
    Write-Host "4️⃣ 格式化为FAT32..." -ForegroundColor Cyan
    Format-Volume -Partition $partition -FileSystem FAT32 -NewFileSystemLabel "SD_CARD" -Confirm:$false
    
    # Step 5: Assign drive letter
    Write-Host "5️⃣ 分配驱动器号..." -ForegroundColor Cyan
    $partition | Add-PartitionAccessPath -AssignDriveLetter
    
    # Get drive letter
    $driveLetter = (Get-Partition -DiskNumber $disk.Number | Where-Object {$_.DriveLetter}).DriveLetter
    
    Write-Host "`n✅ SD卡格式化完成！" -ForegroundColor Green
    Write-Host "   文件系统: FAT32" -ForegroundColor Green
    Write-Host "   驱动器号: ${driveLetter}:" -ForegroundColor Green
    Write-Host "   卷标: SD_CARD" -ForegroundColor Green
    
    Write-Host "`n🎉 操作完成！SD卡已准备就绪。" -ForegroundColor Green
    
} catch {
    Write-Host "`n❌ 格式化失败: $_" -ForegroundColor Red
    exit 1
}
