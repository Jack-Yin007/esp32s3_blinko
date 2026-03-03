#!/usr/bin/env python3
"""测试BLE文件系统浏览服务"""
import asyncio
from bleak import BleakClient

# BLE FS Service UUIDs
FS_SERVICE_UUID = "6e400020-b5a3-f393-e0a9-e50e24dcca9e"
FS_PATH_CHAR = "6e400021-b5a3-f393-e0a9-e50e24dcca9e"   # Write
FS_DATA_CHAR = "6e400022-b5a3-f393-e0a9-e50e24dcca9e"   # Read
FS_CONTROL_CHAR = "6e400023-b5a3-f393-e0a9-e50e24dcca9e" # Write

# Commands
FS_CMD_LIST = 0x01

async def list_directory(path="/flash/audio"):
    """列出目录内容"""
    address = "90:E5:B1:AE:E4:56"
    
    print(f"连接到 {address}...")
    async with BleakClient(address) as client:
        print("✅ 连接成功\n")
        
        # 1. 写入路径
        print(f"📝 设置路径: {path}")
        await client.write_gatt_char(FS_PATH_CHAR, path.encode('utf-8'), response=False)
        await asyncio.sleep(0.1)
        
        # 2. 发送LIST命令
        print("🔍 发送LIST命令")
        await client.write_gatt_char(FS_CONTROL_CHAR, bytes([FS_CMD_LIST]), response=False)
        await asyncio.sleep(0.2)
        
        # 3. 读取结果
        print("📥 读取目录列表\n")
        data = await client.read_gatt_char(FS_DATA_CHAR)
        
        if not data or len(data) == 0:
            print("❌ 没有数据返回")
            return
        
        # 解析目录列表
        count = data[0]
        print(f"找到 {count} 个条目:\n")
        print("=" * 60)
        
        offset = 1
        for i in range(count):
            # 读取文件名（以null结尾）
            name_end = data.index(b'\x00', offset)
            name = data[offset:name_end].decode('utf-8')
            offset = name_end + 1
            
            # 读取大小（4字节，little-endian）
            size = int.from_bytes(data[offset:offset+4], 'little')
            offset += 4
            
            # 读取类型（1字节：0=文件，1=目录）
            is_dir = data[offset]
            offset += 1
            
            type_icon = "📁" if is_dir else "📄"
            type_str = "DIR" if is_dir else "FILE"
            print(f"{type_icon} {name:30s} {type_str:6s} {size:10d} bytes")
        
        print("=" * 60)

if __name__ == "__main__":
    asyncio.run(list_directory("/flash/audio"))
