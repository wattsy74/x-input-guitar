# BGG Firmware Flasher Setup

## How to Prepare Pre-compiled Firmware Binaries

### 1. Compile Firmware Variants

For each firmware type, compile and create .uf2 files:

```bash
# X-Input firmware
arduino-cli compile --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb bgg-xinput-firmware.ino
cp build/bgg-xinput-firmware.ino.uf2 firmware_binaries/xinput.uf2

# HID firmware  
arduino-cli compile --fqbn rp2040:rp2040:rpipico:usbstack=tinyusb bgg-xinput-clean.ino
cp build/bgg-xinput-clean.ino.uf2 firmware_binaries/hid.uf2
```

### 2. Add Config Placeholder to Firmware Source

In your main firmware file, add a config placeholder:

```c
// Config placeholder - will be replaced by flasher
__attribute__((section(".config_section"))) 
const char config_placeholder[4096] = "BGGCONFIG_PLACEHOLDER_4096_BYTES" 
                                      "................................................................"
                                      // ... pad to exactly 4096 bytes
                                      ;
```

### 3. Directory Structure

```
firmware_binaries/
├── xinput.uf2          # Pre-compiled X-Input firmware
├── hid.uf2             # Pre-compiled HID firmware
└── bootloader.uf2      # Optional bootloader

config/
├── default.json        # Default configuration
├── hid.json           # HID-optimized config
└── custom_presets/    # User-defined presets
    ├── guitar_hero.json
    ├── rock_band.json
    └── custom.json
```

## Benefits of This Approach

1. **No Arduino CLI dependency** - Just copy .uf2 files
2. **Reliable** - Pre-tested firmware binaries
3. **Fast** - No compilation time
4. **Configurable** - Inject custom settings at flash time
5. **Small bundle** - Only ~200KB per firmware variant
6. **Cross-platform** - Works on any OS that can copy files

## Integration with Electron App

The Python flasher can be called from Node.js:

```javascript
const { spawn } = require('child_process');

function flashFirmware(firmwareType, configData) {
    return new Promise((resolve, reject) => {
        const flasher = spawn('python', ['firmware_flasher_v2.py', firmwareType]);
        
        // Send config via stdin
        flasher.stdin.write(JSON.stringify(configData));
        flasher.stdin.end();
        
        flasher.on('close', (code) => {
            if (code === 0) resolve();
            else reject(new Error(`Flash failed with code ${code}`));
        });
    });
}
```

## Testing

1. Put your Pico in BOOTSEL mode (hold BOOTSEL while connecting USB)
2. Run: `python flasher_gui.py`
3. Select firmware type and config
4. Click "Flash Firmware"

The device should appear as a drive (e.g., E:\) and the firmware will be copied automatically.
