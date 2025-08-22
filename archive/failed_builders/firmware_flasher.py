#!/usr/bin/env python3
"""
Pre-compiled firmware flasher - much more reliable for app distribution
Uses pre-built .uf2 files instead of compiling on-the-fly
"""
import os
import shutil
import time
import psutil
import argparse
from pathlib import Path

# Pre-compiled firmware files (these would be built during app packaging)
FIRMWARE_BINARIES = {
    'X-Input': 'bgg_xinput_firmware.uf2',
    'HID': 'bgg_hid_firmware.uf2',
}

class FirmwareFlasher:
    def __init__(self):
        self.firmware_dir = os.path.join(os.path.dirname(__file__), 'firmware_binaries')
    
    def detect_bootsel_drive(self):
        """Detect RPI-RP2 BOOTSEL drive"""
        drives = []
        for partition in psutil.disk_partitions():
            try:
                if 'RPI-RP2' in partition.mountpoint or 'RPI-RP2' in partition.device:
                    drives.append(partition.mountpoint)
            except:
                continue
        return drives
    
    def wait_for_bootsel(self, timeout=30):
        """Wait for device to enter BOOTSEL mode"""
        print("Waiting for device to enter BOOTSEL mode...")
        print("Hold BOOTSEL button and press RESET, or plug in while holding BOOTSEL")
        
        start_time = time.time()
        while time.time() - start_time < timeout:
            drives = self.detect_bootsel_drive()
            if drives:
                return drives[0]
            time.sleep(1)
            print(".", end="", flush=True)
        
        print("\nTimeout waiting for BOOTSEL mode")
        return None
    
    def flash_firmware(self, controller_type):
        """Flash pre-compiled firmware to device"""
        firmware_file = FIRMWARE_BINARIES.get(controller_type)
        if not firmware_file:
            raise ValueError(f"Unknown controller type: {controller_type}")
        
        firmware_path = os.path.join(self.firmware_dir, firmware_file)
        if not os.path.exists(firmware_path):
            raise FileNotFoundError(f"Firmware binary not found: {firmware_path}")
        
        # Wait for BOOTSEL mode
        bootsel_drive = self.wait_for_bootsel()
        if not bootsel_drive:
            raise RuntimeError("Device not found in BOOTSEL mode")
        
        print(f"\nFlashing {controller_type} firmware...")
        print(f"Source: {firmware_path}")
        print(f"Target: {bootsel_drive}")
        
        # Copy firmware to BOOTSEL drive
        target_path = os.path.join(bootsel_drive, firmware_file)
        shutil.copy2(firmware_path, target_path)
        
        print("✓ Firmware flashed successfully!")
        print("Device will reboot automatically...")
        return True

def main():
    parser = argparse.ArgumentParser(description='BGG Pre-compiled Firmware Flasher')
    parser.add_argument('--type', choices=['X-Input', 'HID'], default='X-Input', help='Controller type')
    
    args = parser.parse_args()
    
    try:
        flasher = FirmwareFlasher()
        flasher.flash_firmware(args.type)
        return 0
    except Exception as e:
        print(f"Error: {e}")
        return 1

if __name__ == '__main__':
    exit(main())
