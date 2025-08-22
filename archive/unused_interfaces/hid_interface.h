#ifndef HID_INTERFACE_H
#define HID_INTERFACE_H

#include "usb_interface.h"
#include "tusb.h"

#ifdef __cplusplus
extern "C" {
#endif

// HID interface implementation
extern const usb_interface_t hid_interface;

// HID report descriptor (for centralized USB descriptors)
extern uint8_t const desc_hid_report[];

#ifdef __cplusplus
}
#endif

#endif // HID_INTERFACE_H
