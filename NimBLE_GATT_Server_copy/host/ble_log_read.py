#!/usr/bin/env python3
"""
BLE日志读取工具 - 使用READ方式轮询日志
"""

import asyncio
import sys
import gc
from datetime import datetime
from bleak import BleakClient, BleakScanner

# BLE UUIDs
LOG_SIZE_UUID = "6E400011-B5A3-F393-E0A9-E50E24DCCA9E"      # 读取日志大小
LOG_DATA_UUID = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"      # 读取日志数据
LOG_CONTROL_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E"   # 控制命令

# 日志控制命令
LOG_CMD_READ_START = 0x05   # 开始读取
LOG_CMD_READ_NEXT = 0x06    # 读取下一块
LOG_CMD_SET_LEVEL = 0x02    # 设置日志级别

# ANSI颜色
class Colors:
    RED = '\033[91m'
    YELLOW = '\033[93m'
    GREEN = '\033[92m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    RESET = '\033[0m'

class BLELogReader:
    def __init__(self, max_log_count=50000):
        self.client = None
        self.log_count = 0
        self.last_size = 0
        self.max_log_count = max_log_count  # 防止计数器溢出
        self.total_processed = 0  # 总处理计数
        self.gc_counter = 0  # 垃圾回收计数器
        
    async def scan_and_connect(self, device_name="Blinko"):
        """扫描并连接设备 - 增强版"""
        # 先清理旧连接
        await self._cleanup_connection()
        
        print(f"🔍 扫描设备: {device_name}...")
        
        try:
            devices = await BleakScanner.discover(timeout=5.0)
        except Exception as e:
            print(f"{Colors.RED}❌ 扫描失败: {e}{Colors.RESET}")
            return False
        
        target = None
        for d in devices:
            if d.name and device_name.lower() in d.name.lower():
                target = d
                print(f"✅ 发现: {d.name} ({d.address})")
                break
        
        if not target:
            print(f"❌ 未找到设备 {device_name}")
            return False
        
        # 连接重试机制
        for attempt in range(3):
            try:
                self.client = BleakClient(target.address)
                await self.client.connect(timeout=10.0)
                print(f"✅ 已连接 (尝试 {attempt + 1}/3)\n")
                return True
            except Exception as e:
                print(f"⚠️ 连接尝试 {attempt + 1} 失败: {e}")
                await self._cleanup_connection()
                if attempt < 2:
                    await asyncio.sleep(2.0)
        
        print(f"❌ 连接失败，已尝试3次")
        return False
    
    async def _cleanup_connection(self):
        """清理连接资源"""
        if self.client:
            try:
                if self.client.is_connected:
                    await self.client.disconnect()
            except:
                pass
            finally:
                self.client = None
        gc.collect()
    
    def _reset_counters_if_needed(self):
        """重置计数器防止溢出"""
        if self.log_count >= self.max_log_count:
            print(f"\n{Colors.CYAN}🔄 日志计数器重置 (已处理 {self.log_count} 条){Colors.RESET}")
            self.log_count = 0
            gc.collect()
    
    def _periodic_gc(self):
        """定期垃圾回收"""
        self.gc_counter += 1
        if self.gc_counter % 100 == 0:
            gc.collect()
            if self.gc_counter % 500 == 0:
                print(f"{Colors.CYAN}💾 内存清理 (已处理 {self.total_processed} 条日志){Colors.RESET}")
    
    def format_log_line(self, log_text: str) -> str:
        """格式化日志行，添加颜色"""
        if 'E (' in log_text or 'ERROR' in log_text.upper():
            return f"{Colors.RED}{log_text}{Colors.RESET}"
        elif 'W (' in log_text or 'WARN' in log_text.upper():
            return f"{Colors.YELLOW}{log_text}{Colors.RESET}"
        elif 'I (' in log_text or 'INFO' in log_text.upper():
            return f"{Colors.GREEN}{log_text}{Colors.RESET}"
        elif 'D (' in log_text or 'DEBUG' in log_text.upper():
            return f"{Colors.BLUE}{log_text}{Colors.RESET}"
        else:
            return log_text
    
    async def get_log_size(self):
        """读取日志缓冲区大小 - 增强错误处理"""
        if not self.client or not self.client.is_connected:
            return 0
        
        try:
            data = await self.client.read_gatt_char(LOG_SIZE_UUID)
            if len(data) >= 4:
                size = int.from_bytes(data[:4], 'little')
                return size
        except Exception as e:
            print(f"{Colors.RED}❌ 读取日志大小失败: {e}{Colors.RESET}")
            if not self.client.is_connected:
                print(f"{Colors.YELLOW}⚠️  连接已断开{Colors.RESET}")
        return 0
    
    async def read_logs_batch(self):
        """批量读取日志 - 内存优化版"""
        if not self.client or not self.client.is_connected:
            return []
        
        try:
            # 1. 发送READ_START命令
            await self.client.write_gatt_char(LOG_CONTROL_UUID, bytes([LOG_CMD_READ_START]), response=False)
            await asyncio.sleep(0.1)
            
            # 2. 读取日志大小
            size = await self.get_log_size()
            if size == 0:
                return []
            
            print(f"{Colors.CYAN}📊 缓冲区大小: {size} 字节{Colors.RESET}")
            
            # 3. 流式处理，避免大列表累积
            processed_lines = []
            total_read = 0
            
            while total_read < size:
                try:
                    # 发送READ_NEXT命令
                    await self.client.write_gatt_char(LOG_CONTROL_UUID, bytes([LOG_CMD_READ_NEXT]), response=False)
                    await asyncio.sleep(0.05)
                    
                    # 读取数据
                    data = await self.client.read_gatt_char(LOG_DATA_UUID)
                    if len(data) == 0:
                        break
                    
                    # 立即处理，避免累积
                    try:
                        text = data.decode('utf-8', errors='replace')
                        lines = text.split('\n')
                        for line in lines:
                            line_stripped = line.strip()
                            if line_stripped:
                                processed_lines.append(line_stripped)
                        
                        # 显式删除大对象引用
                        del text, lines
                        
                    except Exception as decode_err:
                        print(f"⚠️ 数据解码错误: {decode_err}")
                    
                    total_read += len(data)
                    await asyncio.sleep(0.05)
                    
                    # 防止单次读取过多数据
                    if len(processed_lines) > 1000:
                        print(f"{Colors.YELLOW}⚠️  单次读取数据过多，分批处理{Colors.RESET}")
                        break
                    
                except Exception as read_err:
                    print(f"⚠️ 数据读取错误: {read_err}")
                    break
            
            return processed_lines
            
        except Exception as e:
            print(f"{Colors.RED}❌ 批量读取失败: {e}{Colors.RESET}")
            return []
    
    async def poll_logs(self, interval=2.0):
        """轮询读取新日志 - 内存和连接优化版"""
        print(f"{Colors.CYAN}{'='*70}{Colors.RESET}")
        print(f"{Colors.CYAN}📡 轮询模式 (每{interval}秒检查，Ctrl+C退出){Colors.RESET}")
        print(f"{Colors.CYAN}💾 内存保护: 计数器上限 {self.max_log_count}，定期垃圾回收{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*70}{Colors.RESET}\n")
        
        connection_error_count = 0
        max_connection_errors = 5
        
        try:
            while True:
                # 检查连接状态
                if not self.client or not self.client.is_connected:
                    print(f"{Colors.RED}❌ 连接已断开，尝试重连...{Colors.RESET}")
                    if not await self.scan_and_connect():
                        connection_error_count += 1
                        if connection_error_count >= max_connection_errors:
                            print(f"{Colors.RED}❌ 连续重连失败{max_connection_errors}次，退出{Colors.RESET}")
                            break
                        await asyncio.sleep(5.0)
                        continue
                    connection_error_count = 0
                
                # 获取当前日志大小
                size = await self.get_log_size()
                
                if size > self.last_size:
                    print(f"\n{Colors.CYAN}[{datetime.now().strftime('%H:%M:%S')}] 新日志 ({size} 字节){Colors.RESET}")
                    
                    # 读取并立即处理日志
                    logs = await self.read_logs_batch()
                    
                    # 流式显示，处理完立即释放
                    for line in logs:
                        self.log_count += 1
                        self.total_processed += 1
                        self._reset_counters_if_needed()
                        self._periodic_gc()
                        
                        formatted = self.format_log_line(line)
                        print(f"[{self.log_count:04d}] {formatted}")
                    
                    # 显式清理本次数据
                    del logs
                    self.last_size = size
                
                await asyncio.sleep(interval)
                
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⏸️  用户中断{Colors.RESET}")
        except Exception as e:
            print(f"\n{Colors.RED}❌ 轮询错误: {e}{Colors.RESET}")
    
    async def read_once(self):
        """一次性读取所有日志 - 内存优化版"""
        print(f"{Colors.CYAN}{'='*70}{Colors.RESET}")
        print(f"{Colors.CYAN}📖 读取缓冲区所有日志{Colors.RESET}")
        print(f"{Colors.CYAN}{'='*70}{Colors.RESET}\n")
        
        logs = await self.read_logs_batch()
        
        if logs:
            for line in logs:
                self.log_count += 1
                self.total_processed += 1
                self._reset_counters_if_needed()
                self._periodic_gc()
                
                formatted = self.format_log_line(line)
                print(f"[{self.log_count:04d}] {formatted}")
            
            # 显式清理
            del logs
        else:
            print(f"{Colors.YELLOW}⚠️  缓冲区为空{Colors.RESET}")
        
        print(f"\n{Colors.CYAN}📊 共 {self.log_count} 条日志{Colors.RESET}")
    
    def print_memory_stats(self):
        """打印内存使用统计"""
        print(f"\n{Colors.CYAN}📊 内存使用统计:{Colors.RESET}")
        print(f"  总处理日志: {self.total_processed}")
        print(f"  当前计数器: {self.log_count}")
        print(f"  垃圾回收次数: {self.gc_counter // 100}")
    
    async def disconnect(self):
        """断开连接并清理所有资源"""
        await self._cleanup_connection()
        self.print_memory_stats()
        
        # 最终清理
        gc.collect()
        print(f"\n{Colors.CYAN}👋 已断开连接，所有资源已清理{Colors.RESET}")

async def main():
    import argparse
    parser = argparse.ArgumentParser(description='BLE日志读取工具（READ模式）- 内存优化版')
    parser.add_argument('--name', default='Blinko', help='设备名称')
    parser.add_argument('--poll', action='store_true', help='轮询模式（持续监控）')
    parser.add_argument('--interval', type=float, default=2.0, help='轮询间隔（秒）')
    parser.add_argument('--max-logs', type=int, default=50000, help='单轮计数器上限')
    parser.add_argument('--test', action='store_true', help='内存泄漏测试模式（快速轮询）')
    args = parser.parse_args()
    
    # 内存保护设置
    reader = BLELogReader(max_log_count=args.max_logs)
    
    try:
        if not await reader.scan_and_connect(args.name):
            return
        
        if args.test:
            # 内存泄漏测试模式
            print(f"{Colors.CYAN}🧪 内存泄漏测试模式（快速轮询）{Colors.RESET}")
            await reader.poll_logs(0.5)  # 快速轮询测试
        elif args.poll:
            await reader.poll_logs(args.interval)
        else:
            await reader.read_once()
        
    except Exception as e:
        print(f"{Colors.RED}❌ 错误: {e}{Colors.RESET}")
    finally:
        await reader.disconnect()

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n👋 退出")
