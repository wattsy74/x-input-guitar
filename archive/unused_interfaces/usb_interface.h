#ifndef USB_INTERFACE_H
#define USB_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_mode_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

// Button state structure
typedef struct {
    // Fret buttons
    bool green;
    bool red;
    bool yellow;
    bool blue;
    bool orange;
    
    // Strum bar
    bool strum_up;
    bool strum_down;
    
    // Control buttons
    bool start;
    bool select;
    bool guide;
    
    // D-Pad
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    
    // Analog inputs
    uint8_t whammy;      // Changed from uint16_t to uint8_t for proper XInput compatibility
    int16_t joy_x;
    int16_t joy_y;
} button_state_t;

// USB interface function pointers
typedef struct {
    void (*init)(void);
    void (*task)(void);
    void (*send_report)(const button_state_t* state);
    bool (*ready)(void);
    const char* (*get_string_descriptor)(uint8_t index);
    const uint8_t* (*get_device_descriptor)(void);
    const uint8_t* (*get_configuration_descriptor)(uint8_t index);
} usb_interface_t;

// Initialize USB interface based on config mode
void usb_interface_init(usb_mode_enum_t mode);

// Get current USB interface
const usb_interface_t* usb_interface_get(void);

// Get current USB mode
usb_mode_enum_t usb_interface_get_current_mode(void);

// Note: read_button_states() is now implemented in main.cpp

#ifdef __cplusplus
}
#endif

#endif // USB_INTERFACE_H
