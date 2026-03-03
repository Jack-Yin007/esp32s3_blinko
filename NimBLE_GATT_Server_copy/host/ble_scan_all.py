#!/usr/bin/env python3
"""
BLE设备扫描器 - 显示所有附近的BLE设备
"""

import asyncio
from bleak import BleakScanner

async def scan_all_devices():
    print("🔍 扫描所有BLE设备 (15秒)...")
    print("=" * 70)
    
    devices = await BleakScanner.discover(timeout=15.0)
    
    if not devices:
        print("❌ 未找到任何BLE设备")
        print("\n可能的原因:")
        print("1. 蓝牙适配器未启用")
        print("2. 附近没有BLE设备")
        print("3. Windows 蓝牙服务未运行")
        return
    
    # 按信号强度排序
    def get_rssi(device):
        if hasattr(device, 'rssi') and device.rssi is not None:
            return device.rssi
        return -999
    
    devices_sorted = sorted(devices, key=get_rssi, reverse=True)
    
    print(f"✅ 找到 {len(devices_sorted)} 个BLE设备:\n")
    
    for i, device in enumerate(devices_sorted, 1):
        name = device.name if device.name else "(未命名)"
        rssi = device.rssi if hasattr(device, 'rssi') and device.rssi is not None else None
        
        # 信号强度指示器
        signal_emoji = "📶"
        if rssi is not None:
            if rssi > -50:
                signal_emoji = "📶📶📶"
            elif rssi > -70:
                signal_emoji = "📶📶"
            else:
                signal_emoji = "📶"
        
        # 标记可能是ESP32的设备
        marker = ""
        if name and any(keyword in name.lower() for keyword in ['esp32', 'nimble', 'blinko', 'esp']):
            marker = " ⭐ [可能是目标设备]"
        
        print(f"[{i}] {name}{marker}")
        print(f"    📍 MAC: {device.address}")
        if rssi is not None:
            print(f"    {signal_emoji} 信号: {rssi} dBm")
        else:
            print(f"    📶 信号: 未知")
        print()
    
    print("=" * 70)
    print("💡 提示：查找设备名称包含 'ESP32', 'nimble' 或 'Blinko' 的设备")

if __name__ == '__main__':
    asyncio.run(scan_all_devices())
