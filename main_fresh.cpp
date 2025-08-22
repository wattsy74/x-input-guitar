/*
 * BGG XInput Guitar Controller Firmware
 * Clean implementation based on fluffymadness XInput library
 * 
 * Hardware: Raspberry Pi Pico
 * Purpose: Guitar Hero/Rock Band controller with XInput compatibility
 * Author: BGG Project
 * Date: August 2025
 */

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>

//--------------------------------------------------------------------+
// HARDWARE CONFIGURATION
//--------------------------------------------------------------------+
// Guitar button pins (active low with internal pullups)
#define PIN_GREEN       2
#define PIN_RED         3  
#define PIN_YELLOW      4
#define PIN_BLUE        5
#define PIN_ORANGE      6
#define PIN_START       7
#define PIN_SELECT      8
#define PIN_GUIDE       9
#define PIN_STRUM_UP    10
#define PIN_STRUM_DOWN  11

// Analog input pins
#define PIN_WHAMMY      26  // ADC0

//--------------------------------------------------------------------+
// XINPUT CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
// Xbox 360 Controller USB identifiers
#define XBOX_VID        0x045E    // Microsoft Vendor ID
#define XBOX_PID        0x028E    // Xbox 360 Controller Product ID

// XInput button bit masks (from fluffymadness library)
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_GUIDE            0x0400
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

//--------------------------------------------------------------------+
// USB DESCRIPTORS - Exact Xbox 360 Controller Format
//--------------------------------------------------------------------+

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,    // Vendor Specific Class
    .bDeviceSubClass    = 0xFF,    // Vendor Specific Subclass
    .bDeviceProtocol    = 0xFF,    // Vendor Specific Protocol
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = XBOX_VID,
    .idProduct          = XBOX_PID,
    .bcdDevice          = 0x0572,  // Device Release Number
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// Configuration Descriptor (Corrected Xbox 360 format)
uint8_t const desc_configuration[] = {
    // Configuration Descriptor
    0x09,        // bLength
    0x02,        // bDescriptorType (Configuration)
    0x30, 0x00,  // wTotalLength (48 bytes)
    0x01,        // bNumInterfaces
    0x01,        // bConfigurationValue
    0x00,        // iConfiguration
    0x80,        // bmAttributes (Bus powered)
    0xFA,        // bMaxPower (500mA)

    // Interface Descriptor
    0x09,        // bLength  
    0x04,        // bDescriptorType (Interface)
    0x00,        // bInterfaceNumber
    0x00,        // bAlternateSetting
    0x02,        // bNumEndpoints
    0xFF,        // bInterfaceClass (Vendor Specific)
    0x5D,        // bInterfaceSubClass (Xbox Controller)
    0x01,        // bInterfaceProtocol (Xbox Controller)
    0x00,        // iInterface

    // Xbox Controller Specific Descriptor
    0x11,        // bLength (17 bytes)
    0x21,        // bDescriptorType (Xbox specific)
    0x10, 0x01, 0x01, 0x25, 0x81, 0x14, 0x00, 0x00,
    0x00, 0x00, 0x13, 0x01, 0x00, 0x00, 0x00,

    // Endpoint Descriptor (IN)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x81,        // bEndpointAddress (EP1 IN)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x01,        // bInterval (1ms)

    // Endpoint Descriptor (OUT)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x01,        // bEndpointAddress (EP1 OUT)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x08         // bInterval (8ms)
};

// String Descriptors
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: Language (English)
    "Microsoft Corporation",       // 1: Manufacturer
    "Controller (XBOX 360 For Windows)", // 2: Product
    "1",                          // 3: Serial Number
};

//--------------------------------------------------------------------+
// XINPUT GAMEPAD REPORT STRUCTURE
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED {
    uint8_t  report_id;      // Always 0x00
    uint8_t  report_size;    // Always 0x14 (20 bytes)
    uint16_t buttons;        // Button bitmask
    uint8_t  left_trigger;   // Left trigger (0-255)
    uint8_t  right_trigger;  // Right trigger (0-255)
    int16_t  left_thumb_x;   // Left stick X (-32768 to 32767)
    int16_t  left_thumb_y;   // Left stick Y (-32768 to 32767)
    int16_t  right_thumb_x;  // Right stick X (-32768 to 32767)
    int16_t  right_thumb_y;  // Right stick Y (-32768 to 32767)
    uint8_t  reserved[6];    // Reserved bytes
} xinput_gamepad_report_t;

// Global gamepad state
static xinput_gamepad_report_t gamepad_report = {
    .report_id = 0x00,
    .report_size = 0x14
};

//--------------------------------------------------------------------+
// GUITAR INPUT STATE
//--------------------------------------------------------------------+
static struct {
    bool green;
    bool red;
    bool yellow;
    bool blue;
    bool orange;
    bool start;
    bool select;
    bool guide;
    bool strum_up;
    bool strum_down;
    uint16_t whammy;
} guitar_state = {0};

//--------------------------------------------------------------------+
// HARDWARE INITIALIZATION
//--------------------------------------------------------------------+
static void init_gpio(void) {
    // Initialize button pins with internal pullups
    const uint8_t button_pins[] = {
        PIN_GREEN, PIN_RED, PIN_YELLOW, PIN_BLUE, PIN_ORANGE,
        PIN_START, PIN_SELECT, PIN_GUIDE, PIN_STRUM_UP, PIN_STRUM_DOWN
    };
    
    for (int i = 0; i < sizeof(button_pins); i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
    
    // Initialize ADC for whammy bar
    adc_init();
    adc_gpio_init(PIN_WHAMMY);
}

//--------------------------------------------------------------------+
// INPUT READING
//--------------------------------------------------------------------+
static void read_guitar_inputs(void) {
    // Read digital buttons (active low)
    guitar_state.green = !gpio_get(PIN_GREEN);
    guitar_state.red = !gpio_get(PIN_RED);
    guitar_state.yellow = !gpio_get(PIN_YELLOW);
    guitar_state.blue = !gpio_get(PIN_BLUE);
    guitar_state.orange = !gpio_get(PIN_ORANGE);
    guitar_state.start = !gpio_get(PIN_START);
    guitar_state.select = !gpio_get(PIN_SELECT);
    guitar_state.guide = !gpio_get(PIN_GUIDE);
    guitar_state.strum_up = !gpio_get(PIN_STRUM_UP);
    guitar_state.strum_down = !gpio_get(PIN_STRUM_DOWN);
    
    // Read whammy bar (12-bit ADC)
    adc_select_input(0);
    guitar_state.whammy = adc_read();
}

//--------------------------------------------------------------------+
// XINPUT REPORT GENERATION
//--------------------------------------------------------------------+
static void update_xinput_report(void) {
    // Clear previous button state
    gamepad_report.buttons = 0;
    
    // Map guitar buttons to XInput buttons
    if (guitar_state.green)     gamepad_report.buttons |= XINPUT_GAMEPAD_A;      // Green -> A
    if (guitar_state.red)       gamepad_report.buttons |= XINPUT_GAMEPAD_B;      // Red -> B
    if (guitar_state.yellow)    gamepad_report.buttons |= XINPUT_GAMEPAD_Y;      // Yellow -> Y
    if (guitar_state.blue)      gamepad_report.buttons |= XINPUT_GAMEPAD_X;      // Blue -> X
    if (guitar_state.orange)    gamepad_report.buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER; // Orange -> LB
    if (guitar_state.start)     gamepad_report.buttons |= XINPUT_GAMEPAD_START;  // Start -> Start
    if (guitar_state.select)    gamepad_report.buttons |= XINPUT_GAMEPAD_BACK;   // Select -> Back
    if (guitar_state.guide)     gamepad_report.buttons |= XINPUT_GAMEPAD_GUIDE;  // Guide -> Guide
    if (guitar_state.strum_up)  gamepad_report.buttons |= XINPUT_GAMEPAD_DPAD_UP; // Strum Up -> DPad Up
    if (guitar_state.strum_down) gamepad_report.buttons |= XINPUT_GAMEPAD_DPAD_DOWN; // Strum Down -> DPad Down
    
    // Map whammy bar to right trigger
    gamepad_report.right_trigger = (uint8_t)(guitar_state.whammy >> 4); // 12-bit to 8-bit
    
    // Clear other analog inputs
    gamepad_report.left_trigger = 0;
    gamepad_report.left_thumb_x = 0;
    gamepad_report.left_thumb_y = 0;
    gamepad_report.right_thumb_x = 0;
    gamepad_report.right_thumb_y = 0;
    
    // Clear reserved bytes
    memset(gamepad_report.reserved, 0, sizeof(gamepad_report.reserved));
}

//--------------------------------------------------------------------+
// USB DESCRIPTOR CALLBACKS
//--------------------------------------------------------------------+
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    
    static uint16_t _desc_str[32];
    uint8_t chr_count;
    
    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;
        
        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;
        
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }
    
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

//--------------------------------------------------------------------+
// USB VENDOR CLASS CALLBACKS
//--------------------------------------------------------------------+
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // Handle XInput specific control requests
    if (stage == CONTROL_STAGE_SETUP) {
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
            // XInput capability descriptor request
            if (request->bRequest == 0x01) {
                static uint8_t const xinput_capabilities[] = {
                    0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00
                };
                return tud_control_xfer(rhport, request, (void*)xinput_capabilities, sizeof(xinput_capabilities));
            }
        }
    }
    return false;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    // Handle XInput commands from host (rumble, etc.)
    (void)itf;
    (void)buffer;
    (void)bufsize;
    
    // Echo any received data back
    tud_vendor_write(buffer, bufsize);
    tud_vendor_flush();
}

//--------------------------------------------------------------------+
// MAIN APPLICATION
//--------------------------------------------------------------------+
int main(void) {
    // Initialize board and hardware
    board_init();
    init_gpio();
    
    // Initialize USB device
    tud_init(0);
    
    // Main loop
    while (1) {
        // Service USB tasks
        tud_task();
        
        // Read guitar inputs
        read_guitar_inputs();
        
        // Update XInput report
        update_xinput_report();
        
        // Send report if USB is ready
        if (tud_vendor_mounted()) {
            tud_vendor_write(&gamepad_report, sizeof(gamepad_report));
            tud_vendor_flush();
        }
        
        // Small delay
        sleep_ms(1);
    }
    
    return 0;
}
