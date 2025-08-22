#include "usb_interface.h"
#include "usb_mode_storage.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <string.h>
#include <stdio.h>
#include "tusb.h"

//--------------------------------------------------------------------+
// SHARED USB DESCRIPTORS
//--------------------------------------------------------------------+

// String descriptor buffer
static uint16_t _desc_str[32];

// Unified string descriptor callback - supports both XInput and HID modes
const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&chr_count, (const char[]){0x09, 0x04}, 1);
        chr_count = 1;
    } else {
        // Get the current USB interface to determine string arrays
        const usb_interface_t* interface = usb_interface_get();
        if (!interface || !interface->get_string_descriptor) {
            return NULL;
        }
        
        const char* str = interface->get_string_descriptor(index);
        if (!str) return NULL;

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        // Convert ASCII string into UTF-16
        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}

//--------------------------------------------------------------------+
// USB DESCRIPTOR CALLBACKS (DISPATCH TO CURRENT INTERFACE)
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const * tud_descriptor_device_cb(void) {
    const usb_interface_t* interface = usb_interface_get();
    if (interface && interface->get_device_descriptor) {
        return interface->get_device_descriptor();
    }
    return NULL;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    const usb_interface_t* interface = usb_interface_get();
    if (interface && interface->get_configuration_descriptor) {
        return interface->get_configuration_descriptor(index);
    }
    return NULL;
}

// Forward declarations for interface implementations
extern const usb_interface_t xinput_interface;
// Note: HID interface will be implemented in a future update

// Current active interface
static const usb_interface_t* current_interface = NULL;
static usb_mode_enum_t current_mode = USB_MODE_XINPUT;

void usb_interface_init(usb_mode_enum_t mode) {
    printf("Initializing USB interface: %s\n", usb_mode_to_string(mode));
    
    current_mode = mode;  // Store current mode
    
    if (mode == USB_MODE_XINPUT) {
        current_interface = &xinput_interface;
    } else if (mode == USB_MODE_HID) {
        // For now, fall back to XInput if HID is requested
        printf("HID mode not yet implemented, using XInput\n");
        current_interface = &xinput_interface;
        current_mode = USB_MODE_XINPUT;  // Update mode to reflect actual mode
    } else {
        // Default to XInput if mode is unrecognized
        current_interface = &xinput_interface;
        current_mode = USB_MODE_XINPUT;
    }
    
    if (current_interface && current_interface->init) {
        current_interface->init();
    }
}

const usb_interface_t* usb_interface_get(void) {
    return current_interface;
}

usb_mode_enum_t usb_interface_get_current_mode(void) {
    return current_mode;
}
