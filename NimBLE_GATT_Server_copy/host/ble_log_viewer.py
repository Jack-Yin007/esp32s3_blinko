#!/usr/bin/env python3
"""
BLE日志查看器 - 用于ESP32-S3 BLE日志服务
功能：
1. 实时流模式 (--stream)：订阅BLE通知，彩色输出实时日志
2. 下载模式 (--download)：批量读取日志，保存到本地文件
3. 交互命令：clear, level <0-5>, stream on/off

使用示例：
    python ble_log_viewer.py --name Blinko --stream           # 实时查看
    python ble_log_viewer.py --name Blinko --download         # 下载到文件
    python ble_log_viewer.py --name Blinko --download --file my_log.txt  # 指定文件名
"""

import asyncio
import argparse
import sys
from datetime import datetime
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

# ============================================================================
# BLE LOG Service UUIDs (6E400010~6E400013)
# ============================================================================
LOG_SERVICE_UUID     = "6E400010-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_SIZE_CHAR_UUID   = "6E400011-B5A3-F393-E0A9-E50E24DCCA9E"  # Read: uint32_t 当前日志大小
LOG_DATA_CHAR_UUID   = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"  # Read/Notify: 日志数据
LOG_CONTROL_CHAR_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E" # Write: 控制命令

# 控制命令
LOG_CMD_CLEAR       = 0x01  # 清空日志缓冲区
LOG_CMD_SET_LEVEL   = 0x02  # 设置日志等级 (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)
LOG_CMD_STREAM_ON   = 0x03  # 开启实时流
LOG_CMD_STREAM_OFF  = 0x04  # 关闭实时流
LOG_CMD_READ_START  = 0x05  # 重置读取偏移

# 日志等级定义 (与固件log_level_t枚举对应)
LOG_COLLECT_NONE    = 0
LOG_COLLECT_ERROR   = 1
LOG_COLLECT_WARN    = 2
LOG_COLLECT_INFO    = 3
LOG_COLLECT_DEBUG   = 4
LOG_COLLECT_VERBOSE = 5

# 日志等级颜色 (ANSI escape codes)
LOG_COLORS = {
    'E': '\033[91m',  # 红色 (ERROR)
    'W': '\033[93m',  # 黄色 (WARN)
    'I': '\033[92m',  # 绿色 (INFO)
    'D': '\033[94m',  # 蓝色 (DEBUG)
    'V': '\033[95m',  # 紫色 (VERBOSE)
}
COLOR_RESET = '\033[0m'

# ============================================================================
# BLE日志客户端
# ============================================================================
class BLELogViewer:
    def __init__(self, device_name: str):
        self.device_name = device_name
        self.client = None
        self.streaming = False
        self.log_data = bytearray()
        
    async def connect(self):
        """扫描并连接到BLE设备"""
        # 判断是MAC地址还是设备名称
        if ':' in self.device_name or '-' in self.device_name:
            # MAC地址格式 (xx:xx:xx:xx:xx:xx 或 xx-xx-xx-xx-xx-xx)
            print(f"🔍 通过MAC地址连接: {self.device_name}")
            device = await BleakScanner.find_device_by_address(self.device_name, timeout=10.0)
        else:
            # 设备名称
            print(f"🔍 扫描设备名称: '{self.device_name}'...")
            device = await BleakScanner.find_device_by_name(self.device_name, timeout=10.0)
        
        if not device:
            print(f"❌ 未找到设备 '{self.device_name}'")
            return False
        
        print(f"✅ 发现设备: {device.name} ({device.address})")
        
        self.client = BleakClient(device)
        try:
            await self.client.connect()
            print(f"🔗 已连接到 {device.name}")
            
            # 检查LOG服务是否存在
            services = self.client.services
            if LOG_SERVICE_UUID.lower() not in [s.uuid.lower() for s in services]:
                print(f"❌ 设备不支持LOG服务 (UUID: {LOG_SERVICE_UUID})")
                await self.client.disconnect()
                return False
            
            print(f"✅ LOG服务已激活")
            return True
            
        except BleakError as e:
            print(f"❌ 连接失败: {e}")
            return False
    
    async def disconnect(self):
        """断开连接"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("🔌 已断开连接")
    
    async def get_log_size(self) -> int:
        """读取当前日志缓冲区大小"""
        try:
            data = await self.client.read_gatt_char(LOG_SIZE_CHAR_UUID)
            size = int.from_bytes(data, byteorder='little')
            return size
        except Exception as e:
            print(f"❌ 读取日志大小失败: {e}")
            return 0
    
    async def send_command(self, cmd: int, arg: int = 0):
        """发送控制命令"""
        try:
            if arg > 0:
                data = bytes([cmd, arg])
            else:
                data = bytes([cmd])
            await self.client.write_gatt_char(LOG_CONTROL_CHAR_UUID, data)
        except Exception as e:
            print(f"❌ 发送命令失败: {e}")
    
    def notification_handler(self, sender, data: bytearray):
        """处理实时日志通知"""
        try:
            log_line = data.decode('utf-8', errors='replace')
            self._print_colored_log(log_line)
        except Exception as e:
            print(f"❌ 解析日志失败: {e}")
    
    def _print_colored_log(self, log_line: str):
        """彩色打印日志"""
        # 检测日志等级 (格式: "E (12345) TAG: message")
        if len(log_line) > 2 and log_line[1] == ' ':
            level = log_line[0]
            color = LOG_COLORS.get(level, '')
            print(f"{color}{log_line.rstrip()}{COLOR_RESET}")
        else:
            print(log_line.rstrip())
    
    async def stream_logs(self):
        """实时流模式：订阅BLE通知（注意：Windows可能不支持，请使用--download）"""
        print("=" * 60)
        print("📡 实时日志流模式 (Ctrl+C 退出)")
        print("=" * 60)
        print("⚠️  注意：Windows BLE驱动可能不支持订阅，如果失败请使用 --download")
        print()
        
        try:
            # 启用实时流
            await self.send_command(LOG_CMD_STREAM_ON)
            print("✅ 实时流已开启\n")
            
            # 订阅通知
            await self.client.start_notify(LOG_DATA_CHAR_UUID, self.notification_handler)
            self.streaming = True
            
            # 保持连接，接收通知
            while self.streaming:
                await asyncio.sleep(0.1)
        except KeyboardInterrupt:
            print("\n⏸️  停止实时流...")
        except Exception as e:
            print(f"\n❌ 实时流失败: {e}")
            print("💡 提示：请使用 --download 批量下载日志")
        finally:
            try:
                if self.client.is_connected:
                    await self.client.stop_notify(LOG_DATA_CHAR_UUID)
                    await self.send_command(LOG_CMD_STREAM_OFF)
                    print("✅ 实时流已关闭")
            except:
                pass
    
    async def download_logs(self, filename: str = None):
        """下载模式：批量读取日志（显示到终端或保存到文件）"""
        save_to_file = filename is not None
        
        if save_to_file:
            print("=" * 60)
            print(f"💾 下载日志到文件: {filename}")
            print("=" * 60)
        else:
            print("=" * 60)
            print("📄 读取设备日志")
            print("=" * 60)
        
        # 1. 读取日志大小
        log_size = await self.get_log_size()
        if log_size == 0:
            print("⚠️  日志缓冲区为空")
            return
        
        print(f"📊 日志大小: {log_size} 字节")
        
        # 2. 重置读取偏移
        await self.send_command(LOG_CMD_READ_START)
        
        # 3. 分块读取日志 (每次512字节，持续读取直到读完)
        chunk_size = 512
        total_read = 0
        log_data = bytearray()
        max_chunks = 100  # 最多100个块（50KB），防止无限循环
        
        print(f"📥 开始下载...")
        
        for chunk_num in range(max_chunks):
            try:
                chunk = await self.client.read_gatt_char(LOG_DATA_CHAR_UUID)
                
                # 如果返回空数据，说明读完了
                if not chunk or len(chunk) == 0:
                    break
                
                log_data.extend(chunk)
                total_read += len(chunk)
                
                # 进度条（基于初始大小估算）
                if log_size > 0:
                    progress = min(int((total_read / log_size) * 100), 100)
                    bar_len = 40
                    filled = int((progress / 100) * bar_len)
                    bar = '█' * filled + '░' * (bar_len - filled)
                    print(f"\r[{bar}] {progress}% ({total_read}/{log_size} B)", end='', flush=True)
                else:
                    print(f"\r📥 已读取: {total_read} 字节", end='', flush=True)
                
                # 如果读到的数据少于512字节，说明已经到末尾了
                if len(chunk) < chunk_size:
                    break
                    
            except Exception as e:
                print(f"\n❌ 读取失败: {e}")
                break
        
        print(f"\n✅ 第一轮读取完成: {total_read} 字节")
        
        # 3.5. 等待500ms，让最新日志完全写入，然后再次读取
        await asyncio.sleep(0.5)
        
        print(f"📥 检查新增日志...")
        extra_chunks = 0
        for i in range(10):  # 最多再读10个块
            try:
                chunk = await self.client.read_gatt_char(LOG_DATA_CHAR_UUID)
                if not chunk or len(chunk) == 0:
                    break
                
                log_data.extend(chunk)
                total_read += len(chunk)
                extra_chunks += 1
                
                if len(chunk) < chunk_size:
                    break
            except:
                break
        
        if extra_chunks > 0:
            print(f"✅ 捕获到 {extra_chunks} 个新增数据块")
        
        print(f"✅ 总计读取: {total_read} 字节")
        
        # 4. 输出日志（保存到文件或显示到终端）
        if save_to_file:
            # 保存到文件
            try:
                with open(filename, 'wb') as f:
                    f.write(log_data)
                print(f"💾 已保存到: {filename}")
            except Exception as e:
                print(f"❌ 保存文件失败: {e}")
        else:
            # 显示到终端（彩色输出）
            print("\n" + "=" * 60)
            print("📋 设备日志内容:")
            print("=" * 60 + "\n")
            
            try:
                log_text = log_data.decode('utf-8', errors='replace')
                for line in log_text.split('\n'):
                    if line.strip():
                        self._print_colored_log(line)
            except Exception as e:
                print(f"❌ 解析日志失败: {e}")
                # 显示原始数据
                print(log_data.decode('utf-8', errors='replace'))
    
    async def clear_logs(self):
        """清空日志缓冲区"""
        await self.send_command(LOG_CMD_CLEAR)
        print("🗑️  日志缓冲区已清空")
    
    async def set_log_level(self, level: int):
        """设置日志等级 (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)"""
        if 0 <= level <= 5:
            await self.send_command(LOG_CMD_SET_LEVEL, level)
            level_names = ['NONE', 'ERROR', 'WARN', 'INFO', 'DEBUG', 'VERBOSE']
            print(f"📝 日志等级已设置为: {level_names[level]} (过滤等级{level}以下的日志)")
        else:
            print("❌日志等级必须在0-5之间")

# ============================================================================
# 主函数
# ============================================================================
async def main():
    parser = argparse.ArgumentParser(
        description='BLE日志查看器 - ESP32-S3设备远程调试工具',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
使用示例:
  # 通过设备名称连接
  %(prog)s --name Blinko --download
  
  # 通过MAC地址连接（推荐，避免多设备冲突）
  %(prog)s --address 90:E5:B1:AE:E4:56 --download
  
  # 实时查看日志
  %(prog)s --name Blinko --stream
  
  # 指定保存文件名
  %(prog)s --name Blinko --download --file my_device_log.txt
  
  # 清空日志缓冲区
  %(prog)s --address 90:E5:B1:AE:E4:56 --clear
  
  # 设置日志等级为DEBUG
  %(prog)s --name Blinko --level 4
        """
    )
    
    parser.add_argument('--name', '-n', help='BLE设备名称 (如: Blinko)')
    parser.add_argument('--address', '-a', help='BLE设备MAC地址 (如: 90:E5:B1:AE:E4:56)，优先于--name')
    parser.add_argument('--stream', '-s', action='store_true', help='实时流模式 (订阅通知)')
    parser.add_argument('--download', '-d', action='store_true', help='下载模式 (批量读取)')
    parser.add_argument('--file', '-f', help='下载文件名 (默认: device_log_YYYYMMDD_HHMMSS.txt)')
    parser.add_argument('--clear', '-c', action='store_true', help='清空日志缓冲区')
    parser.add_argument('--level', '-l', type=int, choices=[0,1,2,3,4,5], 
                        help='设置日志等级 (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)')
    
    args = parser.parse_args()
    
    # 检查必须指定设备名称或地址
    if not args.name and not args.address:
        parser.print_help()
        print("\n❌ 错误: 必须指定 --name 或 --address")
        sys.exit(1)
    
    # 检查必须至少选择一个操作
    if not (args.stream or args.download or args.clear or args.level is not None):
        parser.print_help()
        print("\n❌ 错误: 必须指定至少一个操作 (--stream, --download, --clear, --level)")
        sys.exit(1)
    
    # 创建客户端（优先使用MAC地址）
    viewer = BLELogViewer(args.address if args.address else args.name)
    
    # 连接设备
    if not await viewer.connect():
        sys.exit(1)
    
    try:
        # 执行操作
        if args.clear:
            await viewer.clear_logs()
        
        if args.level is not None:
            await viewer.set_log_level(args.level)
        
        if args.stream:
            await viewer.stream_logs()
        
        if args.download:
            await viewer.download_logs(args.file)
        
    finally:
        await viewer.disconnect()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 用户中断，退出程序")
        sys.exit(0)
