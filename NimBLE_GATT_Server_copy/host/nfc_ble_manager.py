"""
NFC BLE Manager - 通过BLE管理NFC名片
"""
import asyncio
import struct
from bleak import BleakClient, BleakScanner

# BLE配置
DEVICE_NAME = "Blinko"
B11_NFC_CHAR_UUID = "6e400006-b5a3-f393-e0a9-e50e24dcca9e"

# NFC命令
NFC_CMD_SET_MY_CARD = 0x20
NFC_CMD_GET_MY_CARD = 0x21
NFC_CMD_GET_FRIEND_CARD = 0x22
NFC_CMD_GET_FRIEND_COUNT = 0x23
NFC_CMD_CARD_EXCHANGED = 0x30  # 设备→手机通知

# NFC状态码
NFC_STATUS_OK = 0x00
NFC_STATUS_ERROR = 0x01
NFC_STATUS_NO_FRIEND = 0x02

class NFCBLEManager:
    def __init__(self):
        self.client = None
        self.response = None
        self.card_exchange_callback = None
        
    async def scan_for_device(self, max_attempts=5):
        print(f"Scanning for '{DEVICE_NAME}'...")
        for attempt in range(max_attempts):
            print(f"Scan attempt {attempt + 1}/{max_attempts}")
            try:
                devices = await BleakScanner.discover(timeout=8.0)
                for device in devices:
                    if device.name == DEVICE_NAME:
                        print(f"Found target device: {device.name} ({device.address})")
                        return device.address
                if attempt < max_attempts - 1:
                    print(f"Device not found, waiting 2 seconds...")
                    await asyncio.sleep(2)
            except Exception as e:
                print(f"Scan error: {e}")
                if attempt < max_attempts - 1:
                    await asyncio.sleep(2)
        print(f"Device '{DEVICE_NAME}' not found after {max_attempts} attempts")
        return None
    async def connect(self, device_address=None, max_retries=3):
        if device_address is None:
            device_address = await self.scan_for_device()
            if device_address is None:
                return False
        for retry in range(max_retries):
            try:
                print(f"Connecting to {device_address}... (attempt {retry + 1}/{max_retries})")
                self.client = BleakClient(device_address, timeout=20.0)
                await self.client.connect()
                if not self.client.is_connected:
                    print(f"Connection failed")
                    if retry < max_retries - 1:
                        await asyncio.sleep(2)
                    continue
                print("Connected successfully!")
                await asyncio.sleep(1.0)
                await self.client.start_notify(B11_NFC_CHAR_UUID, self._notification_handler)
                print("Subscribed to NFC characteristic")
                return True
            except Exception as e:
                print(f"Connection error: {e}")
                if self.client and self.client.is_connected:
                    try:
                        await self.client.disconnect()
                    except:
                        pass
                if retry < max_retries - 1:
                    print(f"Retrying in 2 seconds...")
                    await asyncio.sleep(2)
        print(f"Failed to connect after {max_retries} attempts")
        return False
    def _notification_handler(self, sender, data):
        if len(data) == 0:
            return
        cmd = data[0]
        if cmd == NFC_CMD_CARD_EXCHANGED:
            if len(data) >= 65:
                card_data = data[1:65]
                print(f"\nFriend card received via NFC!")
                print(f"Card data: {card_data[:32].hex()}...")
                if self.card_exchange_callback:
                    self.card_exchange_callback(card_data)
        else:
            self.response = data
            print(f"Response received: {len(data)} bytes")
    def set_card_exchange_callback(self, callback):
        self.card_exchange_callback = callback
    async def set_my_card(self, card_data):
        if len(card_data) != 64:
            raise ValueError("Card data must be 64 bytes")
        print("Setting my card...")
        self.response = None
        try:
            cmd = bytes([NFC_CMD_SET_MY_CARD]) + card_data
            await self.client.write_gatt_char(B11_NFC_CHAR_UUID, cmd, response=False)
            for _ in range(10):
                await asyncio.sleep(0.1)
                if self.response:
                    break
            if self.response and self.response[0] == NFC_STATUS_OK:
                print("My card set successfully")
                return True
            else:
                print("Failed to set card (no response or error)")
                return False
        except Exception as e:
            print(f"Error setting card: {e}")
            return False
    async def get_my_card(self):
        print("Getting my card...")
        self.response = None
        try:
            await self.client.write_gatt_char(B11_NFC_CHAR_UUID, bytes([NFC_CMD_GET_MY_CARD]), response=False)
            for _ in range(10):
                await asyncio.sleep(0.1)
                if self.response:
                    break
            if self.response and self.response[0] == NFC_STATUS_OK and len(self.response) >= 65:
                card_data = self.response[1:65]
                print(f"My card received: {len(card_data)} bytes")
                return card_data
            else:
                print("Failed to get my card (timeout or error)")
                return None
        except Exception as e:
            print(f"Error getting card: {e}")
            return None
    async def get_friend_card(self):
        print("Getting friend card...")
        self.response = None
        try:
            await self.client.write_gatt_char(B11_NFC_CHAR_UUID, bytes([NFC_CMD_GET_FRIEND_CARD]), response=False)
            for _ in range(10):
                await asyncio.sleep(0.1)
                if self.response:
                    break
            if self.response:
                status = self.response[0]
                if status == NFC_STATUS_OK and len(self.response) >= 65:
                    card_data = self.response[1:65]
                    print(f"Friend card received: {len(card_data)} bytes")
                    return card_data
                elif status == NFC_STATUS_NO_FRIEND:
                    print("No friend card available yet")
                    return None
            print("Failed to get friend card (timeout)")
            return None
        except Exception as e:
            print(f"Error getting friend card: {e}")
            return None
    async def get_friend_count(self):
        print("Getting friend count...")
        self.response = None
        try:
            await self.client.write_gatt_char(B11_NFC_CHAR_UUID, bytes([NFC_CMD_GET_FRIEND_COUNT]), response=False)
            for _ in range(10):
                await asyncio.sleep(0.1)
                if self.response:
                    break
            if self.response and self.response[0] == NFC_STATUS_OK and len(self.response) >= 2:
                count = self.response[1]
                print(f"Friend count: {count}")
                return count
            print("Failed to get friend count (timeout)")
            return 0
        except Exception as e:
            print(f"Error getting friend count: {e}")
            return 0
    async def disconnect(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()
            print("Disconnected")

# ========== 使用示例 ==========
def on_card_exchanged(card_data):
    print("=" * 60)
    print("New friend card received!")
    print(f"Card preview: {card_data[:32].hex()}")
    print("=" * 60)

async def demo():
    manager = NFCBLEManager()
    manager.set_card_exchange_callback(on_card_exchanged)
    try:
        if not await manager.connect():
            return
        my_card = bytearray(64)
        my_card[0] = 0x01
        my_card[1:3] = struct.pack('>H', 86)
        my_card[3] = 0x01
        my_card[4:24] = "张三".encode('utf-8').ljust(20, b'\x00')
        my_card[24:44] = "+8613800138000".encode('utf-8').ljust(20, b'\x00')
        my_card[44:52] = "user001".encode('utf-8').ljust(8, b'\x00')
        await manager.set_my_card(bytes(my_card))
        card = await manager.get_my_card()
        if card:
            print(f"\nMy card verification:")
            print(f"Name: {card[4:24].decode('utf-8').rstrip(chr(0))}")
            print(f"Phone: {card[24:44].decode('utf-8').rstrip(chr(0))}")
            print(f"User ID: {card[44:52].decode('utf-8').rstrip(chr(0))}")
        count = await manager.get_friend_count()
        if count > 0:
            friend_card = await manager.get_friend_card()
            if friend_card:
                print(f"\nFriend card data:")
                print(f"Raw: {friend_card[:32].hex()}...")
        print("\nMonitoring NFC card exchanges...")
        print("Touch your ESP32 with another device to exchange cards")
        print("Press Ctrl+C to exit\n")
        try:
            await asyncio.sleep(300)
        except KeyboardInterrupt:
            print("\nInterrupted by user")
    finally:
        await manager.disconnect()

async def quick_test():
    manager = NFCBLEManager()
    try:
        if not await manager.connect():
            return
        count = await manager.get_friend_count()
        if count > 0:
            card = await manager.get_friend_card()
            if card:
                print(f"\nFriend card exists:")
                print(f"Data: {card.hex()}")
        else:
            print("\nNo friend cards yet")
    finally:
        await manager.disconnect()

if __name__ == "__main__":
    print("=" * 60)
    print("NFC BLE Manager")
    print("=" * 60)
    print("\nOptions:")
    print("  1. Full demo (set card + monitor exchanges)")
    print("  2. Quick test (read current status)")
    print()
    
    choice = input("Select option (1/2) [1]: ").strip() or "1"
    
    if choice == "2":
        asyncio.run(quick_test())
    else:
        asyncio.run(demo())
