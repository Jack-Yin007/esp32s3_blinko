#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
情绪系统测试脚本
测试所有5个情绪区间的切换和动作组合加载
"""

import asyncio
from bleak import BleakClient

# 设备地址
DEVICE_ADDRESS = "90:E5:B1:AE:E4:56"

# UUID
SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
CONTROL_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NOTIFY_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# 情绪区间定义
ZONES = {
    0x00: "S-亢奋",
    0x01: "A-兴奋",
    0x02: "B-积极",
    0x03: "C-日常",
    0x04: "D-消极"
}

async def test_emotion_zones():
    """测试所有情绪区间"""
    print("\n" + "="*60)
    print("  情绪系统测试")
    print("="*60)
    
    async with BleakClient(DEVICE_ADDRESS, timeout=15.0) as client:
        print(f"✓ 已连接到 {DEVICE_ADDRESS}")
        
        # 订阅通知
        await client.start_notify(NOTIFY_CHAR_UUID, lambda s, d: print(f"  收到通知: {d.hex()}"))
        await asyncio.sleep(0.5)
        
        # 测试所有情绪区间
        for zone_id, zone_name in ZONES.items():
            print(f"\n【测试区间 {zone_name}】")
            command = bytes([0x10, zone_id])
            
            await client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
            print(f"  ✓ 发送命令: {command.hex()}")
            
            await asyncio.sleep(2)
            print(f"  ✓ 请查看ESP32 Monitor日志确认：")
            print(f"     - 情绪区间已切换到: {zone_name}")
            print(f"     - 加载了对应的动作组合")
            print(f"     - 状态已保存到NVS")
        
        # 停止通知
        await client.stop_notify(NOTIFY_CHAR_UUID)
        
    print("\n" + "="*60)
    print("  测试完成")
    print("="*60)
    print("\n请检查ESP32 Monitor日志中的以下信息：")
    print("  1. Emotion zone set to: 0xXX")
    print("  2. Loaded X action combos for zone XXXX")
    print("  3. Emotion state saved to NVS")
    print()

if __name__ == "__main__":
    try:
        asyncio.run(test_emotion_zones())
    except KeyboardInterrupt:
        print("\n\n测试已取消")
    except Exception as e:
        print(f"\n\n错误: {e}")
        import traceback
        traceback.print_exc()
