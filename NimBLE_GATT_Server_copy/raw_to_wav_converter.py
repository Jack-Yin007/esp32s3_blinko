#!/usr/bin/env python3
"""
Convert raw I2S data from ESP32-S3 to WAV file format.
This script reads binary I2S data and converts it to a standard WAV audio file.
"""

import struct
import wave
import numpy as np
import argparse
import os
from datetime import datetime

def convert_raw_to_wav(input_file, output_file=None, sample_rate=16000, channels=1, sample_width=2):
    """
    Convert raw I2S data to WAV format.
    
    Args:
        input_file (str): Path to raw data file (TEST.txt)
        output_file (str): Output WAV file path (optional)
        sample_rate (int): Audio sample rate in Hz (default: 16000)
        channels (int): Number of audio channels (default: 1 for mono)
        sample_width (int): Sample width in bytes (default: 2 for 16-bit)
    """
    
    # Generate output filename if not provided
    if output_file is None:
        base_name = os.path.splitext(input_file)[0]
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_file = f"{base_name}_converted_{timestamp}.wav"
    
    try:
        # Read raw binary data
        print(f"Reading raw data from: {input_file}")
        with open(input_file, 'rb') as f:
            raw_data = f.read()
        
        print(f"Raw data size: {len(raw_data)} bytes")
        
        # Check if data size is valid (should be multiple of sample_width)
        if len(raw_data) % sample_width != 0:
            print(f"Warning: Data size ({len(raw_data)}) is not a multiple of sample width ({sample_width})")
            # Trim to nearest multiple
            raw_data = raw_data[:-(len(raw_data) % sample_width)]
            print(f"Trimmed data size: {len(raw_data)} bytes")
        
        # Calculate number of samples
        num_samples = len(raw_data) // sample_width
        duration = num_samples / sample_rate
        print(f"Number of samples: {num_samples}")
        print(f"Audio duration: {duration:.2f} seconds")
        
        # Convert raw bytes to numpy array
        if sample_width == 2:  # 16-bit signed
            # ESP32 I2S data is typically little-endian 16-bit signed
            audio_data = np.frombuffer(raw_data, dtype=np.int16)
        elif sample_width == 4:  # 32-bit signed
            audio_data = np.frombuffer(raw_data, dtype=np.int32)
        else:
            raise ValueError(f"Unsupported sample width: {sample_width}")
        
        # Print some statistics about the audio data
        print(f"Audio data statistics:")
        print(f"  Min value: {np.min(audio_data)}")
        print(f"  Max value: {np.max(audio_data)}")
        print(f"  Mean value: {np.mean(audio_data):.2f}")
        print(f"  RMS value: {np.sqrt(np.mean(audio_data**2)):.2f}")
        
        # Check for silence (all zeros)
        if np.all(audio_data == 0):
            print("Warning: All audio data is zero (silence)")
        elif np.std(audio_data) < 10:
            print("Warning: Very low audio signal variation detected")
        
        # Write WAV file
        print(f"Writing WAV file to: {output_file}")
        with wave.open(output_file, 'wb') as wav_file:
            wav_file.setnchannels(channels)
            wav_file.setsampwidth(sample_width)
            wav_file.setframerate(sample_rate)
            
            # Convert numpy array back to bytes for writing
            wav_file.writeframes(audio_data.tobytes())
        
        print(f"✅ Conversion completed successfully!")
        print(f"📁 Output file: {output_file}")
        print(f"🎵 Format: {sample_rate}Hz, {channels} channel(s), {sample_width*8}-bit")
        
        return output_file
        
    except FileNotFoundError:
        print(f"❌ Error: Input file '{input_file}' not found")
        return None
    except Exception as e:
        print(f"❌ Error during conversion: {e}")
        return None

def analyze_raw_data(input_file, sample_width=2, num_samples_to_show=20):
    """
    Analyze raw data to help understand the format.
    """
    try:
        with open(input_file, 'rb') as f:
            raw_data = f.read(sample_width * num_samples_to_show)
        
        print(f"\n📊 Raw data analysis (first {num_samples_to_show} samples):")
        
        if sample_width == 2:
            samples = struct.unpack(f'<{len(raw_data)//2}h', raw_data)
        elif sample_width == 4:
            samples = struct.unpack(f'<{len(raw_data)//4}i', raw_data)
        else:
            print(f"Unsupported sample width: {sample_width}")
            return
        
        print(f"Sample values: {samples}")
        print(f"Hex representation: {raw_data.hex()}")
        
    except Exception as e:
        print(f"Error analyzing data: {e}")

def main():
    parser = argparse.ArgumentParser(description='Convert raw I2S data to WAV format')
    parser.add_argument('input_file', help='Input raw data file (e.g., TEST.txt)')
    parser.add_argument('-o', '--output', help='Output WAV file path')
    parser.add_argument('-r', '--rate', type=int, default=16000, help='Sample rate (default: 16000)')
    parser.add_argument('-c', '--channels', type=int, default=1, help='Number of channels (default: 1)')
    parser.add_argument('-w', '--width', type=int, default=2, help='Sample width in bytes (default: 2)')
    parser.add_argument('-a', '--analyze', action='store_true', help='Analyze raw data first')
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not os.path.exists(args.input_file):
        print(f"❌ Error: Input file '{args.input_file}' does not exist")
        return
    
    # Analyze raw data if requested
    if args.analyze:
        analyze_raw_data(args.input_file, args.width)
    
    # Convert the file
    output_file = convert_raw_to_wav(
        input_file=args.input_file,
        output_file=args.output,
        sample_rate=args.rate,
        channels=args.channels,
        sample_width=args.width
    )
    
    if output_file:
        print(f"\n🎉 You can now play the audio file: {output_file}")

if __name__ == "__main__":
    # If run without arguments, try to convert TEST.txt with default settings
    import sys
    if len(sys.argv) == 1:
        print("🎵 Converting TEST.txt with default I2S settings...")
        print("=" * 50)
        
        # Try both TEST.txt files
        test_files = ["TEST.txt", "TEST(1).txt"]
        
        for test_file in test_files:
            if os.path.exists(test_file):
                print(f"\n📁 Processing: {test_file}")
                analyze_raw_data(test_file)
                convert_raw_to_wav(test_file)
            else:
                print(f"❌ File not found: {test_file}")
    else:
        main()