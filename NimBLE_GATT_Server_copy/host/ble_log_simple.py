#!/usr/bin/env python3
"""
BLE日志流快速查看器 - 简化版
直接连接Blinko并查看日志
"""

import asyncio
from bleak import BleakClient, BleakScanner

# BLE UUIDs
LOG_CONTROL_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_DATA_UUID = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"

LOG_CMD_STREAM_ON = 0x03
LOG_CMD_STREAM_OFF = 0x04

def on_log_received(sender, data: bytearray):
    """日志接收回调"""
    try:
        log = data.decode('utf-8', errors='replace').rstrip()
        if log:
            print(log)
    except:
        pass

async def main():
    print("🔍 扫描Blinko设备...")
    devices = await BleakScanner.discover(timeout=5.0)
    
    device = None
    for d in devices:
        if d.name and 'blinko' in d.name.lower():
            device = d
            print(f"✅ 发现: {d.name} ({d.address})")
            break
    
    if not device:
        print("❌ 未找到Blinko设备")
        return
    
    print(f"📱 连接中...")
    async with BleakClient(device.address) as client:
        print("✅ 已连接")
        
        # 订阅日志
        await client.start_notify(LOG_DATA_UUID, on_log_received)
        
        # 等待订阅生效
        await asyncio.sleep(0.3)
        
        # 启动流
        await client.write_gatt_char(LOG_CONTROL_UUID, bytes([LOG_CMD_STREAM_ON]), response=False)
        print("📡 日志流已启动 (Ctrl+C退出)\\n")
        print("="*60)
        
        try:
            # 保持连接
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\n⏸️  停止中...")
            await client.write_gatt_char(LOG_CONTROL_UUID, bytes([LOG_CMD_STREAM_OFF]), response=False)
            print("👋 已断开")

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
