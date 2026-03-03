#!/usr/bin/env python3
"""
BLE设备扫描器 - 显示所有Blinko设备及其MAC地址
"""

import asyncio
from bleak import BleakScanner

async def scan_blinko_devices():
    print("🔍 扫描所有Blinko设备...")
    print("=" * 70)
    
    devices = await BleakScanner.discover(timeout=10.0)
    
    blinko_devices = [d for d in devices if d.name and 'Blinko' in d.name]
    
    if not blinko_devices:
        print("❌ 未找到任何Blinko设备")
        return
    
    # 按信号强度排序（RSSI越大越强，-40 > -60 > -80）
    def get_rssi(device):
        if hasattr(device, 'rssi') and device.rssi is not None:
            return device.rssi
        return -999  # 未知RSSI放在最后
    
    blinko_devices.sort(key=get_rssi, reverse=True)  # 降序排列
    
    print(f"✅ 找到 {len(blinko_devices)} 个Blinko设备 (按信号强度排序):\n")
    
    for i, device in enumerate(blinko_devices, 1):
        rssi = device.rssi if hasattr(device, 'rssi') and device.rssi is not None else None
        
        # 信号强度指示器
        signal_emoji = "📶"
        if rssi is not None:
            if rssi > -50:
                signal_emoji = "📶📶📶"  # 强
            elif rssi > -70:
                signal_emoji = "📶📶"    # 中
            else:
                signal_emoji = "📶"      # 弱
        
        print(f"[{i}] {device.name}")
        print(f"    📍 MAC地址: {device.address}")
        if rssi is not None:
            print(f"    {signal_emoji} 信号强度: {rssi} dBm")
        else:
            print(f"    📶 信号强度: 未知")
        print(f"    🔗 连接命令: python ble_log_viewer.py --address {device.address} --download")
        print()
    
    print("=" * 70)
    print("💡 提示：信号强度越高（数值越小的负数），设备越近")
    print("   例如: -40 dBm > -60 dBm > -80 dBm")

if __name__ == '__main__':
    asyncio.run(scan_blinko_devices())
