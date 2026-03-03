#!/usr/bin/env python3
"""
BLE实时日志流查看器
连接到Blinko设备并订阅实时日志输出
"""

import asyncio
import sys
import argparse
from datetime import datetime
from bleak import BleakClient, BleakScanner

# BLE日志服务UUIDs
LOG_SERVICE_UUID = "6E400010-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_CONTROL_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E"  # 控制特征（写）
LOG_DATA_UUID = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"     # 数据特征（通知）

# 日志控制命令
LOG_CMD_SET_LEVEL = 0x02
LOG_CMD_STREAM_ON = 0x03
LOG_CMD_STREAM_OFF = 0x04

# 日志等级
LOG_LEVEL_NONE = 0
LOG_LEVEL_ERROR = 1
LOG_LEVEL_WARN = 2
LOG_LEVEL_INFO = 3
LOG_LEVEL_DEBUG = 4
LOG_LEVEL_VERBOSE = 5

# ANSI颜色代码
class Colors:
    RED = '\033[91m'      # 错误
    YELLOW = '\033[93m'   # 警告
    GREEN = '\033[92m'    # 信息
    BLUE = '\033[94m'     # 调试
    MAGENTA = '\033[95m'  # 详细
    CYAN = '\033[96m'     # 系统消息
    RESET = '\033[0m'     # 重置
    BOLD = '\033[1m'      # 粗体

class BLELogViewer:
    def __init__(self, device_name: str = "Blinko"):
        self.device_name = device_name
        self.client = None
        self.streaming = False
        self.log_count = 0
        self.start_time = None
        
    async def scan_and_connect(self, address: str = None):
        """扫描并连接到设备"""
        if address:
            print(f"{Colors.CYAN}📱 连接到设备: {address}...{Colors.RESET}")
            self.client = BleakClient(address)
        else:
            print(f"{Colors.CYAN}🔍 扫描设备: {self.device_name}...{Colors.RESET}")
            devices = await BleakScanner.discover(timeout=5.0)
            
            device = None
            for d in devices:
                if d.name and self.device_name.lower() in d.name.lower():
                    device = d
                    print(f"{Colors.GREEN}✅ 发现设备: {d.name} ({d.address}){Colors.RESET}")
                    break
            
            if not device:
                print(f"{Colors.RED}❌ 未找到设备: {self.device_name}{Colors.RESET}")
                return False
            
            self.client = BleakClient(device.address)
        
        try:
            await self.client.connect()
            print(f"{Colors.GREEN}✅ 已连接到 {self.client.address}{Colors.RESET}")
            return True
        except Exception as e:
            print(f"{Colors.RED}❌ 连接失败: {e}{Colors.RESET}")
            return False
    
    def format_log_line(self, log_text: str) -> str:
        """格式化日志行，添加颜色"""
        # 检测日志等级并着色
        if 'E (' in log_text or 'ERROR' in log_text.upper():
            return f"{Colors.RED}{log_text}{Colors.RESET}"
        elif 'W (' in log_text or 'WARN' in log_text.upper():
            return f"{Colors.YELLOW}{log_text}{Colors.RESET}"
        elif 'I (' in log_text or 'INFO' in log_text.upper():
            return f"{Colors.GREEN}{log_text}{Colors.RESET}"
        elif 'D (' in log_text or 'DEBUG' in log_text.upper():
            return f"{Colors.BLUE}{log_text}{Colors.RESET}"
        elif 'V (' in log_text or 'VERBOSE' in log_text.upper():
            return f"{Colors.MAGENTA}{log_text}{Colors.RESET}"
        else:
            return log_text
    
    def notification_handler(self, sender, data: bytearray):
        """处理BLE日志通知"""
        try:
            log_text = data.decode('utf-8', errors='replace').rstrip()
            if log_text:
                self.log_count += 1
                
                # 添加时间戳
                if self.start_time:
                    elapsed = (datetime.now() - self.start_time).total_seconds()
                    timestamp = f"[+{elapsed:7.2f}s]"
                else:
                    timestamp = f"[{datetime.now().strftime('%H:%M:%S')}]"
                
                # 格式化并打印
                formatted_log = self.format_log_line(log_text)
                print(f"{Colors.CYAN}{timestamp}{Colors.RESET} {formatted_log}")
                
        except Exception as e:
            print(f"{Colors.RED}❌ 解析日志失败: {e}{Colors.RESET}")
    
    async def set_log_level(self, level: int):
        """设置日志等级"""
        try:
            cmd = bytes([LOG_CMD_SET_LEVEL, level])
            await self.client.write_gatt_char(LOG_CONTROL_UUID, cmd, response=False)
            
            level_names = {
                0: "NONE",
                1: "ERROR",
                2: "WARN",
                3: "INFO",
                4: "DEBUG",
                5: "VERBOSE"
            }
            print(f"{Colors.CYAN}📝 日志等级设置为: {level_names.get(level, 'UNKNOWN')}{Colors.RESET}")
        except Exception as e:
            print(f"{Colors.RED}❌ 设置日志等级失败: {e}{Colors.RESET}")
    
    async def start_streaming(self):
        """启动日志流"""
        try:
            # 订阅日志通知
            await self.client.start_notify(LOG_DATA_UUID, self.notification_handler)
            print(f"{Colors.GREEN}✅ 已订阅日志通知{Colors.RESET}")
            
            # 等待订阅生效
            await asyncio.sleep(0.3)
            
            # 发送启动流命令
            cmd = bytes([LOG_CMD_STREAM_ON])
            await self.client.write_gatt_char(LOG_CONTROL_UUID, cmd, response=False)
            print(f"{Colors.GREEN}✅ 日志流已启动{Colors.RESET}")
            
            self.streaming = True
            self.start_time = datetime.now()
            
        except Exception as e:
            print(f"{Colors.RED}❌ 启动日志流失败: {e}{Colors.RESET}")
            raise
    
    async def stop_streaming(self):
        """停止日志流"""
        if not self.streaming:
            return
        
        try:
            # 发送停止流命令
            cmd = bytes([LOG_CMD_STREAM_OFF])
            await self.client.write_gatt_char(LOG_CONTROL_UUID, cmd, response=False)
            print(f"\n{Colors.YELLOW}⏸️  日志流已停止{Colors.RESET}")
            
            # 取消订阅
            await self.client.stop_notify(LOG_DATA_UUID)
            
            self.streaming = False
            
        except Exception as e:
            print(f"{Colors.RED}❌ 停止日志流失败: {e}{Colors.RESET}")
    
    async def disconnect(self):
        """断开连接"""
        if self.client and self.client.is_connected:
            await self.stop_streaming()
            await self.client.disconnect()
            print(f"{Colors.CYAN}👋 已断开连接{Colors.RESET}")
            print(f"{Colors.CYAN}📊 统计: 共接收 {self.log_count} 条日志{Colors.RESET}")
    
    async def run(self, address: str = None, log_level: int = LOG_LEVEL_INFO):
        """运行日志查看器"""
        print(f"{Colors.BOLD}{'='*60}{Colors.RESET}")
        print(f"{Colors.BOLD}{Colors.CYAN}         BLE实时日志流查看器{Colors.RESET}")
        print(f"{Colors.BOLD}{'='*60}{Colors.RESET}")
        print()
        
        # 连接设备
        if not await self.scan_and_connect(address):
            return
        
        try:
            # 设置日志等级
            await self.set_log_level(log_level)
            
            # 启动日志流
            await self.start_streaming()
            
            print()
            print(f"{Colors.BOLD}{'='*60}{Colors.RESET}")
            print(f"{Colors.CYAN}📡 实时日志流 (按 Ctrl+C 退出){Colors.RESET}")
            print(f"{Colors.BOLD}{'='*60}{Colors.RESET}")
            print()
            
            # 保持运行，接收日志
            while self.streaming:
                await asyncio.sleep(0.1)
                
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⏸️  用户中断{Colors.RESET}")
        except Exception as e:
            print(f"\n{Colors.RED}❌ 错误: {e}{Colors.RESET}")
        finally:
            await self.disconnect()


async def main():
    parser = argparse.ArgumentParser(
        description='BLE实时日志流查看器',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
示例:
  %(prog)s                                    # 扫描并连接Blinko设备
  %(prog)s --name MyDevice                    # 连接到指定名称的设备
  %(prog)s --address 90:E5:B1:AE:E4:56        # 连接到指定MAC地址
  %(prog)s --level debug                      # 设置日志等级为DEBUG
  %(prog)s --address 90:E5:B1:AE:E4:56 -l 5   # 连接并设置VERBOSE等级

日志等级:
  0 = NONE      (无日志)
  1 = ERROR     (仅错误)
  2 = WARN      (警告及以上)
  3 = INFO      (信息及以上，默认)
  4 = DEBUG     (调试及以上)
  5 = VERBOSE   (详细，所有日志)
        '''
    )
    
    parser.add_argument('--name', '-n',
                       default='Blinko',
                       help='设备名称 (默认: Blinko)')
    
    parser.add_argument('--address', '-a',
                       help='设备MAC地址 (例如: 90:E5:B1:AE:E4:56)')
    
    parser.add_argument('--level', '-l',
                       type=str,
                       choices=['none', 'error', 'warn', 'info', 'debug', 'verbose', 
                               '0', '1', '2', '3', '4', '5'],
                       default='info',
                       help='日志等级 (默认: info)')
    
    args = parser.parse_args()
    
    # 解析日志等级
    level_map = {
        'none': LOG_LEVEL_NONE,
        'error': LOG_LEVEL_ERROR,
        'warn': LOG_LEVEL_WARN,
        'info': LOG_LEVEL_INFO,
        'debug': LOG_LEVEL_DEBUG,
        'verbose': LOG_LEVEL_VERBOSE,
        '0': LOG_LEVEL_NONE,
        '1': LOG_LEVEL_ERROR,
        '2': LOG_LEVEL_WARN,
        '3': LOG_LEVEL_INFO,
        '4': LOG_LEVEL_DEBUG,
        '5': LOG_LEVEL_VERBOSE,
    }
    log_level = level_map[args.level]
    
    # 创建并运行查看器
    viewer = BLELogViewer(args.name)
    await viewer.run(address=args.address, log_level=log_level)


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 退出")
    except Exception as e:
        print(f"\n❌ 错误: {e}")
        sys.exit(1)
