cmake_minimum_required(VERSION 3.13)

# Initialize the SDK based on PICO_SDK_PATH
include(pico_sdk_import.cmake)

project(bgg_xinput_firmware)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# BGG XInput Guitar Controller - Exact Fluffymadness Implementation
add_executable(bgg_xinput_firmware
    main_fluffymadness_exact.cpp
)

# Add required libraries
target_link_libraries(bgg_xinput_firmware 
    pico_stdlib
    pico_unique_id
    hardware_gpio
    hardware_adc
    tinyusb_device
    tinyusb_board
)

# Add our project directory to include path for tusb_config.h
target_include_directories(bgg_xinput_firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enable XInput custom driver - exact fluffymadness approach
target_compile_definitions(bgg_xinput_firmware PRIVATE 
    CFG_TUD_VENDOR=0
    CFG_TUD_CDC=0
    CFG_TUD_HID=0
    CFG_TUSB_DEBUG=0
)

# Enable UART output for debugging, disable USB stdio
pico_enable_stdio_usb(bgg_xinput_firmware 0)
pico_enable_stdio_uart(bgg_xinput_firmware 1)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(bgg_xinput_firmware)
