"""
GATT Notification Monitor - 通用BLE通知监听工具
用于测试ESP32主动推送的各种事件和状态更新

功能：
- 监听所有支持notify/indicate的特征值
- 实时显示通知内容
- 解析已知协议的通知数据
- 可选择性订阅特定服务
"""

import asyncio
import struct
import time
from datetime import datetime
from typing import Dict, List, Optional, Callable
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
import logging

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class GATTNotificationMonitor:
    """GATT通知监听器"""
    
    # 已知的服务和特征UUID
    KNOWN_SERVICES = {
        "6e400001-b5a3-f393-e0a9-e50e24dcca9e": "B11 Main Service",
        "00008018-0000-1000-8000-00805f9b34fb": "OTA Service",
        "0000180f-0000-1000-8000-00805f9b34fb": "Battery Service",
    }
    
    KNOWN_CHARACTERISTICS = {
        # B11宠物服务特征
        "6e400002-b5a3-f393-e0a9-e50e24dcca9e": "B11 TX (设备→手机)",
        "6e400003-b5a3-f393-e0a9-e50e24dcca9e": "B11 RX (手机→设备)",
        "6e400004-b5a3-f393-e0a9-e50e24dcca9e": "B11 Audio",
        "6e400005-b5a3-f393-e0a9-e50e24dcca9e": "B11 Control",
        "6e400006-b5a3-f393-e0a9-e50e24dcca9e": "B11 NFC",
        
        # OTA服务特征
        "00008020-0000-1000-8000-00805f9b34fb": "OTA Firmware RX",
        "00008021-0000-1000-8000-00805f9b34fb": "OTA Progress",
        "00008022-0000-1000-8000-00805f9b34fb": "OTA Command",
        
        # 电池服务
        "00002a19-0000-1000-8000-00805f9b34fb": "Battery Level",
    }
    
    # B11协议命令定义
    B11_COMMANDS = {
        0x01: "获取设备状态",
        0x02: "设置设备状态",
        0x10: "触发动作",
        0x11: "停止动作",
        0x12: "动作完成通知",  # 设备主动推送
        0x13: "动作状态查询",
        0x20: "设置我的名片",
        0x21: "获取我的名片",
        0x22: "获取好友名片",
        0x23: "获取好友数量",
        0x30: "NFC交换名片通知",  # 设备主动推送
        0x40: "心率数据通知",  # 设备主动推送
        0x50: "触摸事件通知",  # 设备主动推送
        0x60: "电量变化通知",  # 设备主动推送
    }
    
    def __init__(self, device_name: Optional[str] = None, device_address: Optional[str] = None):
        """
        初始化监听器
        
        Args:
            device_name: 设备名称（如"Blinko"）
            device_address: 设备MAC地址
        """
        self.device_name = device_name
        self.device_address = device_address
        self.client: Optional[BleakClient] = None
        
        # 通知统计
        self.notification_count: Dict[str, int] = {}
        self.first_notification_time: Optional[float] = None
        self.last_notification_time: Optional[float] = None
        
        # 订阅的特征列表
        self.subscribed_chars: List[BleakGATTCharacteristic] = []
        
        # 自定义解析器
        self.custom_parsers: Dict[str, Callable] = {}
    
    def register_parser(self, char_uuid: str, parser: Callable):
        """
        注册自定义通知解析器
        
        Args:
            char_uuid: 特征UUID
            parser: 解析函数，接收(sender, data)参数
        """
        self.custom_parsers[char_uuid.lower()] = parser
    
    async def scan_devices(self, timeout: float = 5.0) -> List:
        """扫描BLE设备"""
        logger.info(f"扫描BLE设备... (超时: {timeout}s)")
        devices = await BleakScanner.discover(timeout=timeout)
        
        logger.info(f"找到 {len(devices)} 个设备:")
        for device in devices:
            logger.info(f"  - {device.name or '(无名称)'} ({device.address})")
        
        return devices
    
    async def connect(self, timeout: float = 10.0) -> bool:
        """连接到BLE设备"""
        try:
            # 如果没有提供地址，先扫描
            if not self.device_address:
                if not self.device_name:
                    logger.error("必须提供设备名称或地址")
                    return False
                
                logger.info(f"搜索设备: {self.device_name}")
                devices = await BleakScanner.discover(timeout=5.0)
                
                for device in devices:
                    if device.name == self.device_name:
                        self.device_address = device.address
                        logger.info(f"找到设备: {self.device_address}")
                        break
                
                if not self.device_address:
                    logger.error(f"未找到设备: {self.device_name}")
                    return False
            
            # 连接
            logger.info(f"连接到 {self.device_address}...")
            self.client = BleakClient(self.device_address, timeout=timeout)
            await self.client.connect()
            
            if self.client.is_connected:
                logger.info("✅ 连接成功!")
                
                # 显示MTU
                try:
                    mtu = self.client.mtu_size
                    logger.info(f"MTU: {mtu}")
                except:
                    pass
                
                return True
            else:
                logger.error("连接失败")
                return False
                
        except Exception as e:
            logger.error(f"连接错误: {e}")
            return False
    
    async def disconnect(self):
        """断开连接"""
        if self.client and self.client.is_connected:
            # 停止所有通知
            for char in self.subscribed_chars:
                try:
                    await self.client.stop_notify(char)
                except:
                    pass
            
            await self.client.disconnect()
            logger.info("已断开连接")
    
    def _get_char_name(self, char: BleakGATTCharacteristic) -> str:
        """获取特征名称"""
        uuid = char.uuid.lower()
        return self.KNOWN_CHARACTERISTICS.get(uuid, f"Unknown ({char.uuid})")
    
    def _parse_b11_notification(self, sender, data: bytearray):
        """解析B11协议通知"""
        if len(data) == 0:
            return "空数据"
        
        cmd = data[0]
        cmd_name = self.B11_COMMANDS.get(cmd, f"未知命令(0x{cmd:02X})")
        
        # 根据命令类型解析数据
        if cmd == 0x12:  # 动作完成通知
            if len(data) >= 2:
                action_id = data[1]
                return f"🎬 {cmd_name}: 动作ID={action_id}"
        
        elif cmd == 0x30:  # NFC交换名片通知
            if len(data) >= 65:
                card_data = data[1:65]
                # 尝试解析名片数据
                try:
                    name = card_data[4:24].decode('utf-8').rstrip('\x00')
                    phone = card_data[24:44].decode('utf-8').rstrip('\x00')
                    return f"👥 {cmd_name}:\n    姓名: {name}\n    电话: {phone}"
                except:
                    return f"👥 {cmd_name}: {len(card_data)}字节"
        
        elif cmd == 0x40:  # 心率数据通知
            if len(data) >= 3:
                heart_rate = data[1]
                quality = data[2]
                return f"❤️  {cmd_name}: {heart_rate} bpm (质量: {quality})"
        
        elif cmd == 0x50:  # 触摸事件通知
            if len(data) >= 2:
                touch_type = data[1]
                touch_names = {0: "单击", 1: "双击", 2: "长按", 3: "滑动"}
                return f"👆 {cmd_name}: {touch_names.get(touch_type, f'类型{touch_type}')}"
        
        elif cmd == 0x60:  # 电量变化通知
            if len(data) >= 2:
                battery = data[1]
                return f"🔋 {cmd_name}: {battery}%"
        
        return f"{cmd_name}: {len(data)}字节 - {data.hex()}"
    
    def _notification_handler(self, sender: BleakGATTCharacteristic, data: bytearray):
        """通用通知处理器"""
        uuid = sender.uuid.lower()
        char_name = self._get_char_name(sender)
        
        # 统计
        if uuid not in self.notification_count:
            self.notification_count[uuid] = 0
        self.notification_count[uuid] += 1
        
        if self.first_notification_time is None:
            self.first_notification_time = time.time()
        self.last_notification_time = time.time()
        
        # 时间戳
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        
        # 检查是否有自定义解析器
        if uuid in self.custom_parsers:
            try:
                result = self.custom_parsers[uuid](sender, data)
                logger.info(f"[{timestamp}] {char_name}:\n{result}")
                return
            except Exception as e:
                logger.error(f"自定义解析器错误: {e}")
        
        # 使用内置解析器
        if "b11" in char_name.lower():
            # B11协议特征
            parsed = self._parse_b11_notification(sender, data)
            logger.info(f"[{timestamp}] {char_name}:\n  {parsed}")
        
        elif "battery" in char_name.lower():
            # 电池电量
            if len(data) == 1:
                battery_level = data[0]
                logger.info(f"[{timestamp}] {char_name}: {battery_level}%")
            else:
                logger.info(f"[{timestamp}] {char_name}: {data.hex()}")
        
        elif "ota" in char_name.lower():
            # OTA相关
            if len(data) >= 20:
                # OTA ACK包
                sector = struct.unpack('<H', data[0:2])[0]
                status = struct.unpack('<H', data[2:4])[0]
                logger.info(f"[{timestamp}] {char_name}: Sector={sector}, Status={status}")
            else:
                logger.info(f"[{timestamp}] {char_name}: {data.hex()}")
        
        else:
            # 默认十六进制显示
            hex_str = data.hex()
            if len(hex_str) > 64:
                hex_str = hex_str[:64] + "..."
            logger.info(f"[{timestamp}] {char_name}: {len(data)}字节 - {hex_str}")
    
    async def list_services(self):
        """列出所有服务和特征"""
        if not self.client or not self.client.is_connected:
            logger.error("未连接到设备")
            return
        
        logger.info("\n" + "=" * 70)
        logger.info("设备服务和特征列表:")
        logger.info("=" * 70)
        
        for service in self.client.services:
            service_uuid = service.uuid.lower()
            service_name = self.KNOWN_SERVICES.get(service_uuid, "Unknown Service")
            logger.info(f"\n📦 Service: {service_name}")
            logger.info(f"   UUID: {service.uuid}")
            
            for char in service.characteristics:
                char_name = self._get_char_name(char)
                properties = ", ".join(char.properties)
                
                notify_support = "notify" in char.properties or "indicate" in char.properties
                notify_icon = "🔔" if notify_support else "  "
                
                logger.info(f"   {notify_icon} Characteristic: {char_name}")
                logger.info(f"      UUID: {char.uuid}")
                logger.info(f"      Properties: {properties}")
        
        logger.info("\n" + "=" * 70)
    
    async def subscribe_to_characteristic(self, char: BleakGATTCharacteristic):
        """订阅单个特征的通知"""
        try:
            await self.client.start_notify(char, self._notification_handler)
            self.subscribed_chars.append(char)
            char_name = self._get_char_name(char)
            logger.info(f"✅ 已订阅: {char_name}")
            return True
        except Exception as e:
            logger.error(f"订阅失败 {char.uuid}: {e}")
            return False
    
    async def subscribe_all_notifications(self, service_filter: Optional[List[str]] = None):
        """
        订阅所有支持通知的特征
        
        Args:
            service_filter: 服务UUID列表，只订阅这些服务（None表示全部）
        """
        if not self.client or not self.client.is_connected:
            logger.error("未连接到设备")
            return False
        
        logger.info("\n🔔 开始订阅通知...")
        
        subscribed_count = 0
        
        for service in self.client.services:
            # 检查服务过滤
            if service_filter and service.uuid.lower() not in [s.lower() for s in service_filter]:
                continue
            
            for char in service.characteristics:
                # 检查是否支持通知
                if "notify" in char.properties or "indicate" in char.properties:
                    if await self.subscribe_to_characteristic(char):
                        subscribed_count += 1
                        await asyncio.sleep(0.1)  # 小延迟避免过快订阅
        
        logger.info(f"\n✅ 共订阅了 {subscribed_count} 个特征")
        
        if subscribed_count == 0:
            logger.warning("⚠️  没有找到支持通知的特征")
            return False
        
        return True
    
    async def monitor(self, duration: Optional[float] = None):
        """
        开始监听通知
        
        Args:
            duration: 监听时长（秒），None表示持续监听直到中断
        """
        logger.info("\n" + "=" * 70)
        logger.info("📡 开始监听通知...")
        logger.info("   按 Ctrl+C 停止监听")
        if duration:
            logger.info(f"   自动停止时间: {duration}秒")
        logger.info("=" * 70 + "\n")
        
        try:
            if duration:
                await asyncio.sleep(duration)
            else:
                # 持续运行直到中断
                while True:
                    await asyncio.sleep(1)
        except KeyboardInterrupt:
            logger.info("\n\n⚠️  用户中断")
        
        # 显示统计信息
        self._print_statistics()
    
    def _print_statistics(self):
        """打印统计信息"""
        logger.info("\n" + "=" * 70)
        logger.info("📊 通知统计:")
        logger.info("=" * 70)
        
        if not self.notification_count:
            logger.info("   未收到任何通知")
            return
        
        total_count = sum(self.notification_count.values())
        duration = self.last_notification_time - self.first_notification_time if self.first_notification_time else 0
        
        logger.info(f"   总通知数: {total_count}")
        logger.info(f"   持续时间: {duration:.2f}秒")
        if duration > 0:
            logger.info(f"   平均速率: {total_count / duration:.2f} 通知/秒")
        
        logger.info("\n   各特征通知数:")
        for uuid, count in sorted(self.notification_count.items(), key=lambda x: x[1], reverse=True):
            # 查找特征
            char_name = "Unknown"
            for service in self.client.services:
                for char in service.characteristics:
                    if char.uuid.lower() == uuid:
                        char_name = self._get_char_name(char)
                        break
            
            logger.info(f"     {char_name}: {count}")
        
        logger.info("=" * 70)


async def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description='GATT通知监听工具')
    parser.add_argument('--name', type=str, help='设备名称 (如: Blinko)')
    parser.add_argument('--address', type=str, help='设备MAC地址')
    parser.add_argument('--scan', action='store_true', help='扫描设备后退出')
    parser.add_argument('--list', action='store_true', help='列出所有服务和特征')
    parser.add_argument('--duration', type=float, help='监听时长（秒）')
    parser.add_argument('--service', type=str, action='append', 
                       help='只监听指定服务UUID（可多次使用）')
    
    args = parser.parse_args()
    
    # 扫描模式
    if args.scan:
        monitor = GATTNotificationMonitor()
        await monitor.scan_devices(timeout=10.0)
        return
    
    # 检查参数
    if not args.name and not args.address:
        logger.error("请指定设备名称(--name)或地址(--address)")
        parser.print_help()
        return
    
    # 创建监听器
    monitor = GATTNotificationMonitor(
        device_name=args.name,
        device_address=args.address
    )
    
    try:
        # 连接
        if not await monitor.connect():
            logger.error("连接失败")
            return
        
        # 列出服务（如果需要）
        if args.list:
            await monitor.list_services()
            if input("\n继续监听通知? (y/n): ").lower() != 'y':
                return
        
        # 订阅通知
        if not await monitor.subscribe_all_notifications(service_filter=args.service):
            logger.error("订阅失败")
            return
        
        # 开始监听
        await monitor.monitor(duration=args.duration)
        
    finally:
        await monitor.disconnect()


if __name__ == '__main__':
    asyncio.run(main())
