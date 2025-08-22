#include "usb_mode_storage.h"
#include "config.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

//--------------------------------------------------------------------+
// FLASH STORAGE CONFIGURATION
//--------------------------------------------------------------------+
// Store USB mode preference in the last sector of flash
// Pico has 2MB flash, so we use the last 4KB sector
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define USB_MODE_MAGIC      0x42474721  // "BGG!" magic number
#define USB_MODE_VERSION    1

typedef struct {
    uint32_t magic;           // Magic number for validation
    uint32_t version;         // Structure version
    usb_mode_enum_t usb_mode; // Current USB mode
    uint32_t checksum;        // Simple checksum
    uint8_t padding[4080];    // Pad to sector size
} usb_mode_config_t;

static usb_mode_config_t current_config;
static bool config_loaded = false;

//--------------------------------------------------------------------+
// INTERNAL FUNCTIONS
//--------------------------------------------------------------------+

static uint32_t calculate_checksum(const usb_mode_config_t* config) {
    uint32_t sum = 0;
    sum += config->magic;
    sum += config->version;
    sum += (uint32_t)config->usb_mode;
    return sum;
}

static bool load_config_from_flash(void) {
    // Read from flash
    const uint8_t* flash_contents = (const uint8_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
    memcpy(&current_config, flash_contents, sizeof(usb_mode_config_t));
    
    // Validate magic number and checksum
    if (current_config.magic != USB_MODE_MAGIC || 
        current_config.version != USB_MODE_VERSION ||
        calculate_checksum(&current_config) != current_config.checksum) {
        
        // Invalid config, use defaults
        printf("Invalid USB mode config in flash, using defaults\n");
        current_config.magic = USB_MODE_MAGIC;
        current_config.version = USB_MODE_VERSION;
        current_config.usb_mode = USB_MODE_XINPUT;  // Default to XInput
        current_config.checksum = calculate_checksum(&current_config);
        return false;
    }
    
    printf("Loaded USB mode from flash: %s\n", usb_mode_to_string(current_config.usb_mode));
    return true;
}

static bool save_config_to_flash(void) {
    // Update checksum
    current_config.checksum = calculate_checksum(&current_config);
    
    // Disable interrupts during flash operation
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase the sector
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    // Write the config
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&current_config, sizeof(usb_mode_config_t));
    
    // Restore interrupts
    restore_interrupts(ints);
    
    printf("Saved USB mode to flash: %s\n", usb_mode_to_string(current_config.usb_mode));
    return true;
}

//--------------------------------------------------------------------+
// PUBLIC FUNCTIONS
//--------------------------------------------------------------------+

void usb_mode_storage_init(void) {
    if (!config_loaded) {
        load_config_from_flash();
        config_loaded = true;
    }
}

usb_mode_enum_t usb_mode_storage_get(void) {
    if (!config_loaded) {
        usb_mode_storage_init();
    }
    return current_config.usb_mode;
}

bool usb_mode_storage_set(usb_mode_enum_t mode) {
    if (!config_loaded) {
        usb_mode_storage_init();
    }
    
    if (current_config.usb_mode != mode) {
        current_config.usb_mode = mode;
        return save_config_to_flash();
    }
    
    return true;  // No change needed
}

usb_mode_enum_t usb_mode_check_boot_combo(void) {
    // Small delay to ensure pins are stable
    sleep_ms(50);
    
    // Check for button combinations
    // Green button (GPIO 10) = XInput mode
    // Red button (GPIO 11) = HID mode
    bool green_pressed = !gpio_get(device_config.button_pins.green);
    bool red_pressed = !gpio_get(device_config.button_pins.red);
    
    if (green_pressed && !red_pressed) {
        printf("Boot combo detected: XInput mode\n");
        usb_mode_storage_set(USB_MODE_XINPUT);
        return USB_MODE_XINPUT;
    } else if (red_pressed && !green_pressed) {
        printf("Boot combo detected: HID mode\n");
        usb_mode_storage_set(USB_MODE_HID);
        return USB_MODE_HID;
    } else if (green_pressed && red_pressed) {
        printf("Boot combo detected: Both buttons - staying in current mode\n");
    }
    
    // No combo detected, return stored mode
    return usb_mode_storage_get();
}

const char* usb_mode_to_string(usb_mode_enum_t mode) {
    switch (mode) {
        case USB_MODE_XINPUT: return "xinput";
        case USB_MODE_HID: return "hid";
        default: return "unknown";
    }
}

usb_mode_enum_t usb_mode_from_string(const char* mode_str) {
    if (strcmp(mode_str, "xinput") == 0) {
        return USB_MODE_XINPUT;
    } else if (strcmp(mode_str, "hid") == 0) {
        return USB_MODE_HID;
    }
    return USB_MODE_XINPUT;  // Default
}
