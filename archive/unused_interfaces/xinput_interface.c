#include "xinput_interface.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>

//--------------------------------------------------------------------+
// XINPUT CONSTANTS
//--------------------------------------------------------------------+
#define XINPUT_VID        0x045E  // Microsoft
#define XINPUT_PID        0x028E  // Xbox 360 Controller (wired)
#define XINPUT_BCD        0x0572  // Different revision

// XInput button bit masks
#define XINPUT_DPAD_UP       0x0001
#define XINPUT_DPAD_DOWN     0x0002  
#define XINPUT_DPAD_LEFT     0x0004
#define XINPUT_DPAD_RIGHT    0x0008
#define XINPUT_START         0x0010
#define XINPUT_BACK          0x0020
#define XINPUT_LSTICK        0x0040
#define XINPUT_RSTICK        0x0080
#define XINPUT_LB            0x0100
#define XINPUT_RB            0x0200
#define XINPUT_GUIDE         0x0400
#define XINPUT_A             0x1000
#define XINPUT_B             0x2000
#define XINPUT_X             0x4000
#define XINPUT_Y             0x8000

//--------------------------------------------------------------------+
// XINPUT REPORT STRUCTURE
//--------------------------------------------------------------------+
typedef struct {
    uint16_t buttons;  // Button states
    uint8_t lt;        // Left trigger (0-255)
    uint8_t rt;        // Right trigger (0-255)
    int16_t lx;        // Left stick X (-32768 to 32767)
    int16_t ly;        // Left stick Y (-32768 to 32767)
    int16_t rx;        // Right stick X (-32768 to 32767)
    int16_t ry;        // Right stick Y (-32768 to 32767)
    uint8_t reserved[6]; // Reserved bytes
} __attribute__((packed)) xinput_report_t;

// Global XInput report
static xinput_report_t xinput_report;

//--------------------------------------------------------------------+
// USB DESCRIPTORS (MICROSOFT XINPUT)
//--------------------------------------------------------------------+

// NOTE: Device descriptor moved to usb_descriptors.c to prevent conflicts

// Configuration Descriptor with XINPUT-specific interface  
enum {
    ITF_NUM_VENDOR = 0,
    ITF_NUM_TOTAL
};

#define EPNUM_VENDOR_IN   0x81
#define EPNUM_VENDOR_OUT  0x01
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + 9 + 17 + 7 + 7)  // Config + Interface + Unknown + 2 Endpoints

static uint8_t const desc_configuration[] = {
    // Config descriptor: config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 500),

    // XInput Interface Descriptor (Class 0xFF, SubClass 0x5D, Protocol 0x01)
    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    ITF_NUM_VENDOR,        // bInterfaceNumber
    0x00,        // bAlternateSetting
    0x02,        // bNumEndpoints
    0xFF,        // bInterfaceClass (Vendor Specific)
    0x5D,        // bInterfaceSubClass (XInput!)
    0x01,        // bInterfaceProtocol (XInput)
    0x00,        // iInterface

    // CRITICAL: XInput Unknown Descriptor (0x21) - This is what Microsoft driver looks for!
    0x11,        // bLength (17 bytes)
    0x21,        // bDescriptorType (Unknown/Vendor specific)
    0x00, 0x01, 0x01, 0x25, 0x81, 0x14, 0x00, 0x00,
    0x00, 0x00, 0x13, 0x01, 0x00, 0x03, 0x00,

    // Endpoint Descriptor (IN)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    EPNUM_VENDOR_IN,   // bEndpointAddress (IN)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x04,        // bInterval (4ms)

    // Endpoint Descriptor (OUT)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    EPNUM_VENDOR_OUT,  // bEndpointAddress (OUT)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x08,        // bInterval (8ms)
};

// Microsoft OS String Descriptor - CRITICAL for Windows XInput recognition
uint8_t const desc_ms_os_20[] = {
    0x0A, 0x00,             // wLength
    0x00, 0x00, 0x00, 0x00, // dwMS_VendorCode
    'M', 'S', 'F', 'T',     // qwSignature
    '1', '0', '0',          // MS OS 2.0 descriptor
    0xEE                    // bMS_VendorCode
};

//--------------------------------------------------------------------+
// STRING DESCRIPTOR HANDLER
//--------------------------------------------------------------------+

static const char* xinput_get_string_descriptor(uint8_t index) {
    static const char* string_desc_arr[] = {
        "",                              // 0: language descriptor (handled separately)
        "Microsoft",                     // 1: Manufacturer
        "Controller (XBOX 360 For Windows)",  // 2: Product
        "1.0",                          // 3: Serials, should use chip ID
    };
    
    // Note: the 0xEE index string is a Microsoft OS string descriptor
    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) {
        return NULL;
    }
    
    return string_desc_arr[index];
}

//--------------------------------------------------------------------+
// USB DESCRIPTOR CALLBACKS
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// DESCRIPTOR HANDLERS
//--------------------------------------------------------------------+

static const uint8_t* xinput_get_device_descriptor(void) {
    return (uint8_t const *) &desc_device;
}

static const uint8_t* xinput_get_configuration_descriptor(uint8_t index) {
    (void) index;
    return desc_configuration;
}

//--------------------------------------------------------------------+
// VENDOR CONTROL CALLBACKS (CRITICAL FOR XINPUT)
//--------------------------------------------------------------------+

// Invoked when received control transfer with MS Vendor Code
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    if (stage != CONTROL_STAGE_SETUP) return true;

    switch (request->bmRequestType_bit.type) {
        case TUSB_REQ_TYPE_VENDOR:
            switch (request->bRequest) {
                case 0x01:  // Get MS OS 2.0 Descriptor
                    if (request->wIndex == 0x0004) {
                        // MS OS 2.0 descriptor request
                        return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, sizeof(desc_ms_os_20));
                    }
                    break;
                default: break;
            }
            break;
        case TUSB_REQ_TYPE_CLASS:
            if (request->bRequest == 0x01 && request->wValue == 0x0100) {
                // This is the critical XInput initialization request
                uint8_t response[20] = {0};
                return tud_control_xfer(rhport, request, response, sizeof(response));
            }
            break;
        default: break;
    }

    return false;
}

//--------------------------------------------------------------------+
// XINPUT INTERFACE IMPLEMENTATION
//--------------------------------------------------------------------+

static void xinput_init(void) {
    // Initialize TinyUSB
    tusb_init();
    
    // Initialize XInput report structure
    memset(&xinput_report, 0, sizeof(xinput_report));
    
    printf("XInput interface initialized\n");
}

static void xinput_task(void) {
    // TinyUSB device task
    tud_task();
}

static void xinput_send_report(const button_state_t* state) {
    // Map Guitar Hero controls to XInput gamepad
    memset(&xinput_report, 0, sizeof(xinput_report));
    xinput_report.buttons = 0;
    
    // Fret buttons -> Face buttons (Clone Hero standard mapping)
    if (state->green) xinput_report.buttons |= XINPUT_A;        // Green -> A
    if (state->red) xinput_report.buttons |= XINPUT_B;          // Red -> B
    if (state->yellow) xinput_report.buttons |= XINPUT_Y;       // Yellow -> Y
    if (state->blue) xinput_report.buttons |= XINPUT_X;         // Blue -> X
    if (state->orange) xinput_report.buttons |= XINPUT_LB;      // Orange -> LB (Left Bumper)

    // Strum bar -> D-Pad mapping for Clone Hero compatibility (like original working version)
    if (state->strum_up) xinput_report.buttons |= XINPUT_DPAD_UP;      // Strum up -> D-Pad Up
    if (state->strum_down) xinput_report.buttons |= XINPUT_DPAD_DOWN;  // Strum down -> D-Pad Down

    // Control buttons
    if (state->start) xinput_report.buttons |= XINPUT_START;
    if (state->select) xinput_report.buttons |= XINPUT_BACK;
    if (state->guide) xinput_report.buttons |= XINPUT_GUIDE;

    // D-Pad - Map physical D-Pad to shoulder buttons to avoid conflict with strum (like original)
    // Physical D-Pad up/down mapped to available buttons to avoid strum conflict
    if (state->dpad_up) xinput_report.buttons |= XINPUT_RB;          // Physical D-Pad Up -> RB
    if (state->dpad_down) xinput_report.buttons |= XINPUT_LSTICK;    // Physical D-Pad Down -> LS
    if (state->dpad_left) xinput_report.buttons |= XINPUT_DPAD_LEFT;      // D-Pad Left still works
    if (state->dpad_right) xinput_report.buttons |= XINPUT_DPAD_RIGHT;    // D-Pad Right still works

    // Analog controls - Fixed whammy data type issue
    xinput_report.lt = 0;                         // Left trigger -> unused
    xinput_report.rt = state->whammy;             // Right trigger -> Whammy bar (now uint8_t)
    xinput_report.lx = state->joy_x;              // Left stick X -> Joystick X
    xinput_report.ly = state->joy_y;              // Left stick Y -> Joystick Y  
    xinput_report.rx = 0;                                 // Right stick X -> unused
    xinput_report.ry = 0;                                 // Right stick Y -> unused

    // Send the report if USB is ready with proper XInput packet format
    if (tud_vendor_mounted() && tud_vendor_write_available() >= 22) {
        // Send XInput input report (20 bytes data + 2 byte header = 22 bytes total)
        uint8_t report_packet[22];
        report_packet[0] = 0x00; // Message type: input report
        report_packet[1] = 0x14; // Report size: 20 bytes
        
        // Copy the XInput report data (20 bytes)
        memcpy(&report_packet[2], &xinput_report, sizeof(xinput_report));

        tud_vendor_write(report_packet, sizeof(report_packet));
        tud_vendor_write_flush();
    }
}

static bool xinput_ready(void) {
    return tud_vendor_mounted();
}

// XInput interface definition
const usb_interface_t xinput_interface = {
    .init = xinput_init,
    .task = xinput_task,
    .send_report = xinput_send_report,
    .ready = xinput_ready,
    .get_string_descriptor = xinput_get_string_descriptor,
    .get_device_descriptor = xinput_get_device_descriptor,
    .get_configuration_descriptor = xinput_get_configuration_descriptor
};
