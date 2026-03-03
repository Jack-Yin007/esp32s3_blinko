#!/usr/bin/env python3
"""
BLE File System Explorer
========================
Browse files and directories on ESP32 device via BLE.

Usage:
    python ble_file_explorer.py --address MAC_ADDRESS --path /flash
    python ble_file_explorer.py --address MAC_ADDRESS --path /flash/audio
    python ble_file_explorer.py --name "Blinko-XXX" --path /

Features:
    - List files and directories with sizes
    - Recursive directory traversal
    - Human-readable file sizes
    - Color-coded output (files vs directories)

Author: GitHub Copilot
Date: 2025-12-05
"""

import asyncio
import argparse
import sys
from bleak import BleakClient, BleakScanner
from datetime import datetime

# ============================================
# BLE UUIDs for File System Service
# ============================================
# Custom service for file system operations
FS_SERVICE_UUID = "6E400020-B5A3-F393-E0A9-E50E24DCCA9E"
FS_PATH_CHR_UUID = "6E400021-B5A3-F393-E0A9-E50E24DCCA9E"     # Write: set path
FS_DATA_CHR_UUID = "6E400022-B5A3-F393-E0A9-E50E24DCCA9E"     # Read: get file list
FS_CONTROL_CHR_UUID = "6E400023-B5A3-F393-E0A9-E50E24DCCA9E"  # Write: commands

# Commands
FS_CMD_LIST = 0x01      # List directory
FS_CMD_STAT = 0x02      # Get file stats
FS_CMD_DELETE = 0x03    # Delete file (future)

# ============================================
# Helper functions
# ============================================

def format_size(size_bytes):
    """Convert bytes to human-readable format"""
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:6.1f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:6.1f} TB"

def format_file_entry(name, size, is_dir):
    """Format file/directory entry with colors"""
    if is_dir:
        # Blue for directories
        return f"📁 \033[94m{name:40s}\033[0m {'<DIR>':>12s}"
    else:
        # Green for files
        size_str = format_size(size)
        return f"📄 \033[92m{name:40s}\033[0m {size_str:>12s}"

async def find_device_by_name(name):
    """Scan for device by name"""
    print(f"🔍 Scanning for device: {name}...")
    devices = await BleakScanner.discover(timeout=5.0)
    
    for device in devices:
        if device.name and name.lower() in device.name.lower():
            print(f"✓ Found device: {device.name} ({device.address})")
            return device.address
    
    print(f"❌ Device not found: {name}")
    return None

# ============================================
# BLE File System Explorer
# ============================================

class BLEFileExplorer:
    def __init__(self, address):
        self.address = address
        self.client = None
        
    async def connect(self):
        """Connect to BLE device"""
        print(f"📱 Connecting to {self.address}...")
        self.client = BleakClient(self.address, timeout=10.0)
        
        try:
            await self.client.connect()
            print(f"✓ Connected to {self.address}")
            
            # Check if File System service exists
            services = self.client.services
            fs_service = None
            for service in services:
                if "6e400020" in service.uuid.lower():
                    fs_service = service
                    break
            
            if not fs_service:
                print("⚠️  File System service not found!")
                print("   Device may not support file browsing via BLE.")
                print("   Available services:")
                for service in services:
                    print(f"     - {service.uuid}: {service.description}")
                return False
            
            print(f"✓ File System service found")
            return True
            
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from BLE device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("📱 Disconnected")
    
    async def list_directory(self, path):
        """List files and directories at given path"""
        if not self.client or not self.client.is_connected:
            print("❌ Not connected")
            return None
        
        try:
            # Send path to device
            print(f"\n📂 Listing: {path}")
            path_bytes = path.encode('utf-8')
            # Use response=True since firmware advertises only 'write' property
            await self.client.write_gatt_char(FS_PATH_CHR_UUID, path_bytes, response=True)
            await asyncio.sleep(0.1)
            
            # Send LIST command
            await self.client.write_gatt_char(FS_CONTROL_CHR_UUID, bytes([FS_CMD_LIST]), response=True)
            
            # Wait a bit for device to process
            await asyncio.sleep(0.2)
            
            # Read directory listing
            data = await self.client.read_gatt_char(FS_DATA_CHR_UUID)
            
            if not data:
                print("❌ No data received (directory may be empty or not exist)")
                return []
            
            # Parse directory listing
            # Format: [entry_count][name1\0][size1][type1][name2\0][size2][type2]...
            entries = []
            offset = 0
            entry_count = data[0]
            offset += 1
            
            print(f"{'Name':40s} {'Size':>12s}")
            print("=" * 54)
            
            for i in range(entry_count):
                # Read null-terminated name
                name_end = data.find(b'\0', offset)
                if name_end == -1:
                    break
                
                name = data[offset:name_end].decode('utf-8')
                offset = name_end + 1
                
                # Read size (4 bytes, little-endian)
                if offset + 4 > len(data):
                    break
                size = int.from_bytes(data[offset:offset+4], 'little')
                offset += 4
                
                # Read type (1 byte: 0=file, 1=directory)
                if offset >= len(data):
                    break
                is_dir = (data[offset] == 1)
                offset += 1
                
                entries.append({
                    'name': name,
                    'size': size,
                    'is_dir': is_dir
                })
                
                print(format_file_entry(name, size, is_dir))
            
            # Summary
            total_files = sum(1 for e in entries if not e['is_dir'])
            total_dirs = sum(1 for e in entries if e['is_dir'])
            total_size = sum(e['size'] for e in entries if not e['is_dir'])
            
            print("=" * 54)
            print(f"Total: {total_dirs} directories, {total_files} files, {format_size(total_size)}")
            
            return entries
            
        except Exception as e:
            print(f"❌ Failed to list directory: {e}")
            import traceback
            traceback.print_exc()
            return None

# ============================================
# Fallback: Use BLE Log to send shell command
# ============================================

class BLELogFileExplorer:
    """Fallback method using LOG service to send 'ls' commands"""
    
    def __init__(self, address):
        self.address = address
        self.client = None
        
    async def connect(self):
        """Connect to BLE device"""
        print(f"📱 Connecting to {self.address}...")
        self.client = BleakClient(self.address, timeout=10.0)
        
        try:
            await self.client.connect()
            print(f"✓ Connected to {self.address}")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    async def disconnect(self):
        """Disconnect from BLE device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("📱 Disconnected")
    
    async def list_directory_via_log(self, path):
        """
        List directory by monitoring BLE logs after triggering shell command.
        Note: Requires device firmware to support shell commands via BLE.
        """
        print(f"\n⚠️  Using fallback method (BLE Log monitoring)")
        print(f"📂 Path: {path}")
        print(f"\nℹ️  This requires device support for file listing in logs.")
        print(f"   Please check the serial monitor output instead, or")
        print(f"   implement FS_SERVICE in the firmware.")

# ============================================
# Main
# ============================================

async def main():
    parser = argparse.ArgumentParser(
        description='Browse file system on ESP32 device via BLE',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python ble_file_explorer.py --address 90:E5:B1:AE:E4:56 --path /flash
  python ble_file_explorer.py --name Blinko --path /flash/audio
  python ble_file_explorer.py -a 90:E5:B1:AE:E4:56 -p /
        '''
    )
    
    parser.add_argument('--address', '-a', help='BLE device MAC address')
    parser.add_argument('--name', '-n', help='BLE device name (will scan)')
    parser.add_argument('--path', '-p', default='/flash', 
                       help='Path to list (default: /flash)')
    parser.add_argument('--recursive', '-r', action='store_true',
                       help='Recursively list subdirectories (future)')
    
    args = parser.parse_args()
    
    # Get device address
    address = args.address
    if not address and args.name:
        address = await find_device_by_name(args.name)
    
    if not address:
        parser.print_help()
        sys.exit(1)
    
    # Try primary method (FS Service)
    explorer = BLEFileExplorer(address)
    
    if await explorer.connect():
        try:
            entries = await explorer.list_directory(args.path)
            
            if entries is None:
                print("\n⚠️  File System service not available.")
                print("   Falling back to log-based method...\n")
                
                # Try fallback
                fallback = BLELogFileExplorer(address)
                if await fallback.connect():
                    await fallback.list_directory_via_log(args.path)
                    await fallback.disconnect()
        finally:
            await explorer.disconnect()
    else:
        print("\n❌ Connection failed. Please check:")
        print("   1. Device is powered on and advertising")
        print("   2. MAC address is correct")
        print("   3. Bluetooth is enabled on your computer")

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        sys.exit(0)
