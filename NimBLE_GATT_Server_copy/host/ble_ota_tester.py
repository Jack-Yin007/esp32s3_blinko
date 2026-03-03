"""
BLE OTA Firmware Upgrade Test Script
Based on ESP-IDF BLE OTA Protocol

Protocol Details:
- Service UUID: 0x8018
- Characteristics:
  - RECV_FW_CHAR (0x8020): Write firmware data, receive ACK
  - PROGRESS_BAR_CHAR (0x8021): Read/notify progress
  - COMMAND_CHAR (0x8022): Send commands, receive ACK
  - CUSTOMER_CHAR (0x8023): Custom data (optional)

OTA Procedure:
1. Connect to BLE device
2. Enable notifications on characteristics
3. Send START_OTA command (0x0001) with firmware size
4. Send firmware in 4KB sectors (with packet sequence)
5. Each sector ends with CRC16 verification
6. Send END_OTA command (0x0002)
7. Device restarts with new firmware
"""

import asyncio
import struct
import time
import logging
from pathlib import Path
from typing import Optional, Tuple
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class BLEOTAUpgrader:
    """BLE OTA Firmware Upgrader for ESP32 devices"""
    
    # Service and Characteristic UUIDs
    OTA_SERVICE_UUID = "00008018-0000-1000-8000-00805f9b34fb"
    RECV_FW_CHAR_UUID = "00008020-0000-1000-8000-00805f9b34fb"
    PROGRESS_BAR_CHAR_UUID = "00008021-0000-1000-8000-00805f9b34fb"
    COMMAND_CHAR_UUID = "00008022-0000-1000-8000-00805f9b34fb"
    CUSTOMER_CHAR_UUID = "00008023-0000-1000-8000-00805f9b34fb"
    
    # Command IDs
    CMD_START_OTA = 0x0001
    CMD_END_OTA = 0x0002
    CMD_ACK = 0x0003
    
    # ACK Status
    ACK_SUCCESS = 0x0000
    ACK_CRC_ERROR = 0x0001
    ACK_SECTOR_INDEX_ERROR = 0x0002
    ACK_PAYLOAD_LENGTH_ERROR = 0x0003
    
    # Protocol constants
    SECTOR_SIZE = 4096  # 4KB per sector
    COMMAND_PACKET_SIZE = 20
    MAX_PACKET_SEQ = 0xFF  # Last packet marker
    
    def __init__(self, device_name: Optional[str] = None, device_address: Optional[str] = None):
        """
        Initialize BLE OTA Upgrader
        
        Args:
            device_name: BLE device name to search for (e.g., "ESP-C919", "Blinko")
            device_address: BLE device MAC address (if known)
        """
        self.device_name = device_name
        self.device_address = device_address
        self.client: Optional[BleakClient] = None
        self.mtu_size = 23  # Default MTU, will be updated after connection
        
        # Notification state
        self.recv_fw_ack_received = asyncio.Event()
        self.command_ack_received = asyncio.Event()
        self.last_ack_status = None
        self.progress_percent = 0
        
    @staticmethod
    def crc16_ccitt(data: bytes) -> int:
        """
        Calculate CRC16-CCITT checksum
        
        Args:
            data: Input data bytes
            
        Returns:
            CRC16 value
        """
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
    
    def _build_command_packet(self, command_id: int, payload: bytes = b'') -> bytes:
        """
        Build a command packet
        
        Format: [Command_ID(2)] [Payload(16)] [CRC16(2)]
        
        Args:
            command_id: Command ID (0x0001, 0x0002, etc.)
            payload: Command payload (max 16 bytes)
            
        Returns:
            20-byte command packet
        """
        # Command packet: 2 bytes ID + 16 bytes payload + 2 bytes CRC
        packet = bytearray(20)
        
        # Command ID (little-endian)
        struct.pack_into('<H', packet, 0, command_id)
        
        # Payload (pad with zeros if needed)
        payload_len = min(len(payload), 16)
        packet[2:2+payload_len] = payload[:payload_len]
        
        # CRC16 of first 18 bytes
        crc = self.crc16_ccitt(bytes(packet[:18]))
        struct.pack_into('<H', packet, 18, crc)
        
        return bytes(packet)
    
    def _build_firmware_packet(self, sector_index: int, packet_seq: int, 
                               payload: bytes) -> bytes:
        """
        Build a firmware data packet
        
        Format: [Sector_Index(2)] [Packet_Seq(1)] [Payload(MTU_size-4)]
        
        Args:
            sector_index: Sector number (0, 1, 2, ...)
            packet_seq: Packet sequence in sector (0xFF for last packet)
            payload: Firmware data payload
            
        Returns:
            Firmware packet bytes
        """
        packet = bytearray()
        
        # Sector index (little-endian)
        packet.extend(struct.pack('<H', sector_index))
        
        # Packet sequence
        packet.append(packet_seq)
        
        # Payload
        packet.extend(payload)
        
        return bytes(packet)
    
    async def scan_devices(self, timeout: float = 5.0) -> list:
        """
        Scan for BLE devices
        
        Args:
            timeout: Scan duration in seconds
            
        Returns:
            List of discovered devices
        """
        logger.info(f"Scanning for BLE devices (timeout: {timeout}s)...")
        devices = await BleakScanner.discover(timeout=timeout)
        
        logger.info(f"Found {len(devices)} devices:")
        for device in devices:
            logger.info(f"  - {device.name or 'Unknown'} ({device.address})")
        
        return devices
    
    async def connect(self, timeout: float = 10.0) -> bool:
        """
        Connect to BLE device
        
        Args:
            timeout: Connection timeout in seconds
            
        Returns:
            True if connected successfully
        """
        try:
            # Find device if address not provided
            if not self.device_address:
                logger.info(f"Searching for device: {self.device_name}")
                devices = await BleakScanner.discover(timeout=5.0)
                
                for device in devices:
                    if device.name == self.device_name:
                        self.device_address = device.address
                        logger.info(f"Found device at {self.device_address}")
                        break
                
                if not self.device_address:
                    logger.error(f"Device '{self.device_name}' not found")
                    return False
            
            # Connect to device
            logger.info(f"Connecting to {self.device_address}...")
            self.client = BleakClient(self.device_address, timeout=timeout)
            await self.client.connect()
            
            if self.client.is_connected:
                logger.info(f"Connected successfully!")
                
                # Get MTU size
                try:
                    self.mtu_size = self.client.mtu_size
                    logger.info(f"MTU size: {self.mtu_size}")
                except:
                    logger.warning("Could not get MTU size, using default 23")
                
                # List services
                await self._list_services()
                
                return True
            else:
                logger.error("Connection failed")
                return False
                
        except BleakError as e:
            logger.error(f"Connection error: {e}")
            return False
    
    async def _list_services(self):
        """List all services and characteristics"""
        logger.info("Available services:")
        for service in self.client.services:
            logger.info(f"  Service: {service.uuid}")
            for char in service.characteristics:
                properties = ', '.join(char.properties)
                logger.info(f"    Characteristic: {char.uuid} [{properties}]")
    
    async def disconnect(self):
        """Disconnect from BLE device"""
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            logger.info("Disconnected")
    
    def _handle_recv_fw_notification(self, sender, data: bytearray):
        """
        Handle RECV_FW_CHAR notification (ACK for firmware data)
        
        Format: [Sector_Index(2)] [ACK_Status(2)] [... padding ...] [CRC16(2)]
        """
        if len(data) != 20:
            logger.warning(f"Invalid ACK packet length: {len(data)}")
            return
        
        sector_index = struct.unpack('<H', data[0:2])[0]
        ack_status = struct.unpack('<H', data[2:4])[0]
        crc = struct.unpack('<H', data[18:20])[0]
        
        # Verify CRC
        calculated_crc = self.crc16_ccitt(data[:18])
        if crc != calculated_crc:
            logger.error(f"ACK CRC mismatch: expected {calculated_crc:04X}, got {crc:04X}")
            return
        
        self.last_ack_status = ack_status
        
        if ack_status == self.ACK_SUCCESS:
            logger.info(f"✓ Sector {sector_index} ACK: SUCCESS")
        elif ack_status == self.ACK_CRC_ERROR:
            logger.error(f"✗ Sector {sector_index} ACK: CRC ERROR")
        elif ack_status == self.ACK_SECTOR_INDEX_ERROR:
            expected_sector = struct.unpack('<H', data[4:6])[0]
            logger.error(f"✗ Sector {sector_index} ACK: INDEX ERROR (expected {expected_sector})")
        elif ack_status == self.ACK_PAYLOAD_LENGTH_ERROR:
            logger.error(f"✗ Sector {sector_index} ACK: LENGTH ERROR")
        else:
            logger.warning(f"? Sector {sector_index} ACK: Unknown status {ack_status:04X}")
        
        self.recv_fw_ack_received.set()
    
    def _handle_command_notification(self, sender, data: bytearray):
        """
        Handle COMMAND_CHAR notification (ACK for commands)
        
        Format: [Command_ID(2)] [Reply_Command_ID(2)] [Response(2)] [... padding ...] [CRC16(2)]
        """
        if len(data) != 20:
            logger.warning(f"Invalid command ACK packet length: {len(data)}")
            return
        
        command_id = struct.unpack('<H', data[0:2])[0]
        reply_cmd_id = struct.unpack('<H', data[2:4])[0]
        response = struct.unpack('<H', data[4:6])[0]
        crc = struct.unpack('<H', data[18:20])[0]
        
        # Verify CRC
        calculated_crc = self.crc16_ccitt(data[:18])
        if crc != calculated_crc:
            logger.error(f"Command ACK CRC mismatch: expected {calculated_crc:04X}, got {crc:04X}")
            return
        
        if command_id == self.CMD_ACK:
            cmd_name = {
                self.CMD_START_OTA: "START_OTA",
                self.CMD_END_OTA: "END_OTA"
            }.get(reply_cmd_id, f"0x{reply_cmd_id:04X}")
            
            if response == 0x0000:
                logger.info(f"✓ Command {cmd_name} ACK: ACCEPTED")
                self.last_ack_status = self.ACK_SUCCESS
            elif response == 0x0001:
                logger.error(f"✗ Command {cmd_name} ACK: REJECTED")
                self.last_ack_status = self.ACK_CRC_ERROR
            else:
                logger.warning(f"? Command {cmd_name} ACK: Unknown response {response:04X}")
        
        self.command_ack_received.set()
    
    def _handle_progress_notification(self, sender, data: bytearray):
        """Handle PROGRESS_BAR_CHAR notification"""
        if len(data) >= 1:
            self.progress_percent = data[0]
            logger.info(f"📊 Progress: {self.progress_percent}%")
    
    async def enable_notifications(self):
        """Enable notifications on relevant characteristics"""
        try:
            # Enable notification on RECV_FW_CHAR (firmware ACK)
            await self.client.start_notify(
                self.RECV_FW_CHAR_UUID,
                self._handle_recv_fw_notification
            )
            logger.info("✓ Enabled RECV_FW_CHAR notifications")
            
            # Enable notification on COMMAND_CHAR (command ACK)
            await self.client.start_notify(
                self.COMMAND_CHAR_UUID,
                self._handle_command_notification
            )
            logger.info("✓ Enabled COMMAND_CHAR notifications")
            
            # Enable notification on PROGRESS_BAR_CHAR (progress updates)
            await self.client.start_notify(
                self.PROGRESS_BAR_CHAR_UUID,
                self._handle_progress_notification
            )
            logger.info("✓ Enabled PROGRESS_BAR_CHAR notifications")
            
        except BleakError as e:
            logger.error(f"Failed to enable notifications: {e}")
            raise
    
    async def send_start_command(self, firmware_size: int) -> bool:
        """
        Send START_OTA command
        
        Args:
            firmware_size: Total firmware size in bytes
            
        Returns:
            True if command accepted
        """
        logger.info(f"Sending START_OTA command (firmware size: {firmware_size} bytes)...")
        
        # Build payload: firmware size (4 bytes, little-endian)
        payload = struct.pack('<I', firmware_size) + b'\x00' * 12
        
        # Build command packet
        packet = self._build_command_packet(self.CMD_START_OTA, payload)
        
        # Clear event and send command
        self.command_ack_received.clear()
        self.last_ack_status = None
        
        await self.client.write_gatt_char(self.COMMAND_CHAR_UUID, packet)
        
        # Wait for ACK (with timeout)
        try:
            await asyncio.wait_for(self.command_ack_received.wait(), timeout=5.0)
            return self.last_ack_status == self.ACK_SUCCESS
        except asyncio.TimeoutError:
            logger.error("START_OTA command ACK timeout")
            return False
    
    async def send_end_command(self) -> bool:
        """
        Send END_OTA command
        
        Returns:
            True if command accepted
        """
        logger.info("Sending END_OTA command...")
        
        # Build command packet (no payload)
        packet = self._build_command_packet(self.CMD_END_OTA, b'\x00' * 16)
        
        # Clear event and send command
        self.command_ack_received.clear()
        self.last_ack_status = None
        
        try:
            await self.client.write_gatt_char(self.COMMAND_CHAR_UUID, packet)
        except Exception as e:
            # Device may disconnect during reboot - this is expected
            logger.warning(f"END_OTA write error (device may be rebooting): {e}")
            logger.info("✓ OTA completed successfully - device rebooting...")
            return True
        
        # Wait for ACK (with timeout)
        try:
            await asyncio.wait_for(self.command_ack_received.wait(), timeout=5.0)
            if self.last_ack_status == self.ACK_SUCCESS:
                logger.info("✓ END_OTA command accepted - device rebooting...")
                return True
            else:
                logger.error(f"✗ END_OTA command rejected with status {self.last_ack_status}")
                return False
        except asyncio.TimeoutError:
            # Timeout can also mean device already started rebooting
            logger.warning("END_OTA ACK timeout (device may be rebooting)")
            logger.info("✓ OTA completed successfully - device rebooting...")
            return True
    
    async def send_firmware_sector(self, sector_index: int, sector_data: bytes) -> bool:
        """
        Send one 4KB firmware sector
        
        Args:
            sector_index: Sector number (0-based)
            sector_data: Sector data (up to 4096 bytes)
            
        Returns:
            True if sector sent successfully
        """
        sector_len = len(sector_data)
        if sector_len > self.SECTOR_SIZE:
            logger.error(f"Sector {sector_index}: data too large ({sector_len} > {self.SECTOR_SIZE})")
            return False
        
        # Save original data length
        original_data = sector_data
        original_len = sector_len
        
        # Pad sector to 4096 bytes with 0xFF for CRC calculation only
        if sector_len < self.SECTOR_SIZE:
            padded_data = sector_data + b'\xff' * (self.SECTOR_SIZE - sector_len)
        else:
            padded_data = sector_data
        
        # Calculate sector CRC based on padded 4096 bytes
        sector_crc = self.crc16_ccitt(padded_data)
        logger.info(f"Sector {sector_index}: {sector_len} bytes, CRC16=0x{sector_crc:04X}")
        
        # Debug: Print last sector details
        if sector_index == 189:
            logger.info(f"Sector 189 Debug:")
            logger.info(f"  Original data: {original_len} bytes")
            logger.info(f"  CRC calculated on: {len(padded_data)} bytes (padded)")
            logger.info(f"  Sending: {original_len} bytes (original data only)")
            logger.info(f"  First 32 bytes: {original_data[:32].hex()}")
            logger.info(f"  Last 32 bytes of original: {original_data[-32:].hex() if len(original_data) >= 32 else 'N/A'}")
        
        # Calculate max payload size per packet
        # MTU includes: ATT header(3 bytes) + packet header(3 bytes) + payload
        # Safe maximum is MTU - 6, limited to 244 bytes for compatibility
        max_payload = min(self.mtu_size - 6, 244)
        
        logger.info(f"Using max payload size: {max_payload} bytes (MTU={self.mtu_size})")
        
        packet_seq = 0
        offset = 0
        
        # Send only original data (not padded)
        while offset < original_len:
            # Determine if this is the last packet
            remaining = original_len - offset
            
            if remaining <= max_payload - 2:  # Reserve 2 bytes for CRC
                # Last packet: add CRC16 at the end
                payload = original_data[offset:]
                
                # Append CRC16 (no padding needed)
                payload += struct.pack('<H', sector_crc)
                
                # Mark as last packet
                packet_seq = self.MAX_PACKET_SEQ
                
            else:
                # Normal packet: use full max_payload size
                payload = original_data[offset:offset + max_payload]
            
            # Build and send firmware packet
            packet = self._build_firmware_packet(sector_index, packet_seq, payload)
            
            # Check packet size against MTU
            if len(packet) > self.mtu_size:
                logger.error(f"Packet too large: {len(packet)} > MTU {self.mtu_size}")
                logger.error(f"  Reducing max_payload from {max_payload} to {max_payload - 20}")
                max_payload -= 20
                offset = 0  # Restart this sector
                packet_seq = 0
                continue
            
            # Clear ACK event
            self.recv_fw_ack_received.clear()
            self.last_ack_status = None
            
            # Send packet with error handling
            try:
                await self.client.write_gatt_char(self.RECV_FW_CHAR_UUID, packet, response=False)
            except Exception as e:
                logger.error(f"Failed to send packet: {e}")
                logger.error(f"  Packet size: {len(packet)} bytes")
                logger.error(f"  Packet seq: {packet_seq}")
                return False
            
            # Update offset
            if packet_seq == self.MAX_PACKET_SEQ:
                offset += len(payload) - 2  # Subtract CRC bytes
            else:
                offset += len(payload)
            
            # Increment packet sequence (except for last packet)
            if packet_seq != self.MAX_PACKET_SEQ:
                packet_seq += 1
            
            # Small delay between packets (increased for stability)
            await asyncio.sleep(0.02)
            
            # If last packet, wait for ACK
            if packet_seq == self.MAX_PACKET_SEQ:
                try:
                    await asyncio.wait_for(self.recv_fw_ack_received.wait(), timeout=5.0)
                    
                    if self.last_ack_status != self.ACK_SUCCESS:
                        logger.error(f"Sector {sector_index} failed with status {self.last_ack_status}")
                        return False
                    
                except asyncio.TimeoutError:
                    logger.error(f"Sector {sector_index} ACK timeout")
                    return False
                
                break
        
        return True
    
    async def upload_firmware(self, firmware_path: str) -> bool:
        """
        Upload firmware file to device via BLE OTA
        
        Args:
            firmware_path: Path to firmware binary file (.bin)
            
        Returns:
            True if upload successful
        """
        # Read firmware file
        firmware_path = Path(firmware_path)
        if not firmware_path.exists():
            logger.error(f"Firmware file not found: {firmware_path}")
            return False
        
        firmware_data = firmware_path.read_bytes()
        firmware_size = len(firmware_data)
        
        logger.info(f"Firmware: {firmware_path.name}")
        logger.info(f"Size: {firmware_size} bytes ({firmware_size / 1024:.2f} KB)")
        
        # Calculate number of sectors
        num_sectors = (firmware_size + self.SECTOR_SIZE - 1) // self.SECTOR_SIZE
        logger.info(f"Sectors: {num_sectors}")
        
        # Send START_OTA command
        if not await self.send_start_command(firmware_size):
            logger.error("Failed to start OTA")
            return False
        
        # Send firmware sectors
        start_time = time.time()
        
        for sector_index in range(num_sectors):
            sector_offset = sector_index * self.SECTOR_SIZE
            sector_end = min(sector_offset + self.SECTOR_SIZE, firmware_size)
            sector_data = firmware_data[sector_offset:sector_end]
            
            logger.info(f"Uploading sector {sector_index + 1}/{num_sectors}...")
            
            if not await self.send_firmware_sector(sector_index, sector_data):
                logger.error(f"Failed to upload sector {sector_index}")
                return False
            
            # Show progress
            progress = ((sector_index + 1) / num_sectors) * 100
            elapsed = time.time() - start_time
            speed = (sector_offset + len(sector_data)) / elapsed / 1024
            logger.info(f"Progress: {progress:.1f}% | Speed: {speed:.2f} KB/s")
        
        # Send END_OTA command
        if not await self.send_end_command():
            logger.error("Failed to end OTA")
            return False
        
        elapsed_time = time.time() - start_time
        avg_speed = firmware_size / elapsed_time / 1024
        
        logger.info(f"✓ Firmware uploaded successfully!")
        logger.info(f"Total time: {elapsed_time:.2f}s | Average speed: {avg_speed:.2f} KB/s")
        logger.info("Device will restart with new firmware...")
        
        return True


async def main():
    """Main test function"""
    import argparse
    
    parser = argparse.ArgumentParser(description='BLE OTA Firmware Upgrade Test')
    parser.add_argument('--scan', action='store_true', help='Scan for BLE devices')
    parser.add_argument('--name', type=str, help='Device name to connect (e.g., ESP-C919, Blinko)')
    parser.add_argument('--address', type=str, help='Device MAC address')
    parser.add_argument('--firmware', type=str, help='Firmware binary file path (.bin)')
    
    args = parser.parse_args()
    
    # Scan mode
    if args.scan:
        upgrader = BLEOTAUpgrader()
        await upgrader.scan_devices(timeout=10.0)
        return
    
    # OTA mode
    if not args.firmware:
        logger.error("Please specify firmware file with --firmware")
        parser.print_help()
        return
    
    if not args.name and not args.address:
        logger.error("Please specify device name with --name or address with --address")
        parser.print_help()
        return
    
    # Create upgrader
    upgrader = BLEOTAUpgrader(device_name=args.name, device_address=args.address)
    
    try:
        # Connect to device
        if not await upgrader.connect(timeout=10.0):
            logger.error("Failed to connect")
            return
        
        # Enable notifications
        await upgrader.enable_notifications()
        
        # Small delay to ensure notifications are ready
        await asyncio.sleep(1.0)
        
        # Upload firmware
        success = await upgrader.upload_firmware(args.firmware)
        
        if success:
            logger.info("✓ OTA upgrade completed successfully!")
        else:
            logger.error("✗ OTA upgrade failed")
        
    except Exception as e:
        logger.error(f"Error: {e}", exc_info=True)
    
    finally:
        # Disconnect
        await upgrader.disconnect()


if __name__ == '__main__':
    asyncio.run(main())
