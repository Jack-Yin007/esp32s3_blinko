#!/usr/bin/env python3
"""测试BLE文件上传"""
import asyncio
from bleak import BleakScanner

async def scan():
    print("扫描BLE设备...")
    devices = await BleakScanner.discover(timeout=10.0)
    
    print(f"\n找到 {len(devices)} 个设备:")
    for device in devices:
        if device.name:
            print(f"  - {device.name}: {device.address}")

if __name__ == "__main__":
    asyncio.run(scan())
