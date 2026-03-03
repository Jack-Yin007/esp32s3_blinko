#!/usr/bin/env python3
"""简单测试上传单个文件"""
import asyncio
import struct
from bleak import BleakClient

ADDRESS = "90:E5:B1:AE:E4:56"
FS_PATH_CHR = "6e400021-b5a3-f393-e0a9-e50e24dcca9e"
FS_DATA_CHR = "6e400022-b5a3-f393-e0a9-e50e24dcca9e"
FS_CTRL_CHR = "6e400023-b5a3-f393-e0a9-e50e24dcca9e"

CMD_MKDIR = 0x05
CMD_WRITE = 0x04

async def upload_test():
    print(f"连接到 {ADDRESS}...")
    async with BleakClient(ADDRESS, timeout=15.0) as client:
        print("✅ 已连接")
        
        # 等待服务发现
        print("等待服务发现...")
        await asyncio.sleep(1.0)
        print("开始上传\n")
        
        # 1. 创建目录
        print("📁 创建目录 /flash/test")
        await client.write_gatt_char(FS_PATH_CHR, b"/flash/test")
        await asyncio.sleep(0.1)
        await client.write_gatt_char(FS_CTRL_CHR, bytes([CMD_MKDIR]))
        await asyncio.sleep(0.3)
        
        # 2. 写入测试文件
        test_path = "/flash/test/hello.txt"
        test_data = b"Hello from BLE!\nThis is a test file.\n"
        
        print(f"\n📝 写入文件: {test_path}")
        print(f"   数据大小: {len(test_data)} bytes")
        
        # 设置路径
        await client.write_gatt_char(FS_PATH_CHR, test_path.encode('utf-8'))
        await asyncio.sleep(0.1)
        
        # 发送WRITE命令
        await client.write_gatt_char(FS_CTRL_CHR, bytes([CMD_WRITE]))
        await asyncio.sleep(0.1)
        
        # 发送文件大小
        size_data = struct.pack('<I', len(test_data))
        await client.write_gatt_char(FS_DATA_CHR, size_data)
        await asyncio.sleep(0.1)
        
        # 发送数据
        await client.write_gatt_char(FS_DATA_CHR, test_data)
        await asyncio.sleep(0.5)
        
        print("✅ 上传完成！")
        print("\n请检查串口监控器查看日志")

if __name__ == "__main__":
    asyncio.run(upload_test())
