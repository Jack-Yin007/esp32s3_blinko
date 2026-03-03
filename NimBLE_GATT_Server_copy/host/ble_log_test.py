#!/usr/bin/env python3
"""简化的BLE日志测试脚本 - 用于诊断连接问题"""

import asyncio
from bleak import BleakClient, BleakScanner

LOG_SERVICE_UUID     = "6E400010-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_SIZE_CHAR_UUID   = "6E400011-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_DATA_CHAR_UUID   = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_CONTROL_CHAR_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E"

async def test_connection():
    print("🔍 扫描设备 'Blinko'...")
    device = await BleakScanner.find_device_by_name("Blinko", timeout=10.0)
    
    if not device:
        print("❌ 未找到设备")
        return
    
    print(f"✅ 发现设备: {device.name} ({device.address})")
    
    async with BleakClient(device) as client:
        print(f"🔗 已连接: {client.is_connected}")
        
        # 列出所有服务和特征
        print("\n📋 服务列表:")
        for service in client.services:
            print(f"  Service: {service.uuid}")
            for char in service.characteristics:
                print(f"    Char: {char.uuid} - {char.properties}")
        
        # 检查LOG服务
        print(f"\n🔍 查找LOG服务: {LOG_SERVICE_UUID}")
        log_service = None
        for service in client.services:
            if service.uuid.lower() == LOG_SERVICE_UUID.lower():
                log_service = service
                print(f"✅ 找到LOG服务")
                break
        
        if not log_service:
            print("❌ 未找到LOG服务")
            return
        
        # 读取日志大小
        print(f"\n📊 读取日志大小...")
        try:
            size_data = await client.read_gatt_char(LOG_SIZE_CHAR_UUID)
            log_size = int.from_bytes(size_data, byteorder='little')
            print(f"✅ 日志大小: {log_size} 字节")
        except Exception as e:
            print(f"❌ 读取日志大小失败: {e}")
        
        # 测试批量读取日志
        print(f"\n📥 测试批量读取日志...")
        
        try:
            # 发送READ_START命令重置偏移
            print("发送READ_START命令...")
            await client.write_gatt_char(LOG_CONTROL_CHAR_UUID, bytes([0x05]))
            await asyncio.sleep(0.1)
            
            # 分块读取日志
            total_read = 0
            chunk_count = 0
            
            print(f"开始读取 {log_size} 字节日志...\n")
            
            while total_read < log_size and chunk_count < 50:  # 最多读取50个块
                chunk = await client.read_gatt_char(LOG_DATA_CHAR_UUID)
                chunk_count += 1
                
                if not chunk:
                    break
                
                # 打印日志内容（彩色）
                log_text = chunk.decode('utf-8', errors='replace')
                for line in log_text.split('\n'):
                    if line.strip():
                        # 检测日志等级并着色
                        if len(line) > 2 and line[1] == ' ':
                            level = line[0]
                            colors = {'E': '\033[91m', 'W': '\033[93m', 'I': '\033[92m', 'D': '\033[94m', 'V': '\033[95m'}
                            color = colors.get(level, '')
                            print(f"{color}{line}\033[0m")
                        else:
                            print(line)
                
                total_read += len(chunk)
                
                # 如果读到的数据少于512字节，说明到末尾了
                if len(chunk) < 512:
                    break
            
            print(f"\n✅ 读取完成: {total_read} 字节，{chunk_count} 个块")
            
        except Exception as e:
            print(f"❌ 读取失败: {e}")

if __name__ == '__main__':
    asyncio.run(test_connection())
