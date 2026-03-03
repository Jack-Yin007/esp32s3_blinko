#!/usr/bin/env python3
"""
通过BLE触发ESP32设备自检
发送命令码 0x0009 到OTA控制特征
"""

import asyncio
import struct
from bleak import BleakClient, BleakScanner

# BLE UUID
OTA_CONTROL_CHAR_UUID = "00008022-0000-1000-8000-00805f9b34fb"

# 命令码
CMD_START_SELFTEST = 0x0009

DEVICE_NAME = "Blinko"

def crc16_ccitt(data: bytes) -> int:
    """计算CRC16-CCITT校验"""
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc

def build_command_packet(command_id: int, payload: bytes = b'') -> bytes:
    """构建20字节命令包"""
    packet = bytearray(20)
    
    # 命令ID（小端序，2字节）
    struct.pack_into('<H', packet, 0, command_id)
    
    # 载荷（最多16字节，不足补0）
    payload_len = min(len(payload), 16)
    packet[2:2+payload_len] = payload[:payload_len]
    
    # CRC16（前18字节的校验，小端序）
    crc = crc16_ccitt(bytes(packet[:18]))
    struct.pack_into('<H', packet, 18, crc)
    
    return bytes(packet)

async def trigger_selftest():
    print(f"🔍 扫描设备: {DEVICE_NAME}...")
    devices = await BleakScanner.discover(timeout=5.0)
    
    device = None
    for d in devices:
        if d.name and DEVICE_NAME in d.name:
            print(f"✅ 找到设备: {d.name} ({d.address})")
            device = d
            break
    
    if not device:
        print(f"❌ 未找到设备 '{DEVICE_NAME}'")
        return
    
    print(f"\n📡 连接到 {device.address}...")
    async with BleakClient(device.address) as client:
        print("✅ 已连接!")
        
        # 构建自检命令包
        cmd_packet = build_command_packet(CMD_START_SELFTEST)
        
        print(f"\n🔧 发送自检命令: 0x{CMD_START_SELFTEST:04X}")
        print(f"   命令包 (20字节): {cmd_packet.hex(' ')}")
        
        try:
            await client.write_gatt_char(OTA_CONTROL_CHAR_UUID, cmd_packet, response=False)
            print("✅ 命令发送成功!")
            print("\n" + "="*70)
            print("📋 自检已触发!")
            print("   设备将停止非必要任务，释放内存后执行自检")
            print("   请查看 ESP-IDF Monitor 终端查看详细测试结果")
            print("   自检完成后设备将在10秒后自动重启")
            print("="*70)
            
            # 等待命令被处理
            await asyncio.sleep(2)
            
        except Exception as e:
            print(f"❌ 发送命令时出错: {e}")
            import traceback
            traceback.print_exc()
            return
    
    print("\n✅ 完成! 请查看串口监视器查看测试结果")

if __name__ == "__main__":
    print("╔════════════════════════════════════════════════════════════════════╗")
    print("║          BLE自检触发器 - 通过BLE触发ESP32设备自检                  ║")
    print("║          Self-Test Trigger via BLE (Command 0x0009)               ║")
    print("╚════════════════════════════════════════════════════════════════════╝\n")
    
    try:
        asyncio.run(trigger_selftest())
    except KeyboardInterrupt:
        print("\n\n⚠️ 用户中断")
    except Exception as e:
        print(f"\n❌ 发生错误: {e}")
        import traceback
        traceback.print_exc()
