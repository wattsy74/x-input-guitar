# BGG XInput Firmware - Workspace Cleanup Plan

## ✅ KEEP - Working Build System
```
build_pico/                    # Working CMake build directory
├── bgg_xinput_firmware.elf   # ✅ WORKING firmware
└── ...build files...

CMakeLists.txt                # ✅ WORKING build config
main.cpp                      # ✅ WORKING source (restored from backup)
config.c                      # ✅ Shared config
config.h                      # ✅ Shared config  
tusb_config.h                 # ✅ TinyUSB config
xinput_interface.c/.h         # ✅ XInput implementation
usb_interface.c/.h            # ✅ USB helpers
neopixel.c/.h                 # ✅ LED support
```

## ✅ KEEP - App Integration
```
BGG-Windows-App v4.0/         # ✅ Existing Electron app
flasher_gui.py                # ✅ New GUI flasher
firmware_flasher_v2.py        # ✅ Core flasher logic
config/                       # ✅ Config files
firmware_binaries/            # ✅ For pre-compiled firmware
```

## 🗑️ DELETE - Redundant/Broken Files
```
# Old ELF files (keep only the working one)
bgg_xinput_firmware_*.elf     # 🗑️ DELETE (15+ old versions)

# Failed build attempts  
firmware_builder*.py          # 🗑️ DELETE (Arduino CLI attempts)
build_firmware_variants.py    # 🗑️ DELETE
firmware_flasher.py           # 🗑️ DELETE (old version)

# Duplicate/old source files
temp_backup/                  # 🗑️ DELETE (already restored what we need)
main_*.cpp                    # 🗑️ DELETE (duplicates)
bgg-xinput-*.ino              # 🗑️ DELETE (Arduino attempts)
xinput_*.ino                  # 🗑️ DELETE (Arduino attempts)

# Old build directories
build/                        # 🗑️ DELETE
build_clean/                  # 🗑️ DELETE  
build_dual/                   # 🗑️ DELETE
build_xinput/                 # 🗑️ DELETE
```

## 📦 MOVE - Archive Old Work
```
archive/
├── old_elf_files/           # Move old .elf files here
├── arduino_attempts/        # Move .ino files here  
├── experimental_builds/     # Move main_*.cpp here
└── failed_builders/         # Move firmware_builder*.py here
```
