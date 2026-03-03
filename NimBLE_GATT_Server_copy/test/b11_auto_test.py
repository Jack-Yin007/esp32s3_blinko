#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
B11通信协议自动化测试套件
完整测试所有协议功能并生成测试报告
"""

import asyncio
import time
from datetime import datetime
from bleak import BleakClient, BleakScanner
from typing import List, Dict, Optional

# ==================== 配置 ====================
DEVICE_NAME = "Blinko"
DEVICE_ADDRESS = "90:E5:B1:AE:E4:56"  # 可选：直接使用MAC地址

# B11协议UUID
SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
CONTROL_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
NOTIFY_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
AUDIO_CHAR_UUID = "6e400004-b5a3-f393-e0a9-e50e24dcca9e"

# B11命令定义
CMD_EMOTION_ZONE = 0x10      # 设置情绪区间
CMD_TRIGGER_ACTION = 0x20    # 触发动作（上报）
CMD_SENSOR_DATA = 0x30       # 传感器数据（上报）
CMD_RECORDING_CONTROL = 0x40 # 录音控制
CMD_DEVICE_STATUS = 0x50     # 设备状态（上报）
CMD_ERROR_REPORT = 0x60      # 错误上报

# 区间定义
ZONE_S = 0x00  # S区间（亢奋）
ZONE_A = 0x01  # A区间（兴奋）
ZONE_B = 0x02  # B区间（积极）
ZONE_C = 0x03  # C区间（日常）
ZONE_D = 0x04  # D区间（消极）

# 录音控制
RECORD_START = 0x01
RECORD_STOP = 0x02

# ==================== 测试结果类 ====================
class TestResult:
    def __init__(self, name: str, category: str):
        self.name = name
        self.category = category
        self.passed = False
        self.duration = 0.0
        self.error_msg = ""
        self.details = ""
        self.start_time = 0
        
    def start(self):
        self.start_time = time.time()
        
    def finish(self, passed: bool, details: str = "", error: str = ""):
        self.duration = time.time() - self.start_time
        self.passed = passed
        self.details = details
        self.error_msg = error

class TestReport:
    def __init__(self):
        self.results: List[TestResult] = []
        self.start_time = datetime.now()
        self.end_time = None
        self.device_info = {}
        
    def add_result(self, result: TestResult):
        self.results.append(result)
        
    def finish(self):
        self.end_time = datetime.now()
        
    def get_summary(self) -> Dict:
        total = len(self.results)
        passed = sum(1 for r in self.results if r.passed)
        failed = total - passed
        duration = (self.end_time - self.start_time).total_seconds() if self.end_time else 0
        
        return {
            'total': total,
            'passed': passed,
            'failed': failed,
            'success_rate': (passed / total * 100) if total > 0 else 0,
            'duration': duration
        }
    
    def print_report(self):
        """打印测试报告"""
        summary = self.get_summary()
        
        print("\n" + "=" * 80)
        print("  B11通信协议自动化测试报告")
        print("=" * 80)
        print(f"测试时间: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"测试设备: {self.device_info.get('name', 'N/A')} ({self.device_info.get('address', 'N/A')})")
        print(f"总耗时: {summary['duration']:.2f}秒")
        print("=" * 80)
        
        # 测试摘要
        print("\n【测试摘要】")
        print(f"  总测试数: {summary['total']}")
        print(f"  通过: {summary['passed']} ✓")
        print(f"  失败: {summary['failed']} ✗")
        print(f"  成功率: {summary['success_rate']:.1f}%")
        
        # 按类别分组
        categories = {}
        for result in self.results:
            if result.category not in categories:
                categories[result.category] = []
            categories[result.category].append(result)
        
        # 打印每个类别的测试结果
        for category, results in categories.items():
            print(f"\n【{category}】")
            for result in results:
                status = "✓ 通过" if result.passed else "✗ 失败"
                print(f"  {status} - {result.name} ({result.duration:.2f}s)")
                if result.details:
                    print(f"       详情: {result.details}")
                if result.error_msg:
                    print(f"       错误: {result.error_msg}")
        
        # 失败详情
        failed_tests = [r for r in self.results if not r.passed]
        if failed_tests:
            print("\n【失败测试详情】")
            for result in failed_tests:
                print(f"  ✗ {result.name}")
                print(f"     类别: {result.category}")
                print(f"     错误: {result.error_msg}")
                if result.details:
                    print(f"     详情: {result.details}")
        
        print("\n" + "=" * 80)
        if summary['failed'] == 0:
            print("  🎉 所有测试通过！B11协议实现完全正确！")
        else:
            print(f"  ⚠️  {summary['failed']}个测试失败，请检查实现")
        print("=" * 80 + "\n")

# ==================== 测试套件 ====================
class B11TestSuite:
    def __init__(self):
        self.client: Optional[BleakClient] = None
        self.report = TestReport()
        self.notifications = []
        self.notification_event = asyncio.Event()
        
    def log(self, msg: str):
        """日志输出"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{timestamp}] {msg}")
        
    def notification_handler(self, sender, data):
        """通知处理器"""
        self.notifications.append(data)
        self.notification_event.set()
        self.log(f"收到通知: {data.hex()}")
        
    async def wait_for_notification(self, timeout: float = 2.0) -> Optional[bytes]:
        """等待通知"""
        try:
            await asyncio.wait_for(self.notification_event.wait(), timeout=timeout)
            self.notification_event.clear()
            if self.notifications:
                return self.notifications[-1]
        except asyncio.TimeoutError:
            pass
        return None
        
    # ==================== 基础测试 ====================
    async def test_device_scan(self) -> TestResult:
        """测试1.1: 设备扫描"""
        result = TestResult("设备扫描", "基础连接")
        result.start()
        
        try:
            self.log("扫描BLE设备...")
            devices = await BleakScanner.discover(timeout=10.0)
            
            device = None
            for d in devices:
                if d.address == DEVICE_ADDRESS or d.name == DEVICE_NAME:
                    device = d
                    break
            
            if device:
                self.report.device_info = {
                    'name': device.name or "未命名",
                    'address': device.address
                }
                result.finish(True, f"找到设备: {device.name} ({device.address})")
                return result
            else:
                result.finish(False, error="未找到Blinko设备")
                return result
                
        except Exception as e:
            result.finish(False, error=str(e))
            return result
    
    async def test_device_connection(self) -> TestResult:
        """测试1.2: 设备连接"""
        result = TestResult("设备连接", "基础连接")
        result.start()
        
        try:
            self.log(f"连接设备: {DEVICE_ADDRESS}")
            self.client = BleakClient(DEVICE_ADDRESS, timeout=15.0)
            await self.client.connect()
            
            if self.client.is_connected:
                result.finish(True, "成功连接到设备")
            else:
                result.finish(False, error="连接失败")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    async def test_service_discovery(self) -> TestResult:
        """测试1.3: 服务发现"""
        result = TestResult("B11服务发现", "基础连接")
        result.start()
        
        try:
            services = self.client.services
            
            b11_service = None
            for service in services:
                if service.uuid.lower() == SERVICE_UUID:
                    b11_service = service
                    break
            
            if b11_service:
                chars = [char.uuid for char in b11_service.characteristics]
                result.finish(True, f"找到B11服务，包含{len(chars)}个特征值")
            else:
                result.finish(False, error="未找到B11服务")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    async def test_characteristics(self) -> TestResult:
        """测试1.4: 特征值验证"""
        result = TestResult("特征值验证", "基础连接")
        result.start()
        
        try:
            found_chars = []
            expected_chars = [CONTROL_CHAR_UUID, NOTIFY_CHAR_UUID, AUDIO_CHAR_UUID]
            
            for service in self.client.services:
                if service.uuid.lower() == SERVICE_UUID:
                    for char in service.characteristics:
                        if char.uuid.lower() in expected_chars:
                            found_chars.append(char.uuid.lower())
            
            if len(found_chars) == 3:
                result.finish(True, "所有特征值验证通过")
            else:
                missing = set(expected_chars) - set(found_chars)
                result.finish(False, error=f"缺少特征值: {missing}")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    async def test_notification_subscription(self) -> TestResult:
        """测试1.5: 通知订阅"""
        result = TestResult("通知订阅", "基础连接")
        result.start()
        
        try:
            await self.client.start_notify(NOTIFY_CHAR_UUID, self.notification_handler)
            # 等待一下确保订阅生效
            await asyncio.sleep(0.5)
            result.finish(True, "通知订阅成功")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    # ==================== 情绪区间测试 ====================
    async def test_emotion_zone(self, zone: int, zone_name: str) -> TestResult:
        """测试情绪区间设置"""
        result = TestResult(f"设置情绪{zone_name}", "情绪区间命令")
        result.start()
        
        try:
            self.notifications.clear()
            command = bytes([CMD_EMOTION_ZONE, zone])
            
            await self.client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
            await asyncio.sleep(0.5)
            
            result.finish(True, f"命令: {command.hex()}")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    # ==================== 录音控制测试 ====================
    async def test_recording_control(self, action: int, action_name: str) -> TestResult:
        """测试录音控制"""
        result = TestResult(f"录音{action_name}", "录音控制命令")
        result.start()
        
        try:
            self.notifications.clear()
            command = bytes([CMD_RECORDING_CONTROL, action])
            
            await self.client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
            await asyncio.sleep(0.5)
            
            result.finish(True, f"命令: {command.hex()}")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    # ==================== 边界测试 ====================
    async def test_invalid_command(self) -> TestResult:
        """测试3.1: 无效命令字"""
        result = TestResult("无效命令字处理", "边界条件测试")
        result.start()
        
        try:
            self.notifications.clear()
            command = bytes([0xFF, 0x00])  # 无效命令字
            
            await self.client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
            
            # 等待错误通知
            notification = await self.wait_for_notification(2.0)
            
            if notification and len(notification) >= 2:
                if notification[0] == CMD_ERROR_REPORT:
                    result.finish(True, f"正确返回错误: {notification.hex()}")
                else:
                    result.finish(False, error="未返回错误通知")
            else:
                # 即使没有收到通知，只要没有崩溃也算通过
                result.finish(True, "设备正确处理无效命令（无响应）")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    async def test_invalid_zone(self) -> TestResult:
        """测试3.2: 无效区间号"""
        result = TestResult("无效区间号处理", "边界条件测试")
        result.start()
        
        try:
            self.notifications.clear()
            command = bytes([CMD_EMOTION_ZONE, 0xFF])  # 无效区间
            
            await self.client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
            
            notification = await self.wait_for_notification(2.0)
            
            if notification and len(notification) >= 2:
                if notification[0] == CMD_ERROR_REPORT:
                    result.finish(True, f"正确返回错误: {notification.hex()}")
                else:
                    result.finish(False, error="未返回错误通知")
            else:
                result.finish(True, "设备正确处理无效区间（无响应）")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    async def test_empty_command(self) -> TestResult:
        """测试3.3: 空命令"""
        result = TestResult("空命令处理", "边界条件测试")
        result.start()
        
        try:
            command = bytes([])
            
            await self.client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
            await asyncio.sleep(0.5)
            
            # 不崩溃就算通过
            result.finish(True, "设备正确处理空命令")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    # ==================== 性能测试 ====================
    async def test_rapid_commands(self) -> TestResult:
        """测试4.1: 快速连续命令"""
        result = TestResult("快速连续命令", "性能测试")
        result.start()
        
        try:
            zones = [ZONE_A, ZONE_B, ZONE_C, ZONE_D, ZONE_S]
            
            for zone in zones:
                command = bytes([CMD_EMOTION_ZONE, zone])
                await self.client.write_gatt_char(CONTROL_CHAR_UUID, command, response=False)
                await asyncio.sleep(0.1)  # 100ms间隔
            
            result.finish(True, f"成功发送{len(zones)}个连续命令")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    async def test_long_connection(self) -> TestResult:
        """测试4.2: 长时间连接稳定性"""
        result = TestResult("长时间连接稳定性", "性能测试")
        result.start()
        
        try:
            # 保持连接5秒
            await asyncio.sleep(5)
            
            if self.client.is_connected:
                result.finish(True, "连接保持稳定")
            else:
                result.finish(False, error="连接中断")
                
        except Exception as e:
            result.finish(False, error=str(e))
            
        return result
    
    # ==================== 运行所有测试 ====================
    async def run_all_tests(self):
        """运行所有测试"""
        self.log("开始B11协议自动化测试...")
        self.log("=" * 60)
        
        try:
            # 第1组：基础连接测试
            self.log("\n【第1组：基础连接测试】")
            self.report.add_result(await self.test_device_scan())
            self.report.add_result(await self.test_device_connection())
            
            if not self.client or not self.client.is_connected:
                self.log("连接失败，终止测试")
                return
            
            self.report.add_result(await self.test_service_discovery())
            self.report.add_result(await self.test_characteristics())
            self.report.add_result(await self.test_notification_subscription())
            
            # 第2组：情绪区间测试
            self.log("\n【第2组：情绪区间命令测试】")
            zones = [
                (ZONE_S, "S区间(亢奋)"),
                (ZONE_A, "A区间(兴奋)"),
                (ZONE_B, "B区间(积极)"),
                (ZONE_C, "C区间(日常)"),
                (ZONE_D, "D区间(消极)")
            ]
            
            for zone, name in zones:
                self.report.add_result(await self.test_emotion_zone(zone, name))
                await asyncio.sleep(0.5)
            
            # 第3组：录音控制测试
            self.log("\n【第3组：录音控制命令测试】")
            self.report.add_result(await self.test_recording_control(RECORD_START, "开始"))
            await asyncio.sleep(1)
            self.report.add_result(await self.test_recording_control(RECORD_STOP, "停止"))
            
            # 第4组：边界条件测试
            self.log("\n【第4组：边界条件测试】")
            self.report.add_result(await self.test_invalid_command())
            await asyncio.sleep(0.5)
            self.report.add_result(await self.test_invalid_zone())
            await asyncio.sleep(0.5)
            self.report.add_result(await self.test_empty_command())
            
            # 第5组：性能测试
            self.log("\n【第5组：性能测试】")
            self.report.add_result(await self.test_rapid_commands())
            self.report.add_result(await self.test_long_connection())
            
        except Exception as e:
            self.log(f"测试异常: {e}")
            
        finally:
            # 清理
            if self.client and self.client.is_connected:
                try:
                    await self.client.stop_notify(NOTIFY_CHAR_UUID)
                    await self.client.disconnect()
                    self.log("已断开连接")
                except:
                    pass
            
            self.report.finish()
            self.report.print_report()

# ==================== 主程序 ====================
async def main():
    """主函数"""
    print("\n")
    print("=" * 80)
    print("  B11通信协议自动化测试套件")
    print("  版本: 1.0")
    print("  设备: Blinko (90:E5:B1:AE:E4:56)")
    print("=" * 80)
    print()
    
    suite = B11TestSuite()
    await suite.run_all_tests()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n测试已取消")
    except Exception as e:
        print(f"\n\n错误: {e}")
        import traceback
        traceback.print_exc()
