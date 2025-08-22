#ifndef USB_MODE_STORAGE_H
#define USB_MODE_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB modes
typedef enum {
    USB_MODE_XINPUT = 0,
    USB_MODE_HID = 1
} usb_mode_enum_t;

// Boot button combinations for mode switching
#define BOOT_COMBO_XINPUT_MASK  (1 << 0)  // Green button (GPIO 10)
#define BOOT_COMBO_HID_MASK     (1 << 1)  // Red button (GPIO 11)

// Initialize USB mode storage system
void usb_mode_storage_init(void);

// Get current USB mode from flash storage
usb_mode_enum_t usb_mode_storage_get(void);

// Set USB mode and save to flash storage
bool usb_mode_storage_set(usb_mode_enum_t mode);

// Check for boot button combination and return detected mode
// Returns current stored mode if no combo detected
usb_mode_enum_t usb_mode_check_boot_combo(void);

// Get USB mode as string
const char* usb_mode_to_string(usb_mode_enum_t mode);

// Convert string to USB mode
usb_mode_enum_t usb_mode_from_string(const char* mode_str);

#ifdef __cplusplus
}
#endif

#endif // USB_MODE_STORAGE_H
