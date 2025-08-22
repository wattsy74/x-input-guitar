/*
 * Universal TinyUSB Configuration
 * 
 * This config enables both HID and Vendor classes so we can dynamically
 * choose which one to use based on runtime mode detection.
 */

#ifndef _TUSB_CONFIG_UNIVERSAL_H_
#define _TUSB_CONFIG_UNIVERSAL_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// BOARD SPECIFIC CONFIGURATION
//--------------------------------------------------------------------

// RHPort number used for device
#define BOARD_TUD_RHPORT      0

// RHPort max operational speed
#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// Defined by compiler flags
#ifndef CFG_TUSB_MCU
  #error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
  #error CFG_TUSB_OS must be defined
#endif

// Enable device stack
#define CFG_TUD_ENABLED       1

// Default to Endpoint size to 64
#define CFG_TUD_ENDPOINT0_SIZE    64

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

// Buffer size for control transfers
#define CFG_TUD_ENDPOINT0_SIZE   64

// Enable both HID and Vendor classes for universal support
#define CFG_TUD_HID              1
#define CFG_TUD_VENDOR           1

// Disable other classes we don't need
#define CFG_TUD_CDC              0
#define CFG_TUD_MSC              0
#define CFG_TUD_AUDIO            0
#define CFG_TUD_VIDEO            0
#define CFG_TUD_MIDI             0
#define CFG_TUD_DFU_RUNTIME      0
#define CFG_TUD_DFU              0
#define CFG_TUD_ECM_RNDIS        0
#define CFG_TUD_NCM              0
#define CFG_TUD_BTH              0

// HID buffer size Should be sufficient to hold ID (if any) + Data
#define CFG_TUD_HID_EP_BUFSIZE    64

// Vendor FIFO size of TX and RX
#define CFG_TUD_VENDOR_EPSIZE     64

//--------------------------------------------------------------------
// USB DESCRIPTORS CONFIGURATION
//--------------------------------------------------------------------

// String descriptor index for Manufacturer, Product, Serial
#define USBD_MANUFACTURER_STRING_INDEX  1
#define USBD_PRODUCT_STRING_INDEX       2
#define USBD_SERIAL_STRING_INDEX        3

// Language ID for string descriptors
#define USBD_LANGID_STRING              0x0409  // English (US)

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_UNIVERSAL_H_ */
