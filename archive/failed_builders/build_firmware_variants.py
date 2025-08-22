# Firmware Build Script for App Packaging
# This script pre-compiles all firmware variants for bundling

import subprocess
import os
import shutil
from pathlib import Path

# Build configurations
BUILD_CONFIGS = {
    'X-Input': {
        'source': 'bgg-xinput-firmware.ino',
        'output': 'bgg_xinput_firmware.uf2'
    },
    'HID': {
        'source': 'bgg-xinput-clean.ino', 
        'output': 'bgg_hid_firmware.uf2'
    }
}

ARDUINO_CLI = r'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
FQBN = 'rp2040:rp2040:rpipico:usbstack=tinyusb'

def build_firmware_variant(config_name, config):
    """Build a specific firmware variant"""
    print(f"Building {config_name} firmware...")
    
    # Backup current main file
    main_file = 'bgg-xinput-firmware.ino'
    source_file = config['source']
    
    if source_file != main_file:
        if os.path.exists(main_file):
            shutil.move(main_file, f"{main_file}.backup")
        shutil.copy2(source_file, main_file)
    
    try:
        # Compile
        cmd = [ARDUINO_CLI, 'compile', '--fqbn', FQBN, '--output-dir', 'build_temp', '.']
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            raise RuntimeError(f"Compilation failed: {result.stderr}")
        
        # Copy UF2 file to firmware binaries
        uf2_source = os.path.join('build_temp', 'bgg-xinput-firmware.ino.uf2')
        uf2_dest = os.path.join('firmware_binaries', config['output'])
        
        if os.path.exists(uf2_source):
            shutil.copy2(uf2_source, uf2_dest)
            print(f"✓ Built {config['output']}")
        else:
            raise FileNotFoundError(f"UF2 file not found: {uf2_source}")
            
    finally:
        # Restore original file
        if source_file != main_file:
            if os.path.exists(main_file):
                os.remove(main_file)
            if os.path.exists(f"{main_file}.backup"):
                shutil.move(f"{main_file}.backup", main_file)

def main():
    """Build all firmware variants for app packaging"""
    print("Building firmware variants for app packaging...")
    
    # Create output directory
    os.makedirs('firmware_binaries', exist_ok=True)
    os.makedirs('build_temp', exist_ok=True)
    
    try:
        for config_name, config in BUILD_CONFIGS.items():
            build_firmware_variant(config_name, config)
        
        print("\n✓ All firmware variants built successfully!")
        print("Files in firmware_binaries/ can be bundled with your app")
        
    except Exception as e:
        print(f"Build failed: {e}")
        return 1
    finally:
        # Cleanup
        if os.path.exists('build_temp'):
            shutil.rmtree('build_temp')
    
    return 0

if __name__ == '__main__':
    exit(main())
