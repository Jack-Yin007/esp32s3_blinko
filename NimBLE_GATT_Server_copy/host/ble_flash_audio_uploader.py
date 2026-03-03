#!/usr/bin/env python3
"""
BLE Flash Audio Uploader
通过BLE文件系统服务将本地audio目录的所有WAV文件上传到ESP32的/flash/audio目录

使用方法:
    python ble_flash_audio_uploader.py [--address MAC地址] [--audio-dir 音频目录]

功能:
    1. 扫描并连接ESP32设备
    2. 自动创建/flash/audio目录（如果不存在）
    3. 批量上传所有WAV文件
    4. 显示上传进度和统计信息
"""

import asyncio
import os
import sys
import argparse
from pathlib import Path
from bleak import BleakClient, BleakScanner
import struct

# BLE文件系统服务UUID（与固件中fs_service.c定义一致）
FS_SERVICE_UUID = "6e400020-b5a3-f393-e0a9-e50e24dcca9e"
FS_COMMAND_CHAR_UUID = "6e400021-b5a3-f393-e0a9-e50e24dcca9e"  # Path (Write)
FS_DATA_CHAR_UUID = "6e400022-b5a3-f393-e0a9-e50e24dcca9e"     # Data (Read)
FS_RESPONSE_CHAR_UUID = "6e400023-b5a3-f393-e0a9-e50e24dcca9e" # Control (Write)

# 文件系统命令（匹配固件fs_service.h）
CMD_LIST_DIR = 0x01
CMD_STAT = 0x02
CMD_DELETE_FILE = 0x03
CMD_WRITE_FILE = 0x04
CMD_MKDIR = 0x05

# MTU设置
BLE_MTU = 512
MAX_DATA_SIZE = BLE_MTU - 3  # 减去BLE ATT开销

class BLEFlashAudioUploader:
    def __init__(self, device_address=None, max_scan_retries=3, max_connect_retries=3):
        self.device_address = device_address
        self.device = None  # 保存扫描到的设备对象
        self.client = None
        self.response_event = asyncio.Event()
        self.last_response = None
        self.max_scan_retries = max_scan_retries
        self.max_connect_retries = max_connect_retries
        
    async def scan_devices(self):
        """扫描附近的BLE设备（带重试机制）"""
        for attempt in range(1, self.max_scan_retries + 1):
            print(f"🔍 扫描BLE设备... (尝试 {attempt}/{self.max_scan_retries})")
            try:
                devices = await BleakScanner.discover(timeout=10.0)  # 增加到10秒
                
                # 过滤出ESP32设备（通常名称包含"ESP32"或"nimble"或"Blinko"）
                esp_devices = []
                for device in devices:
                    if device.name and ("ESP32" in device.name or "nimble" in device.name.lower() or "Blinko" in device.name):
                        esp_devices.append(device)
                        print(f"  ✓ 发现设备: {device.name} ({device.address})")
                
                if esp_devices:
                    return esp_devices
                
                if attempt < self.max_scan_retries:
                    wait_time = 2 * attempt  # 递增等待时间: 2秒, 4秒, 6秒...
                    print(f"  ⚠️ 未找到设备，{wait_time}秒后重试...")
                    await asyncio.sleep(wait_time)
            except Exception as e:
                print(f"  ⚠️ 扫描出错: {e}")
                if attempt < self.max_scan_retries:
                    wait_time = 2 * attempt
                    print(f"  等待{wait_time}秒后重试...")
                    await asyncio.sleep(wait_time)
        
        return []
    
    async def connect(self):
        """连接到ESP32设备（带重试机制）"""
        if not self.device_address:
            devices = await self.scan_devices()
            if not devices:
                print("❌ 未找到ESP32设备")
                return False
            # 保存设备对象而不仅仅是地址
            self.device = devices[0]
            self.device_address = devices[0].address
            print(f"📱 自动选择设备: {devices[0].name} ({self.device_address})")
        
        for attempt in range(1, self.max_connect_retries + 1):
            print(f"🔗 连接到 {self.device_address}... (尝试 {attempt}/{self.max_connect_retries})")
            # 如果有设备对象，直接用设备对象连接
            if hasattr(self, 'device') and self.device:
                self.client = BleakClient(self.device)
            else:
                self.client = BleakClient(self.device_address)
            
            try:
                await self.client.connect(timeout=15.0)  # 增加连接超时
                print("✅ 连接成功")
                
                # 确保服务发现完成
                print("🔍 发现服务...")
                services = self.client.services
                fs_service = None
                for service in services:
                    if service.uuid == FS_SERVICE_UUID:
                        fs_service = service
                        break
                
                if not fs_service:
                    print(f"  ⚠️ 未找到文件系统服务 {FS_SERVICE_UUID}")
                else:
                    print(f"  ✓ 找到文件系统服务，包含 {len(fs_service.characteristics)} 个特征")
                
                # 不尝试配对，因为会弹出Windows对话框
                # try:
                #     await self.client.pair()
                # except:
                #     pass
                
                # 启用响应通知（如果需要）
                # await self.client.start_notify(FS_RESPONSE_CHAR_UUID, self._notification_handler)
                
                return True
            except Exception as e:
                print(f"  ⚠️ 连接失败: {e}")
                if attempt < self.max_connect_retries:
                    wait_time = 2 * attempt
                    print(f"  等待{wait_time}秒后重试...")
                    await asyncio.sleep(wait_time)
                    # 清理之前的client
                    try:
                        if self.client and self.client.is_connected:
                            await self.client.disconnect()
                    except:
                        pass
        
        print("❌ 连接失败：已达到最大重试次数")
        return False
    
    async def mkdir(self, path):
        """创建目录"""
        print(f"📁 创建目录: {path}")
        try:
            # 检查连接状态
            if not self.client or not self.client.is_connected:
                print(f"  ⚠️ 连接已断开，尝试重新连接...")
                if not await self.connect():
                    return False
            
            # 1. 设置路径
            await self.client.write_gatt_char(FS_COMMAND_CHAR_UUID, path.encode('utf-8'), response=False)
            await asyncio.sleep(0.05)
            
            # 2. 发送MKDIR命令
            await self.client.write_gatt_char(FS_RESPONSE_CHAR_UUID, bytes([CMD_MKDIR]), response=False)
            await asyncio.sleep(0.1)
            
            print(f"  ✓ 目录创建命令已发送")
            return True
        except Exception as e:
            print(f"  ❌ 创建目录失败: {e}")
            return False
    
    async def write_file(self, remote_path, local_file_path):
        """上传文件到ESP32"""
        if not os.path.exists(local_file_path):
            print(f"❌ 本地文件不存在: {local_file_path}")
            return False
        
        file_size = os.path.getsize(local_file_path)
        print(f"📤 上传: {os.path.basename(local_file_path)} ({file_size} bytes) -> {remote_path}")
        
        try:
            # 1. 设置文件路径
            await self.client.write_gatt_char(FS_COMMAND_CHAR_UUID, remote_path.encode('utf-8'), response=False)
            await asyncio.sleep(0.05)
            
            # 2. 发送WRITE命令
            await self.client.write_gatt_char(FS_RESPONSE_CHAR_UUID, bytes([CMD_WRITE_FILE]), response=False)
            await asyncio.sleep(0.1)
            
            # 3. 发送文件大小（4字节，little-endian）
            size_data = struct.pack('<I', file_size)
            await self.client.write_gatt_char(FS_DATA_CHAR_UUID, size_data, response=False)
            await asyncio.sleep(0.05)
            
            # 4. 分块发送文件数据
            with open(local_file_path, 'rb') as f:
                offset = 0
                chunk_size = 240  # 保守的数据块大小
                while offset < file_size:
                    chunk = f.read(chunk_size)
                    if not chunk:
                        break
                    
                    await self.client.write_gatt_char(FS_DATA_CHAR_UUID, chunk, response=False)
                    offset += len(chunk)
                    
                    # 显示进度
                    progress = (offset / file_size) * 100
                    print(f"  进度: {progress:.1f}% ({offset}/{file_size} bytes)", end='\r')
                    
                    await asyncio.sleep(0.03)  # 避免发送过快
            
            print(f"\n  ✓ 上传完成")
            await asyncio.sleep(0.2)  # 等待设备完成写入
            return True
            
        except Exception as e:
            print(f"\n  ❌ 上传失败: {e}")
            return False
    
    async def list_dir(self, path):
        """列出目录内容"""
        print(f"📂 列出目录: {path}")
        try:
            # 1. 设置路径
            await self.client.write_gatt_char(FS_COMMAND_CHAR_UUID, path.encode('utf-8'), response=False)
            await asyncio.sleep(0.05)
            
            # 2. 发送LIST命令
            await self.client.write_gatt_char(FS_RESPONSE_CHAR_UUID, bytes([CMD_LIST_DIR]), response=False)
            await asyncio.sleep(0.2)
            
            # 3. 读取结果
            data = await self.client.read_gatt_char(FS_DATA_CHAR_UUID)
            if data and len(data) > 0:
                count = data[0]
                print(f"  ✓ 目录包含 {count} 个条目")
                return True
        except Exception as e:
            print(f"  ❌ 列出目录失败: {e}")
        return False
    
    async def upload_audio_directory(self, audio_dir, remote_base_path="/flash/audio"):
        """批量上传音频目录"""
        audio_path = Path(audio_dir)
        if not audio_path.exists():
            print(f"❌ 音频目录不存在: {audio_dir}")
            return False
        
        # 获取所有WAV文件（去重，避免Windows不区分大小写导致重复）
        wav_files = list(set(list(audio_path.glob("*.wav")) + list(audio_path.glob("*.WAV"))))
        if not wav_files:
            print(f"❌ 音频目录中没有WAV文件: {audio_dir}")
            return False
        
        # 按文件名排序以保持一致性
        wav_files.sort(key=lambda x: x.name.lower())
        
        print(f"📁 找到 {len(wav_files)} 个WAV文件")
        print("=" * 60)
        
        # 确保远程目录存在（递归创建父目录）
        path_parts = remote_base_path.strip('/').split('/')
        current_path = ""
        for part in path_parts:
            current_path += "/" + part
            await self.mkdir(current_path)
            await asyncio.sleep(0.1)
        
        # 上传所有文件
        success_count = 0
        failed_count = 0
        
        for i, wav_file in enumerate(wav_files, 1):
            remote_path = f"{remote_base_path}/{wav_file.name}"
            print(f"\n[{i}/{len(wav_files)}] 上传 {wav_file.name}")
            
            try:
                if await self.write_file(remote_path, str(wav_file)):
                    success_count += 1
                else:
                    failed_count += 1
                    print(f"  ❌ 上传失败")
            except Exception as e:
                failed_count += 1
                print(f"  ❌ 上传异常: {e}")
            
            await asyncio.sleep(0.2)  # 文件间延迟
        
        print("\n" + "=" * 60)
        print(f"📊 上传完成: 成功 {success_count}, 失败 {failed_count}")
        
        # 验证上传结果
        print("\n🔍 验证上传结果...")
        await self.list_dir(remote_base_path)
        
        return failed_count == 0
    
    async def disconnect(self):
        """断开连接"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("👋 已断开连接")


async def main():
    parser = argparse.ArgumentParser(description="BLE Flash Audio Uploader")
    parser.add_argument("--address", help="ESP32的BLE MAC地址（可选，自动扫描）")
    parser.add_argument("--audio-dir", default="./audio", help="本地音频文件目录（默认: ./audio）")
    parser.add_argument("--path", default="/flash/audio", help="设备上的目标路径（默认: /flash/audio）")
    parser.add_argument("--scan-retries", type=int, default=3, help="扫描重试次数（默认: 3）")
    parser.add_argument("--connect-retries", type=int, default=3, help="连接重试次数（默认: 3）")
    args = parser.parse_args()
    
    # 获取音频目录的绝对路径
    script_dir = Path(__file__).parent.parent  # 回到项目根目录
    audio_dir = script_dir / args.audio_dir
    
    print("=" * 60)
    print("  BLE Flash Audio Uploader")
    print("  通过BLE上传WAV文件到ESP32 Flash")
    print("=" * 60)
    print(f"本地目录: {audio_dir}")
    print(f"设备路径: {args.path}")
    print(f"扫描重试: {args.scan_retries}次, 连接重试: {args.connect_retries}次")
    print()
    
    uploader = BLEFlashAudioUploader(
        args.address,
        max_scan_retries=args.scan_retries,
        max_connect_retries=args.connect_retries
    )
    
    try:
        # 连接设备
        if not await uploader.connect():
            return 1
        
        # 上传音频文件
        success = await uploader.upload_audio_directory(str(audio_dir), args.path)
        
        # 断开连接
        await uploader.disconnect()
        
        return 0 if success else 1
        
    except KeyboardInterrupt:
        print("\n⚠️ 用户中断")
        await uploader.disconnect()
        return 1
    except Exception as e:
        print(f"\n❌ 错误: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
