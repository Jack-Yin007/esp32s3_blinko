#!/usr/bin/env python3
"""
简化的OTA测试工具 - 用于调试
只发送START命令并等待ACK
"""

import asyncio
import struct
import logging
from bleak import BleakClient, BleakScanner

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# UUIDs
COMMAND_CHAR_UUID = "00008022-0000-1000-8000-00805f9b34fb"

# Commands
CMD_START_OTA = 0x0001
CMD_ACK = 0x0003

ack_received = asyncio.Event()
last_ack_data = None

def crc16_ccitt(data: bytes) -> int:
    """Calculate CRC16-CCITT"""
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

def build_command_packet(command_id: int, payload: bytes = b'') -> bytes:
    """Build 20-byte command packet"""
    packet = bytearray(20)
    
    # Command ID (little-endian)
    struct.pack_into('<H', packet, 0, command_id)
    
    # Payload (pad with zeros if needed)
    payload_len = min(len(payload), 16)
    packet[2:2+payload_len] = payload[:payload_len]
    
    # CRC16 of first 18 bytes
    crc = crc16_ccitt(bytes(packet[:18]))
    struct.pack_into('<H', packet, 18, crc)
    
    return bytes(packet)

def handle_notification(sender, data: bytearray):
    """Handle notification from COMMAND characteristic"""
    global last_ack_data
    
    logger.info(f"Received notification: {len(data)} bytes")
    logger.info(f"Raw data: {data.hex()}")
    
    if len(data) != 20:
        logger.warning(f"Invalid packet length: {len(data)}")
        return
    
    command_id = struct.unpack('<H', data[0:2])[0]
    reply_cmd_id = struct.unpack('<H', data[2:4])[0]
    response = struct.unpack('<H', data[4:6])[0]
    
    logger.info(f"  Command ID: 0x{command_id:04X}")
    logger.info(f"  Reply CMD:  0x{reply_cmd_id:04X}")
    logger.info(f"  Response:   0x{response:04X}")
    
    if command_id == CMD_ACK:
        logger.info(f"✓ ACK received for command 0x{reply_cmd_id:04X}, status: 0x{response:04X}")
        last_ack_data = data
        ack_received.set()

async def main():
    logger.info("Scanning for Blinko...")
    devices = await BleakScanner.discover(timeout=5.0)
    
    device = None
    for d in devices:
        if d.name == "Blinko":
            device = d
            break
    
    if not device:
        logger.error("Device not found")
        return
    
    logger.info(f"Found device at {device.address}")
    
    async with BleakClient(device.address) as client:
        logger.info(f"Connected! MTU: {client.mtu_size}")
        
        # Enable notifications
        logger.info("Enabling notifications on COMMAND characteristic...")
        await client.start_notify(COMMAND_CHAR_UUID, handle_notification)
        logger.info("✓ Notifications enabled")
        
        await asyncio.sleep(1.0)  # Wait for notification setup
        
        # Send START_OTA command
        firmware_size = 774704
        payload = struct.pack('<I', firmware_size) + b'\x00' * 12
        packet = build_command_packet(CMD_START_OTA, payload)
        
        logger.info(f"Sending START_OTA command (size: {firmware_size})...")
        logger.info(f"Packet: {packet.hex()}")
        
        ack_received.clear()
        await client.write_gatt_char(COMMAND_CHAR_UUID, packet)
        
        # Wait for ACK
        try:
            await asyncio.wait_for(ack_received.wait(), timeout=10.0)
            logger.info("✓ ACK received successfully!")
        except asyncio.TimeoutError:
            logger.error("✗ ACK timeout")
        
        await asyncio.sleep(1.0)

if __name__ == "__main__":
    asyncio.run(main())
