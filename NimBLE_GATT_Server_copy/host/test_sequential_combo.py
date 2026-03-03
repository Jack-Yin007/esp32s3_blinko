#!/usr/bin/env python3
"""
简化的组合动作顺序测试脚本
用于测试ESP32设备是否正确实现了顺序执行组合动作
"""

import asyncio
import sys
import struct
import time
from bleak import BleakClient, BleakScanner
from datetime import datetime

# BLE服务和特征UUID
OTA_SERVICE_UUID = "00008018-0000-1000-8000-00805f9b34fb"
OTA_CONTROL_UUID = "00008022-0000-1000-8000-00805f9b34fb"

LOG_SERVICE_UUID = "6E400010-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_DATA_UUID = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"

# 命令定义
COMBO_TEST_CMD = 0x000A  # 组合动作测试命令

class Colors:
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    RED = '\033[91m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    END = '\033[0m'

def log(message, color=Colors.BLUE):
    timestamp = datetime.now().strftime("%H:%M:%S")
    print(f"{color}[{timestamp}] {message}{Colors.END}")

class SequentialComboTester:
    def __init__(self):
        self.client = None
        self.device_name = "ESP32"
        
    async def scan_for_device(self):
        """扫描Blinko设备"""
        log("🔍 扫描Blinko设备...", Colors.CYAN)
        
        devices = await BleakScanner.discover(timeout=10.0)
        target_devices = []
        
        for device in devices:
            if device.name and ("Blinko" in device.name or "ESP32" in device.name):
                target_devices.append(device)
                log(f"📱 找到设备: {device.name} ({device.address})", Colors.GREEN)
        
        return target_devices
    
    async def connect_to_device(self, device):
        """连接到设备"""
        log(f"🔗 连接设备: {device.name}", Colors.YELLOW)
        
        self.client = BleakClient(device.address)
        await self.client.connect()
        
        log("✅ 连接成功", Colors.GREEN)
        return True
    
    async def setup_notifications(self):
        """设置日志通知"""
        try:
            def log_notification_handler(characteristic, data):
                try:
                    message = data.decode('utf-8').strip()
                    if message:
                        # 检测关键信息并用不同颜色显示
                        if "🎯" in message or "开始" in message:
                            log(f"📢 {message}", Colors.GREEN)
                        elif "✅" in message or "完成" in message:
                            log(f"📢 {message}", Colors.CYAN)
                        elif "⚡" in message or "执行" in message:
                            log(f"📢 {message}", Colors.YELLOW)
                        else:
                            log(f"📢 {message}")
                except:
                    log(f"📢 [二进制数据: {len(data)} bytes]")
            
            await self.client.start_notify(LOG_DATA_UUID, log_notification_handler)
            log("✅ 日志通知已启用", Colors.GREEN)
            
        except Exception as e:
            log(f"⚠️ 日志通知设置失败: {e}", Colors.RED)
    
    async def send_sequential_test_command(self):
        """发送顺序测试命令"""
        log("📤 发送顺序组合动作测试命令...", Colors.YELLOW)
        
        # 构造命令包
        # 命令格式: [CMD(2)][FLAGS(1)][DURATION(2)][INTERVAL(2)][PADDING(13)]
        command = struct.pack('<H', COMBO_TEST_CMD)  # 命令ID: 0x000A (2字节)
        command += struct.pack('<B', 0x04)           # FLAGS: 显示详细信息 (1字节)
        command += struct.pack('<H', 3000)          # 动作持续时间: 3秒 (2字节)
        command += struct.pack('<H', 1000)          # 间隔时间: 1秒 (2字节)
        
        # 添加填充字节达到20字节
        command += b'\x00' * (20 - len(command))
        
        await self.client.write_gatt_char(OTA_CONTROL_UUID, command, response=False)
        log("✅ 命令发送成功", Colors.GREEN)
    
    async def monitor_test(self, duration=180):
        """监控测试执行"""
        log(f"⏰ 监控测试执行 ({duration}秒)...", Colors.CYAN)
        
        start_time = time.time()
        
        while time.time() - start_time < duration:
            await asyncio.sleep(1)
        
        log("⏰ 监控结束", Colors.YELLOW)
    
    async def cleanup(self):
        """清理资源"""
        if self.client and self.client.is_connected:
            try:
                await self.client.stop_notify(LOG_DATA_UUID)
            except:
                pass
            try:
                await self.client.disconnect()
                log("🔚 连接已断开", Colors.CYAN)
            except:
                pass

async def main():
    tester = SequentialComboTester()
    
    try:
        # 扫描设备
        devices = await tester.scan_for_device()
        if not devices:
            log("❌ 未找到ESP32设备", Colors.RED)
            return
        
        # 连接第一个设备
        device = devices[0]
        await tester.connect_to_device(device)
        
        # 设置通知
        await tester.setup_notifications()
        
        # 等待连接稳定
        await asyncio.sleep(2)
        
        # 显示测试信息
        print("\n" + "="*60)
        log("🎭 ESP32组合动作顺序测试", Colors.BOLD + Colors.CYAN)
        log("📋 测试说明: 设备将按S→A→B→C→D顺序执行所有组合动作", Colors.BLUE)
        log("⏱️ 每个动作持续3秒，间隔1秒", Colors.BLUE)
        print("="*60 + "\n")
        
        # 询问是否继续
        try:
            response = input("是否开始测试? (Y/n): ")
            if response.lower() in ['n', 'no']:
                log("❌ 测试已取消", Colors.YELLOW)
                return
        except (KeyboardInterrupt, EOFError):
            log("\n❌ 测试已取消", Colors.YELLOW)
            return
        
        # 发送测试命令
        await tester.send_sequential_test_command()
        
        # 监控测试
        await tester.monitor_test(duration=180)  # 3分钟监控
        
    except KeyboardInterrupt:
        log("\n⏹️ 用户中断测试", Colors.YELLOW)
    except Exception as e:
        log(f"❌ 测试错误: {e}", Colors.RED)
    finally:
        await tester.cleanup()

if __name__ == "__main__":
    if sys.platform == "win32":
        asyncio.set_event_loop_policy(asyncio.WindowsProactorEventLoopPolicy())
    
    print(f"{Colors.BOLD}{Colors.CYAN}")
    print("="*60)
    print("🎭  ESP32组合动作顺序测试工具  🎭")
    print("="*60)
    print(f"{Colors.END}")
    
    asyncio.run(main())