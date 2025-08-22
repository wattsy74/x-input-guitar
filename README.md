# BGG XInput Firmware

Working XInput firmware for Raspberry Pi Pico-based guitar controllers, compatible with the BumbleGum Guitars Windows application.

## Features

- ✅ **USB Enumeration**: Successfully appears as Xbox 360 Controller in Windows
- ✅ **Fret Buttons**: Green→A, Red→B, Yellow→Y, Blue→X, Orange→LB
- ✅ **DPad Controls**: Strum Up/Down, Left/Right navigation
- ✅ **Control Buttons**: Start, Select/Back, Guide
- ✅ **Analog Controls**: 
  - Left Stick (Joystick X/Y)
  - Right Stick X (Whammy)
  - Right Stick Y (Tilt)
- ✅ **NeoPixel Ready**: GPIO 23 pin available for LED support

## Pin Mapping

Based on `xInput-Button-Mappings.txt`:

### Digital Buttons
- **Green Fret** → A → GPIO 10
- **Red Fret** → B → GPIO 11  
- **Yellow Fret** → Y → GPIO 12
- **Blue Fret** → X → GPIO 13
- **Orange Fret** → Left Shoulder → GPIO 14
- **Start** → Start → GPIO 1
- **Select** → Back → GPIO 0
- **Centre** → Guide → GPIO 6

### DPad
- **Strum Up** → DPad Up → GPIO 7 & GPIO 2
- **Strum Down** → DPad Down → GPIO 8 & GPIO 3
- **Left** → DPad Left → GPIO 4
- **Right** → DPad Right → GPIO 5

### Analog Inputs (ADC)
- **Joystick X-Axis** → Left Stick X → GPIO 28 (ADC2)
- **Joystick Y-Axis** → Left Stick Y → GPIO 29 (ADC3)
- **Whammy** → Right Stick X → GPIO 27 (ADC1)
- **Tilt** → Right Stick Y → GPIO 9 (Digital)

### Other
- **Neopixel Data** → GPIO 23

## Technical Details

- **Framework**: TinyUSB with custom XInput driver
- **Based on**: fluffymadness XInput implementation
- **USB Protocol**: Xbox 360 Controller emulation
- **VID/PID**: 0x045E:0x028E (Microsoft Xbox 360 Controller)
- **Report Structure**: Standard Xbox 360 XInput format with int16_t analog values

## Building

1. Make sure you have the Pico SDK installed and configured
2. Create build directory: `mkdir build && cd build`
3. Configure CMake: `cmake ..`
4. Build: `ninja`
5. Output: `bgg_xinput_firmware.uf2`

## Installation

1. Hold BOOTSEL button on Pico while connecting USB
2. Copy `bgg_xinput_firmware.uf2` to the RPI-RP2 drive
3. Pico will reboot and appear as Xbox 360 Controller

## File Structure

- `main_fluffymadness_exact.cpp` - Main firmware file with working XInput implementation
- `xInput-Button-Mappings.txt` - Complete pin mapping specification
- `config.h` - Configuration structure for BGG app compatibility
- `CMakeLists.txt` - Build configuration
- `tusb_config.h` - USB stack configuration

## Status

Current firmware provides stable Xbox 360 controller functionality with all buttons, analog controls, and Guide button working correctly. Ready for NeoPixel LED integration.

## Next Steps

- [ ] Integrate NeoPixel LED support
- [ ] Add BGG Windows App configuration support
- [ ] Add LED patterns and effects
- [ ] Optimize performance
