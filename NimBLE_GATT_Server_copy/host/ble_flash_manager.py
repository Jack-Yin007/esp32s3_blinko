#!/usr/bin/env python3
"""
BLE Flash Audio Manager
=======================
Manage audio files in Flash partition via BLE.

Usage:
    python ble_flash_manager.py --address MAC_ADDRESS --clear
    python ble_flash_manager.py --address MAC_ADDRESS --copy
    python ble_flash_manager.py --name Blinko --refresh

Commands:
    --clear   : Delete all WAV files in /flash/audio
    --copy    : Copy audio files from SD card to Flash (lowercase .wav)
    --refresh : Clear old files and copy new ones (--clear + --copy)

Author: GitHub Copilot
Date: 2025-12-05
"""

import asyncio
import argparse
import sys
from bleak import BleakClient, BleakScanner

# BLE UUIDs for LOG Service (reusing for commands)
LOG_SERVICE_UUID = "6E400010-B5A3-F393-E0A9-E50E24DCCA9E"
LOG_CONTROL_CHR_UUID = "6E400013-B5A3-F393-E0A9-E50E24DCCA9E"

# Commands
LOG_CMD_FLASH_CLEAR = 0x10  # Clear Flash audio directory
LOG_CMD_FLASH_COPY = 0x11   # Copy audio files from SD to Flash

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

async def send_command(address, command):
    """Send command to device"""
    print(f"📱 Connecting to {address}...")
    
    async with BleakClient(address, timeout=10.0) as client:
        if not client.is_connected:
            print("❌ Failed to connect")
            return False
        
        print(f"✓ Connected")
        
        # Send command
        cmd_name = "CLEAR" if command == LOG_CMD_FLASH_CLEAR else "COPY"
        print(f"📤 Sending {cmd_name} command...")
        
        await client.write_gatt_char(LOG_CONTROL_CHR_UUID, bytes([command]))
        
        # Wait for operation to complete
        print(f"⏳ Waiting for operation to complete...")
        await asyncio.sleep(3.0 if command == LOG_CMD_FLASH_COPY else 1.0)
        
        print(f"✓ {cmd_name} command sent successfully")
        print(f"📱 Check device logs for results")
        
        return True

async def main():
    parser = argparse.ArgumentParser(
        description='Manage Flash audio files via BLE',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Clear Flash audio directory
  python ble_flash_manager.py --address 90:E5:B1:AE:E4:56 --clear
  
  # Copy audio files from SD to Flash (lowercase)
  python ble_flash_manager.py --address 90:E5:B1:AE:E4:56 --copy
  
  # Clear and copy (refresh)
  python ble_flash_manager.py --name Blinko --refresh
        '''
    )
    
    parser.add_argument('--address', '-a', help='BLE device MAC address')
    parser.add_argument('--name', '-n', help='BLE device name (will scan)')
    parser.add_argument('--clear', action='store_true', help='Clear Flash audio directory')
    parser.add_argument('--copy', action='store_true', help='Copy audio files from SD to Flash')
    parser.add_argument('--refresh', action='store_true', help='Clear + Copy (full refresh)')
    
    args = parser.parse_args()
    
    # Get device address
    address = args.address
    if not address and args.name:
        address = await find_device_by_name(args.name)
    
    if not address:
        parser.print_help()
        sys.exit(1)
    
    # Determine operation
    if args.refresh:
        print("🔄 Refreshing Flash audio files...")
        print("\n" + "="*50)
        print("Step 1: Clearing old files")
        print("="*50)
        success = await send_command(address, LOG_CMD_FLASH_CLEAR)
        if not success:
            sys.exit(1)
        
        print("\n" + "="*50)
        print("Step 2: Copying new files (lowercase)")
        print("="*50)
        success = await send_command(address, LOG_CMD_FLASH_COPY)
        if not success:
            sys.exit(1)
        
        print("\n" + "="*50)
        print("✅ Refresh complete!")
        print("="*50)
        print("\nVerify with:")
        print(f"  python ble_file_explorer.py --address {address} --path /flash/audio")
        
    elif args.clear:
        await send_command(address, LOG_CMD_FLASH_CLEAR)
        
    elif args.copy:
        await send_command(address, LOG_CMD_FLASH_COPY)
        
    else:
        parser.print_help()
        print("\n❌ Please specify an operation: --clear, --copy, or --refresh")
        sys.exit(1)

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        sys.exit(0)
