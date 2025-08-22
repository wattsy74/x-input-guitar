import subprocess
import os
import shutil

ARDUINO_CLI = r'C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
FQBN = 'rp2040:rp2040:rpipico:usbstack=tinyusb'
FIRMWARE_FILES = {
    'X-Input': 'bgg-xinput-firmware.ino',
    'HID': 'bgg-xinput-clean.ino',  # Using clean version for HID
}

def compile_firmware(controller_type):
    firmware = FIRMWARE_FILES[controller_type]
    
    # Ensure we're working with the correct main sketch file
    main_sketch = 'bgg-xinput-firmware.ino'  # Default main sketch
    target_sketch = firmware
    
    # Temporarily rename the target sketch to be the main one if needed
    temp_renamed = False
    original_main = None
    
    if target_sketch != main_sketch:
        if os.path.exists(main_sketch):
            original_main = f"{main_sketch}.backup"
            shutil.move(main_sketch, original_main)
        shutil.copy2(target_sketch, main_sketch)
        temp_renamed = True
    
    try:
        cmd = [
            ARDUINO_CLI, 'compile', '--fqbn', FQBN, '.'
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(result.stderr)
        return f'Compiled {firmware} successfully!'
    finally:
        # Restore original files if we renamed anything
        if temp_renamed:
            if os.path.exists(main_sketch):
                os.remove(main_sketch)
            if original_main and os.path.exists(original_main):
                shutil.move(original_main, main_sketch)

def upload_firmware(controller_type, port):
    firmware = FIRMWARE_FILES[controller_type]
    if not port:
        raise ValueError('Serial port required (e.g., COM3)')
    
    # Ensure we're working with the correct main sketch file
    main_sketch = 'bgg-xinput-firmware.ino'  # Default main sketch
    target_sketch = firmware
    
    # Temporarily rename the target sketch to be the main one if needed
    temp_renamed = False
    original_main = None
    
    if target_sketch != main_sketch:
        if os.path.exists(main_sketch):
            original_main = f"{main_sketch}.backup"
            shutil.move(main_sketch, original_main)
        shutil.copy2(target_sketch, main_sketch)
        temp_renamed = True
    
    try:
        cmd = [
            ARDUINO_CLI, 'upload', '--fqbn', FQBN, '--port', port, '.'
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(result.stderr)
        return f'Uploaded {firmware} to {port}!'
    finally:
        # Restore original files if we renamed anything
        if temp_renamed:
            if os.path.exists(main_sketch):
                os.remove(main_sketch)
            if original_main and os.path.exists(original_main):
                shutil.move(original_main, main_sketch)
