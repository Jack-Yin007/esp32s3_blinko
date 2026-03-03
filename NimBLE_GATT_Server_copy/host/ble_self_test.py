#!/usr/bin/env python3
"""
BLE Self-Test Trigger Script
触发ESP32设备自检并通过BLE Log实时查看结果

Usage:
    python ble_self_test.py [device_name]
    
Example:
    python ble_self_test.py B11_Audio_OTA
    python ble_self_test.py  # 使用默认设备名
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner
import argparse

# BLE服务和特征UUID (Nordic UART Service)
SERVICE_UUID = "6e400010-b5a3-f393-e0a9-e50e24dcca9e"
LOG_SIZE_UUID = "6e400011-b5a3-f393-e0a9-e50e24dcca9e"  # READ - 日志大小
LOG_DATA_UUID = "6e400012-b5a3-f393-e0a9-e50e24dcca9e"  # NOTIFY - 日志数据流
LOG_CTRL_UUID = "6e400013-b5a3-f393-e0a9-e50e24dcca9e"  # WRITE - 控制

# 命令服务UUID
CMD_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
CMD_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

# 默认设备名
DEFAULT_DEVICE_NAME = "B11_Audio_OTA"

# 自检命令
CMD_SELF_TEST = 0x80
ACTION_START = 0x01

class BLESelfTest:
    def __init__(self, device_name=DEFAULT_DEVICE_NAME):
        self.device_name = device_name
        self.client = None
        self.test_complete = asyncio.Event()
        self.log_lines = []
        
    async def find_device(self):
        """扫描并查找目标设备"""
        print(f"🔍 Scanning for device: {self.device_name}...")
        devices = await BleakScanner.discover(timeout=5.0)
        
        for device in devices:
            if device.name and self.device_name in device.name:
                print(f"✅ Found device: {device.name} ({device.address})")
                return device.address
        
        print(f"❌ Device '{self.device_name}' not found")
        return None
    
    def log_notification_handler(self, sender, data):
        """处理BLE日志通知"""
        try:
            log_line = data.decode('utf-8', errors='ignore').strip()
            if log_line:
                print(log_line)
                self.log_lines.append(log_line)
                
                # 检测测试完成标志
                if "Self-Test Complete Report" in log_line or "╚════════════════════════════════════════╝" in log_line:
                    # 等待报告完全输出
                    asyncio.create_task(self.delayed_complete())
        except Exception as e:
            print(f"⚠️  Log decode error: {e}")
    
    async def delayed_complete(self):
        """延迟设置完成标志，确保所有日志输出"""
        await asyncio.sleep(2.0)
        self.test_complete.set()
    
    async def trigger_self_test(self):
        """触发设备自检"""
        address = await self.find_device()
        if not address:
            return False
        
        print(f"\n📡 Connecting to {address}...")
        
        try:
            async with BleakClient(address, timeout=10.0) as client:
                self.client = client
                print(f"✅ Connected!")
                
                # 订阅日志通知
                print("📝 Subscribing to log stream...")
                await client.start_notify(LOG_DATA_UUID, self.log_notification_handler)
                print("✅ Log stream active\n")
                
                # 等待欢迎消息
                await asyncio.sleep(1.0)
                
                # 发送自检命令: 0x80 0x01
                cmd = bytes([CMD_SELF_TEST, ACTION_START])
                print(f"🔧 Triggering self-test (0x{CMD_SELF_TEST:02X} 0x{ACTION_START:02X})...")
                await client.write_gatt_char(LOG_CTRL_UUID, cmd, response=False)
                print("✅ Self-test command sent\n")
                print("=" * 60)
                
                # 等待测试完成（最多60秒）
                try:
                    await asyncio.wait_for(self.test_complete.wait(), timeout=60.0)
                    print("=" * 60)
                    print("✅ Self-test completed!")
                except asyncio.TimeoutError:
                    print("=" * 60)
                    print("⚠️  Timeout waiting for test completion")
                
                # 停止通知
                await client.stop_notify(LOG_DATA_UUID)
                
                return True
                
        except Exception as e:
            print(f"❌ Error: {e}")
            import traceback
            traceback.print_exc()
            return False
    
    def print_summary(self):
        """打印测试摘要"""
        if not self.log_lines:
            return
        
        print("\n" + "=" * 60)
        print("📊 TEST SUMMARY")
        print("=" * 60)
        
        # 提取关键信息
        passed = 0
        failed = 0
        warnings = 0
        
        for line in self.log_lines:
            if "Passed:" in line:
                try:
                    passed = int(line.split("Passed:")[1].split("devices")[0].strip())
                except:
                    pass
            elif "Failed:" in line:
                try:
                    failed = int(line.split("Failed:")[1].split("devices")[0].strip())
                except:
                    pass
            elif "Warnings:" in line:
                try:
                    warnings = int(line.split("Warnings:")[1].split("devices")[0].strip())
                except:
                    pass
        
        total = passed + failed + warnings
        if total > 0:
            print(f"✅ Passed:   {passed}/{total} ({passed*100//total}%)")
            print(f"❌ Failed:   {failed}/{total} ({failed*100//total}%)")
            print(f"⚠️  Warnings: {warnings}/{total} ({warnings*100//total}%)")
        
        print("=" * 60)

async def main():
    parser = argparse.ArgumentParser(description='Trigger BLE device self-test')
    parser.add_argument('device', nargs='?', default=DEFAULT_DEVICE_NAME,
                       help=f'Device name (default: {DEFAULT_DEVICE_NAME})')
    args = parser.parse_args()
    
    print("╔════════════════════════════════════════════════════════╗")
    print("║       BLE Device Self-Test Trigger                    ║")
    print("╚════════════════════════════════════════════════════════╝\n")
    
    tester = BLESelfTest(args.device)
    success = await tester.trigger_self_test()
    
    if success:
        tester.print_summary()
        print("\n✅ Self-test session completed successfully")
        return 0
    else:
        print("\n❌ Self-test session failed")
        return 1

if __name__ == "__main__":
    try:
        exit_code = asyncio.run(main())
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        sys.exit(130)
