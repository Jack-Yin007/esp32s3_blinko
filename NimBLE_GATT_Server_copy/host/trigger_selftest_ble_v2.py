#!/usr/bin/env python3
"""
通过BLE触发自检并接收测试报告（优化版 - 完整日志显示）
"""

import asyncio
import struct
import sys
from datetime import datetime
from bleak import BleakClient, BleakScanner

# BLE UUIDs
OTA_CONTROL_UUID = "00008022-0000-1000-8000-00805f9b34fb"
LOG_SIZE_UUID = "6E400011-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_DATA_UUID = "6E400012-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_CONTROL_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E"

# 命令
SELF_TEST_CMD = 0x0009

# ANSI颜色
class Colors:
    RED = '\033[91m'
    YELLOW = '\033[93m'
    GREEN = '\033[92m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    MAGENTA = '\033[95m'
    RESET = '\033[0m'

class SelfTestTrigger:
    def __init__(self):
        self.client = None
        self.log_count = 0
        self.selftest_started = False
        self.selftest_completed = False
        self.last_read_pos = 0
        self.displayed_lines = set()
        
    async def scan_and_connect(self, device_name="Blinko"):
        """扫描并连接设备"""
        print(f"🔍 扫描设备: {device_name}...")
        devices = await BleakScanner.discover(timeout=5.0)
        
        target = None
        for d in devices:
            if d.name and device_name.lower() in d.name.lower():
                target = d
                print(f"✅ 发现: {d.name} ({d.address})")
                break
        
        if not target:
            print(f"❌ 未找到设备 {device_name}")
            return False
        
        self.client = BleakClient(target.address)
        await self.client.connect()
        print(f"✅ 已连接\n")
        return True
    
    def crc16_ccitt(self, data: bytes) -> int:
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
    
    async def trigger_selftest(self):
        """发送自检触发命令"""
        try:
            print(f"{Colors.MAGENTA}🔧 发送自检命令 (0x0009)...{Colors.RESET}")
            
            # 创建20字节命令包
            cmd_packet = bytearray(20)
            cmd_packet[0] = SELF_TEST_CMD & 0xFF
            cmd_packet[1] = (SELF_TEST_CMD >> 8) & 0xFF
            
            # 计算CRC16
            crc = self.crc16_ccitt(bytes(cmd_packet[:18]))
            cmd_packet[18] = crc & 0xFF
            cmd_packet[19] = (crc >> 8) & 0xFF
            
            await self.client.write_gatt_char(OTA_CONTROL_UUID, cmd_packet, response=False)
            print(f"{Colors.GREEN}✅ 自检命令已发送{Colors.RESET}\n")
            
        except Exception as e:
            print(f"{Colors.RED}❌ 发送命令失败: {e}{Colors.RESET}")
            return False
        return True
    
    def format_log_line(self, log_text: str) -> str:
        """格式化日志行，添加颜色"""
        # 清理日志行
        log_text = log_text.strip()
        
        # 自检相关日志特殊处理
        if any(word in log_text for word in ['SelfTest:', 'Self-Test', 'SelfTestTrigger:']):
            if '[PASS]' in log_text or 'PASS:' in log_text or '✅' in log_text:
                return f"{Colors.GREEN}{log_text}{Colors.RESET}"
            elif '[FAIL]' in log_text or 'FAIL:' in log_text or '❌' in log_text:
                return f"{Colors.RED}{log_text}{Colors.RESET}"
            elif '[WARN]' in log_text or 'WARNING:' in log_text or '⚠️' in log_text:
                return f"{Colors.YELLOW}{log_text}{Colors.RESET}"
            elif '====' in log_text:
                return f"{Colors.CYAN}{log_text}{Colors.RESET}"
            else:
                return f"{Colors.CYAN}{log_text}{Colors.RESET}"
        
        # 普通日志
        return log_text
    
    async def get_log_chunk(self):
        """获取日志数据块（改进版）"""
        try:
            # 1. 首次读取时，发送READ_START命令创建快照
            if self.last_read_pos == 0:
                read_start_cmd = bytes([0x05])  # LOG_CMD_READ_START
                await self.client.write_gatt_char(LOG_CONTROL_UUID, read_start_cmd, response=False)
                await asyncio.sleep(0.1)
            
            # 2. 读取数据
            data = await self.client.read_gatt_char(LOG_DATA_UUID)
            if data:
                self.last_read_pos += len(data)
            
            return data
            
        except Exception as e:
            return b""
    
    def is_selftest_log(self, log_line: str) -> bool:
        """判断是否为自检相关日志"""
        if not log_line.strip():
            return False
            
        selftest_keywords = [
            'SelfTest:', 'Self-Test', 'SelfTestTrigger:',
            'Testing:', '[PASS]', '[FAIL]', '[WARN]',
            'Self-test command received', 'Device will restart',
            'Complete!', 'Starting Self-Test',
            # 设备测试关键词
            'AW9535', 'LED Controller', 'Vibrator Motor',
            'Battery', 'I2S Audio', 'WAV Player', 'Microphone',
            'Servo Motor', 'NFC Module', 'Touch Sensor',
            'Voice Recognition', 'Flash FAT', 'BLE Stack'
        ]
        return any(keyword in log_line for keyword in selftest_keywords)
    
    async def monitor_selftest_logs(self, timeout=60):
        """监控自检日志输出（改进版）"""
        print(f"{Colors.CYAN}{'='*70}{Colors.RESET}")
        print(f"{Colors.CYAN}🔬 实时监控自检报告 (超时: {timeout}秒){Colors.RESET}")
        print(f"{Colors.CYAN}{'='*70}{Colors.RESET}\n")
        
        start_time = asyncio.get_event_loop().time()
        last_activity = start_time
        no_activity_count = 0
        log_buffer = ""
        
        try:
            while True:
                current_time = asyncio.get_event_loop().time()
                
                # 检查超时
                if current_time - start_time > timeout:
                    print(f"\n{Colors.YELLOW}⏰ 监控超时 ({timeout}秒){Colors.RESET}")
                    break
                
                # 读取新的日志数据
                chunk = await self.get_log_chunk()
                
                if chunk:
                    last_activity = current_time
                    no_activity_count = 0
                    
                    try:
                        # 解码数据并添加到缓冲区
                        text = chunk.decode('utf-8', errors='ignore')
                        log_buffer += text
                        
                        # 处理完整的行
                        while '\n' in log_buffer:
                            line, log_buffer = log_buffer.split('\n', 1)
                            line = line.strip()
                            
                            if line and self.is_selftest_log(line):
                                # 去重检查
                                line_id = hash(line)
                                if line_id not in self.displayed_lines:
                                    self.displayed_lines.add(line_id)
                                    self.log_count += 1
                                    
                                    # 格式化输出
                                    formatted = self.format_log_line(line)
                                    
                                    # 特殊标记和状态检测
                                    prefix = ""
                                    if 'Self-Test command received' in line or '📋 Self-Test command' in line:
                                        self.selftest_started = True
                                        prefix = f"{Colors.MAGENTA}🚀 {Colors.RESET}"
                                    elif 'Starting Self-Test' in line:
                                        prefix = f"{Colors.CYAN}🔬 {Colors.RESET}"
                                    elif 'Self-Test Complete' in line or 'Complete!' in line:
                                        self.selftest_completed = True
                                        prefix = f"{Colors.GREEN}🎉 {Colors.RESET}"
                                    elif '[PASS]' in line or 'PASS:' in line:
                                        prefix = f"{Colors.GREEN}✅ {Colors.RESET}"
                                    elif '[FAIL]' in line or 'FAIL:' in line:
                                        prefix = f"{Colors.RED}❌ {Colors.RESET}"
                                    elif '[WARN]' in line or 'WARNING:' in line:
                                        prefix = f"{Colors.YELLOW}⚠️  {Colors.RESET}"
                                    elif 'Device will restart' in line:
                                        prefix = f"{Colors.MAGENTA}🔄 {Colors.RESET}"
                                    elif '====' in line and 'SelfTest' in line:
                                        prefix = f"{Colors.CYAN}📋 {Colors.RESET}"
                                    
                                    print(f"{prefix}{formatted}")
                                    
                    except UnicodeDecodeError:
                        pass
                else:
                    no_activity_count += 1
                
                # 如果自检已完成且较长时间无新数据，退出
                if (self.selftest_started and self.selftest_completed and 
                    no_activity_count > 30):  # 6秒无新数据
                    print(f"\n{Colors.GREEN}✅ 自检完成，设备即将重启{Colors.RESET}")
                    break
                
                await asyncio.sleep(0.2)  # 200ms轮询间隔
                
        except KeyboardInterrupt:
            print(f"\n{Colors.YELLOW}⏸️  用户中断{Colors.RESET}")
    
    async def disconnect(self):
        """断开连接"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print(f"\n{Colors.CYAN}👋 已断开连接{Colors.RESET}")

async def main():
    import argparse
    parser = argparse.ArgumentParser(description='BLE自检触发器')
    parser.add_argument('--name', default='Blinko', help='设备名称')
    parser.add_argument('--timeout', type=int, default=60, help='监控超时（秒）')
    args = parser.parse_args()
    
    trigger = SelfTestTrigger()
    
    try:
        # 1. 连接设备
        if not await trigger.scan_and_connect(args.name):
            return
        
        # 2. 触发自检
        if not await trigger.trigger_selftest():
            return
        
        # 3. 等待一下让自检开始
        await asyncio.sleep(1)
        
        # 4. 监控自检日志
        await trigger.monitor_selftest_logs(args.timeout)
        
        # 5. 显示统计
        print(f"\n{Colors.CYAN}📊 共接收到 {trigger.log_count} 条自检日志{Colors.RESET}")
        
    except Exception as e:
        print(f"{Colors.RED}❌ 错误: {e}{Colors.RESET}")
        import traceback
        traceback.print_exc()
    finally:
        await trigger.disconnect()

if __name__ == '__main__':
    print(f"\n{Colors.CYAN}╔════════════════════════════════════════════════════════════════════╗{Colors.RESET}")
    print(f"{Colors.CYAN}║          BLE自检触发器 - 完整日志显示版                          ║{Colors.RESET}")
    print(f"{Colors.CYAN}║          Self-Test Trigger with Complete Log Display              ║{Colors.RESET}")
    print(f"{Colors.CYAN}╚════════════════════════════════════════════════════════════════════╝{Colors.RESET}\n")
    
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n⚠️ 用户中断")
    except Exception as e:
        print(f"\n❌ 发生错误: {e}")
        import traceback
        traceback.print_exc()