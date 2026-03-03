"""
BLE audio recorder with ADPCM decompression support
Receives compressed audio from ESP32 and decodes it
"""
import asyncio
import struct
import wave
import numpy as np
from datetime import datetime
import os
from bleak import BleakClient, BleakScanner, BleakGATTCharacteristic

# Configuration - Actual ESP32 Audio Service
DEVICE_NAME = "Blinko"

# ESP32 has TWO audio services:
# 1. B11 Protocol (Nordic UART Service) - ACTIVE for audio streaming
B11_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
B11_AUDIO_CHAR_UUID = "6e400004-b5a3-f393-e0a9-e50e24dcca9e"    # This is the REAL audio stream

# 2. Standard Digital Output Service - Not used for audio
# AUDIO_SERVICE_UUID = "00001910-0000-1000-8000-00805f9b34fb"
# AUDIO_CHAR_UUID = "00002a56-0000-1000-8000-00805f9b34fb"

# Note: ESP32 uses B11 Audio characteristic (6e400004) for actual audio streaming
# The audio starts automatically when notifications are enabled

# Audio settings - 8kHz
SAMPLE_RATE = 8000   # 8kHz
CHANNELS = 1
SAMPLE_WIDTH = 2
RECORD_DURATION = 10

# ADPCM support
USE_ADPCM = True  # ESP32 sends ADPCM compressed data


class ADPCMDecoder:
    """IMA ADPCM decoder matching ESP32 encoder"""
    
    def __init__(self):
        self.predicted_sample = 0
        self.step_index = 0
        
        # IMA ADPCM step table
        self.step_table = [
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
            19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
            50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
            130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
            337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
            876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
            2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
            5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
            15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
        ]
        
        # Index adjustment table
        self.index_table = [
            -1, -1, -1, -1, 2, 4, 6, 8,
            -1, -1, -1, -1, 2, 4, 6, 8
        ]
    
    def decode_sample(self, adpcm_nibble):
        """Decode one 4-bit ADPCM sample to 16-bit PCM"""
        step = self.step_table[self.step_index]
        diffq = step >> 3
        
        if adpcm_nibble & 4:
            diffq += step
        if adpcm_nibble & 2:
            diffq += step >> 1
        if adpcm_nibble & 1:
            diffq += step >> 2
        
        if adpcm_nibble & 8:
            self.predicted_sample -= diffq
        else:
            self.predicted_sample += diffq
        
        # Clamp
        if self.predicted_sample > 32767:
            self.predicted_sample = 32767
        elif self.predicted_sample < -32768:
            self.predicted_sample = -32768
        
        # Update step index
        self.step_index += self.index_table[adpcm_nibble & 0x07]
        if self.step_index < 0:
            self.step_index = 0
        elif self.step_index > 88:
            self.step_index = 88
        
        return self.predicted_sample
    
    def decode(self, adpcm_data):
        """Decode ADPCM byte array to PCM samples"""
        pcm_samples = []
        
        for byte in adpcm_data:
            # Decode high 4 bits
            high_nibble = (byte >> 4) & 0x0F
            pcm_samples.append(self.decode_sample(high_nibble))
            
            # Decode low 4 bits
            low_nibble = byte & 0x0F
            pcm_samples.append(self.decode_sample(low_nibble))
        
        return pcm_samples


class OptimizedBLEAudioRecorder:
    def __init__(self):
        self.client = None
        self.is_recording = False
        self.audio_data = []
        self.total_bytes = 0
        self.start_time = None
        self.adpcm_decoder = ADPCMDecoder() if USE_ADPCM else None
        self.adpcm_buffer = bytearray()  # Buffer for incomplete ADPCM packets
        self.total_compressed_bytes = 0
        self.total_decompressed_samples = 0

    async def scan_for_device(self, max_attempts=5):
        """Scan for the ESP32-S3 BLE device"""
        print(f"🔍 Scanning for '{DEVICE_NAME}'...")
        
        for attempt in range(max_attempts):
            print(f"   Scan attempt {attempt + 1}/{max_attempts}")
            
            devices = await BleakScanner.discover(timeout=8.0)
            
            for device in devices:
                if device.name == DEVICE_NAME:
                    print(f"✅ Found target device: {device.name} ({device.address})")
                    return device.address
            
            if attempt < max_attempts - 1:
                print(f"   Device not found, waiting 2 seconds...")
                await asyncio.sleep(2)
        
        print(f"❌ Device '{DEVICE_NAME}' not found")
        return None

    async def notification_handler(self, characteristic: BleakGATTCharacteristic, data: bytearray):
        """Handle incoming audio data notifications (ADPCM or PCM)"""
        if not self.is_recording:
            return
        
        # Debug: print first notification
        if self.total_bytes == 0:
            print(f"📥 First notification received! Length: {len(data)} bytes")
            print(f"   Raw data (first 16 bytes): {data[:16].hex()}")
            if USE_ADPCM:
                print(f"   🗜️  ADPCM mode: will decompress to PCM")
        
        if USE_ADPCM:
            # ADPCM compressed data
            self.adpcm_buffer.extend(data)
            self.total_compressed_bytes += len(data)
            
            # Process complete ADPCM frames (need header: 4 bytes + ADPCM data)
            while len(self.adpcm_buffer) >= 4:
                # Read header: sample_count (2 bytes) + original_bytes (2 bytes)
                sample_count = (self.adpcm_buffer[0] << 8) | self.adpcm_buffer[1]
                original_bytes = (self.adpcm_buffer[2] << 8) | self.adpcm_buffer[3]
                
                # Calculate expected ADPCM bytes (4:1 compression)
                adpcm_bytes = (sample_count + 1) // 2  # 2 samples per byte
                total_frame_bytes = 4 + adpcm_bytes
                
                # Check if we have complete frame
                if len(self.adpcm_buffer) < total_frame_bytes:
                    break  # Wait for more data
                
                # Extract ADPCM data
                adpcm_data = self.adpcm_buffer[4:total_frame_bytes]
                
                # Decode ADPCM to PCM
                pcm_samples = self.adpcm_decoder.decode(adpcm_data)
                
                # Truncate to expected sample count
                pcm_samples = pcm_samples[:sample_count]
                
                self.audio_data.extend(pcm_samples)
                self.total_decompressed_samples += len(pcm_samples)
                
                # Remove processed frame from buffer
                self.adpcm_buffer = self.adpcm_buffer[total_frame_bytes:]
            
            self.total_bytes = self.total_decompressed_samples * 2
            
        else:
            # Raw PCM data (original mode)
            if len(data) % 2 != 0:
                data = data[:-1]
            
            try:
                samples = struct.unpack('<' + 'h' * (len(data) // 2), data)
                self.audio_data.extend(samples)
                self.total_bytes += len(data)
            except struct.error as e:
                print(f"Error unpacking audio data: {e}")
                return
        
        # Print progress every 1000 samples
        if len(self.audio_data) % 1000 == 0 and len(self.audio_data) > 0:
            elapsed = datetime.now().timestamp() - self.start_time
            max_sample = max(abs(s) for s in list(self.audio_data)[-1000:]) if self.audio_data else 0
            expected_samples = int(elapsed * SAMPLE_RATE)
            efficiency = (len(self.audio_data) / expected_samples * 100) if expected_samples > 0 else 0
            print(f"Recording... {elapsed:.1f}s | {len(self.audio_data)} samples | Max: {max_sample} | Efficiency: {efficiency:.1f}%")

    async def connect_and_record(self, device_address, max_retries=3):
        """Connect to device and start recording"""
        
        for attempt in range(max_retries):
            try:
                print(f"\n🔗 Connection attempt {attempt + 1}/{max_retries}")
                
                async with BleakClient(device_address, timeout=20.0) as client:
                    if not client.is_connected:
                        print("❌ Failed to connect")
                        continue
                    
                    print("✅ Connected successfully!")
                    await asyncio.sleep(2.0)
                    
                    # List all services for debugging
                    services = client.services
                    service_count = len(list(services))
                    print(f"\n🔍 Found {service_count} services:")
                    for service in services:
                        print(f"   Service: {service.uuid}")
                        for char in service.characteristics:
                            print(f"      Char: {char.uuid} | Props: {char.properties}")
                    
                    # Find B11 service
                    b11_service = None
                    for service in services:
                        if B11_SERVICE_UUID.lower() in str(service.uuid).lower():
                            b11_service = service
                            print(f"\n✅ Found B11 service: {service.uuid}")
                            break
                    
                    if not b11_service:
                        print(f"❌ B11 service not found! Expected: {B11_SERVICE_UUID}")
                        continue
                    
                    # Find B11 audio characteristic
                    audio_char = None
                    
                    for char in b11_service.characteristics:
                        char_uuid_str = str(char.uuid).lower()
                        if B11_AUDIO_CHAR_UUID.lower() in char_uuid_str:
                            audio_char = char
                            print(f"✅ Found B11 audio characteristic: {char.uuid}")
                            print(f"   Properties: {char.properties}")
                    
                    if not audio_char:
                        print(f"❌ B11 audio characteristic not found! Expected: {B11_AUDIO_CHAR_UUID}")
                        continue
                    
                    # Check capabilities
                    can_notify = "notify" in audio_char.properties
                    
                    if not can_notify:
                        print("❌ B11 audio characteristic doesn't support notifications!")
                        continue
                    
                    # Start notifications on audio characteristic
                    print("\n📡 Starting audio notifications...")
                    await client.start_notify(audio_char, self.notification_handler)
                    
                    # Wait for ESP32 to start streaming (important!)
                    print("⏳ Waiting for ESP32 audio stream to stabilize...")
                    await asyncio.sleep(3.0)  # 增加到3秒确保ESP32准备好
                    
                    # Initialize recording
                    self.audio_data = []
                    self.total_bytes = 0
                    self.is_recording = True
                    self.start_time = datetime.now().timestamp()
                    
                    # Note: ESP32 auto-starts recording when notifications are enabled
                    print("✅ Audio notifications enabled - ESP32 should be streaming now")
                    
                    print(f"\n🎙️ Recording 2kHz audio for {RECORD_DURATION} seconds...")
                    print("🔊 Make some noise near the microphone!")
                    
                    # Record for specified duration
                    await asyncio.sleep(RECORD_DURATION)
                    
                    # Stop recording
                    self.is_recording = False
                    print("\n⏰ Recording time completed")
                    
                    # Note: ESP32 auto-stops when notifications are disabled
                    print("📝 Stopping notifications - ESP32 will auto-stop recording")
                    
                    # Wait for remaining data before stopping notifications
                    await asyncio.sleep(1.0)
                    
                    # Try to stop notifications gracefully
                    try:
                        await client.stop_notify(audio_char)
                        print("✅ Notifications stopped successfully")
                    except Exception as stop_error:
                        print(f"⚠️  Warning: Failed to stop notifications: {stop_error}")
                        print("   (This is usually harmless - connection may have already closed)")
                    
                    # Calculate final efficiency
                    actual_duration = len(self.audio_data) / SAMPLE_RATE if self.audio_data else 0
                    expected_samples = RECORD_DURATION * SAMPLE_RATE
                    efficiency = (len(self.audio_data) / expected_samples * 100) if expected_samples > 0 else 0
                    
                    print(f"\n✅ Recording completed!")
                    print(f"📊 Total samples: {len(self.audio_data):,}")
                    print(f"📊 Expected samples (2kHz): {expected_samples:,}")
                    print(f"📊 Actual duration: {actual_duration:.2f} seconds")
                    print(f"📊 Efficiency: {efficiency:.1f}%")
                    
                    if USE_ADPCM:
                        # ADPCM compression statistics
                        uncompressed_bytes = self.total_decompressed_samples * 2
                        compression_ratio = uncompressed_bytes / self.total_compressed_bytes if self.total_compressed_bytes > 0 else 0
                        print(f"\n🗜️  ADPCM Compression Stats:")
                        print(f"   Received (compressed): {self.total_compressed_bytes:,} bytes")
                        print(f"   Decompressed to: {uncompressed_bytes:,} bytes")
                        print(f"   Compression ratio: {compression_ratio:.2f}:1")
                        print(f"   Bandwidth saved: {(1 - 1/compression_ratio)*100:.1f}%")
                    
                    return True
                    
            except Exception as e:
                print(f"❌ Connection attempt {attempt + 1} failed: {e}")
                if attempt < max_retries - 1:
                    await asyncio.sleep(3)
        
        return False

    def save_to_wav(self, filename=None):
        """Save recorded audio data to WAV file with 8kHz sample rate"""
        if not self.audio_data:
            print("❌ No audio data to save!")
            return False
        
        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"esp32_audio_8khz_{timestamp}.wav"
        
        try:
            audio_array = np.array(self.audio_data, dtype=np.int16)
            
            # Audio quality analysis
            max_val = np.max(np.abs(audio_array))
            mean_val = np.mean(np.abs(audio_array))
            
            print(f"\n📊 Audio Analysis:")
            print(f"   Max amplitude: {max_val}")
            print(f"   Mean amplitude: {mean_val:.1f}")
            
            if max_val == 0:
                print("⚠️  All samples are zero!")
            elif max_val > 30000:
                print("⚠️  Possible clipping!")
            else:
                print("✅ Good audio range")
            
            # Save WAV file with 8kHz sample rate
            with wave.open(filename, 'wb') as wav_file:
                wav_file.setnchannels(CHANNELS)
                wav_file.setsampwidth(SAMPLE_WIDTH)
                wav_file.setframerate(SAMPLE_RATE)  # 8kHz
                wav_file.writeframes(audio_array.tobytes())
            
            file_size = os.path.getsize(filename)
            duration = len(self.audio_data) / SAMPLE_RATE
            
            print(f"\n💾 Audio saved: {filename}")
            print(f"📏 File size: {file_size} bytes")
            print(f"⏱️  Duration: {duration:.2f}s")
            print(f"📡 Sample rate: {SAMPLE_RATE} Hz (8kHz)")
            
            return True
            
        except Exception as e:
            print(f"❌ Error saving WAV file: {e}")
            return False

async def main():
    """Main function"""
    print("🎵 ESP32-S3 8kHz BLE Audio Recorder")
    print("=" * 55)
    print(f"Target device: '{DEVICE_NAME}'")
    print(f"B11 Service UUID: {B11_SERVICE_UUID}")
    print(f"B11 Audio Characteristic: {B11_AUDIO_CHAR_UUID}")
    print(f"Sample rate: {SAMPLE_RATE} Hz (8kHz for BLE)")
    print("=" * 55)
    
    recorder = OptimizedBLEAudioRecorder()
    
    # Scan for device
    device_address = await recorder.scan_for_device(max_attempts=5)
    if not device_address:
        print("❌ Could not find ESP32 device")
        return
    
    # Connect and record
    success = await recorder.connect_and_record(device_address, max_retries=3)
    if not success:
        print("❌ Recording failed!")
        return
    
    # Save recording
    if recorder.save_to_wav():
        print("\n🎉 ESP32-S3 8kHz recording completed successfully!")
        print("Audio data from INMP441 microphone saved!")
    else:
        print("⚠️  Recording completed but save failed")

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n⚠️  Recording interrupted by user")
    except Exception as e:
        print(f"\n💥 Unexpected error: {e}")