#!/usr/bin/env python3
"""
Firmware flasher that uses pre-compiled binaries and injects config data
"""
import os
import json
import struct
import subprocess
from pathlib import Path

class FirmwareFlasher:
    def __init__(self):
        self.firmware_dir = Path("firmware_binaries")
        self.config_dir = Path("config")
        
    def get_available_firmwares(self):
        """Get list of available pre-compiled firmware files"""
        if not self.firmware_dir.exists():
            return []
        return [f.stem for f in self.firmware_dir.glob("*.uf2")]
    
    def load_config(self, config_name="default"):
        """Load configuration from JSON file"""
        config_file = self.config_dir / f"{config_name}.json"
        if config_file.exists():
            with open(config_file, 'r') as f:
                return json.load(f)
        return {}
    
    def create_config_binary(self, config_data):
        """Convert config JSON to binary format for embedding"""
        config_json = json.dumps(config_data, separators=(',', ':'))
        config_bytes = config_json.encode('utf-8')
        
        # Create a config block with header
        header = struct.pack('<4sI', b'BGGC', len(config_bytes))  # Magic + Length
        return header + config_bytes + b'\x00' * (4096 - len(header) - len(config_bytes))  # Pad to 4KB
    
    def inject_config_into_firmware(self, firmware_path, config_data):
        """Inject config data into firmware binary at specific offset"""
        with open(firmware_path, 'rb') as f:
            firmware_data = bytearray(f.read())
        
        config_binary = self.create_config_binary(config_data)
        
        # Find config placeholder in firmware (look for magic bytes)
        config_placeholder = b'BGGCONFIG_PLACEHOLDER_4096_BYTES'
        placeholder_pos = firmware_data.find(config_placeholder)
        
        if placeholder_pos == -1:
            raise ValueError("Config placeholder not found in firmware")
        
        # Replace placeholder with actual config
        firmware_data[placeholder_pos:placeholder_pos + 4096] = config_binary
        
        # Create temporary firmware file with injected config
        temp_firmware = firmware_path.with_suffix('.temp.uf2')
        with open(temp_firmware, 'wb') as f:
            f.write(firmware_data)
        
        return temp_firmware
    
    def flash_firmware(self, firmware_type, config_data=None, device_path=None):
        """Flash firmware with embedded config to device"""
        firmware_file = self.firmware_dir / f"{firmware_type}.uf2"
        
        if not firmware_file.exists():
            raise FileNotFoundError(f"Firmware not found: {firmware_file}")
        
        # If config provided, inject it into firmware
        if config_data:
            firmware_file = self.inject_config_into_firmware(firmware_file, config_data)
        
        # Auto-detect RPI-RP2 drive if not specified
        if not device_path:
            device_path = self.find_rpi_device()
        
        if not device_path:
            raise RuntimeError("No Pico device found in BOOTSEL mode")
        
        # Copy firmware to device
        import shutil
        dest_path = Path(device_path) / "firmware.uf2"
        shutil.copy2(firmware_file, dest_path)
        
        # Clean up temp file if created
        if config_data and firmware_file.suffix == '.temp.uf2':
            firmware_file.unlink()
        
        return True
    
    def find_rpi_device(self):
        """Find Raspberry Pi Pico in BOOTSEL mode"""
        import platform
        
        if platform.system() == "Windows":
            # Look for RPI-RP2 drive
            import string
            from pathlib import Path
            
            for drive in string.ascii_uppercase:
                drive_path = Path(f"{drive}:\\")
                if drive_path.exists():
                    try:
                        # Check if it's a Pico by looking for INFO_UF2.TXT
                        info_file = drive_path / "INFO_UF2.TXT"
                        if info_file.exists():
                            with open(info_file, 'r') as f:
                                content = f.read()
                                if "RPI-RP2" in content:
                                    return str(drive_path)
                    except:
                        continue
        
        return None

# Example usage
def main():
    flasher = FirmwareFlasher()
    
    # Load default config
    config = flasher.load_config("default")
    
    # Add app-specific config
    config.update({
        "device_name": "BGG Guitar Controller",
        "led_brightness": 128,
        "button_mapping": {
            "green": 2,
            "red": 3,
            "yellow": 4,
            "blue": 5,
            "orange": 6
        }
    })
    
    # Flash X-Input firmware with embedded config
    try:
        flasher.flash_firmware("xinput", config)
        print("Firmware flashed successfully!")
    except Exception as e:
        print(f"Flash failed: {e}")

if __name__ == "__main__":
    main()
