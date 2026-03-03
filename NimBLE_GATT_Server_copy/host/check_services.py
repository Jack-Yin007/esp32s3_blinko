#!/usr/bin/env python3
"""检查BLE设备的服务和特征值"""
import asyncio
from bleak import BleakClient

async def check_services():
    address = "90:E5:B1:AE:E4:56"
    print(f"连接到 {address}...")
    
    async with BleakClient(address) as client:
        print("✅ 连接成功\n")
        print("=" * 70)
        print("可用的服务和特征值:")
        print("=" * 70)
        
        for service in client.services:
            print(f"\n📦 服务: {service.uuid}")
            print(f"   描述: {service.description}")
            for char in service.characteristics:
                props = ", ".join(char.properties)
                print(f"   📝 特征值: {char.uuid}")
                print(f"      属性: {props}")

if __name__ == "__main__":
    asyncio.run(check_services())
