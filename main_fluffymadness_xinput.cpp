/*
 * BGG XInput Firmware - Fluffymadness Compatible
 * Based on proven fluffymadness XInput library
 * 
 * This implementation uses the exact same approach as the working
 * Arduino XInput library for maximum compatibility
 */

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>

//--------------------------------------------------------------------+
// XINPUT DEFINITIONS - Exact fluffymadness implementation
//--------------------------------------------------------------------+
// Use official Xbox 360 Controller identifiers
#define USB_VENDOR_ID   0x045E    // Microsoft
#define USB_PRODUCT_ID  0x028E    // Xbox 360 Controller

// XInput button definitions
#define BUTTON_A        0x1000
#define BUTTON_B        0x2000  
#define BUTTON_X        0x4000
#define BUTTON_Y        0x8000
#define BUTTON_LB       0x0100
#define BUTTON_RB       0x0200
#define BUTTON_BACK     0x0020
#define BUTTON_START    0x0010
#define BUTTON_LOGO     0x0400
#define DPAD_UP         0x0001
#define DPAD_DOWN       0x0002
#define DPAD_LEFT       0x0004
#define DPAD_RIGHT      0x0008

//--------------------------------------------------------------------+
// GPIO DEFINITIONS
//--------------------------------------------------------------------+
#define PIN_GREEN     2
#define PIN_RED       3  
#define PIN_YELLOW    4
#define PIN_BLUE      5
#define PIN_ORANGE    6
#define PIN_START     7
#define PIN_SELECT    8
#define PIN_GUIDE     9
#define PIN_STRUM_UP  10
#define PIN_STRUM_DOWN 11
#define PIN_WHAMMY    26  // ADC0

//--------------------------------------------------------------------+
// USB DESCRIPTORS - Exact fluffymadness format
//--------------------------------------------------------------------+

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,
    .bDeviceSubClass    = 0xFF, 
    .bDeviceProtocol    = 0xFF,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VENDOR_ID,
    .idProduct          = USB_PRODUCT_ID,
    .bcdDevice          = 0x0572,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// Configuration Descriptor - Raw bytes from working implementation
static uint8_t const desc_configuration[] = {
    // Configuration Descriptor (9 bytes)
    0x09,        // bLength
    0x02,        // bDescriptorType (Configuration)
    0x30, 0x00,  // wTotalLength (48 bytes)
    0x01,        // bNumInterfaces
    0x01,        // bConfigurationValue
    0x00,        // iConfiguration
    0x80,        // bmAttributes (Bus powered)
    0xFA,        // bMaxPower (500mA)

    // Interface Descriptor (9 bytes)
    0x09,        // bLength
    0x04,        // bDescriptorType (Interface)
    0x00,        // bInterfaceNumber
    0x00,        // bAlternateSetting
    0x02,        // bNumEndpoints
    0xFF,        // bInterfaceClass (Vendor Specific)
    0x5D,        // bInterfaceSubClass (Xbox 360)
    0x01,        // bInterfaceProtocol (Xbox 360 Controller)
    0x00,        // iInterface

    // Xbox 360 Specific Descriptor (17 bytes)
    0x11,        // bLength
    0x21,        // bDescriptorType (Xbox 360 specific)
    0x10, 0x01, 0x01, 0x24, 0x81, 0x14, 0x03, 0x00,
    0x03, 0x13, 0x02, 0x00, 0x03, 0x00,

    // Endpoint Descriptor IN (7 bytes)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x81,        // bEndpointAddress (IN)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x04,        // bInterval (4ms)

    // Endpoint Descriptor OUT (7 bytes)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x01,        // bEndpointAddress (OUT)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x08         // bInterval (8ms)
};

// String Descriptors
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: English (0x0409)
    "Microsoft Corporation",        // 1: Manufacturer
    "Controller (XBOX 360 For Windows)", // 2: Product
    "00000001",                    // 3: Serial Number
};

//--------------------------------------------------------------------+
// XINPUT GAMEPAD REPORT
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
    uint8_t  reserved[6];    // Padding
} xinput_gamepad_t;

static xinput_gamepad_t gamepad = {0};

//--------------------------------------------------------------------+
// GUITAR INPUT STATE
//--------------------------------------------------------------------+
static bool buttons[16] = {false};
static uint16_t whammy_value = 0;

//--------------------------------------------------------------------+
// HARDWARE INITIALIZATION
//--------------------------------------------------------------------+
static void init_gpio(void) {
    // Initialize button pins with pullup
    uint8_t button_pins[] = {
        PIN_GREEN, PIN_RED, PIN_YELLOW, PIN_BLUE, PIN_ORANGE,
        PIN_START, PIN_SELECT, PIN_GUIDE, PIN_STRUM_UP, PIN_STRUM_DOWN
    };
    
    for (int i = 0; i < 10; i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
    
    // Initialize ADC for whammy
    adc_init();
    adc_gpio_init(PIN_WHAMMY);
}

//--------------------------------------------------------------------+
// INPUT READING
//--------------------------------------------------------------------+
static void read_inputs(void) {
    // Read buttons (active low)
    buttons[0] = !gpio_get(PIN_GREEN);    // A
    buttons[1] = !gpio_get(PIN_RED);      // B  
    buttons[2] = !gpio_get(PIN_YELLOW);   // X
    buttons[3] = !gpio_get(PIN_BLUE);     // Y
    buttons[4] = !gpio_get(PIN_ORANGE);   // LB
    buttons[5] = false;                   // RB
    buttons[6] = !gpio_get(PIN_SELECT);   // BACK
    buttons[7] = !gpio_get(PIN_START);    // START
    buttons[8] = false;                   // L3
    buttons[9] = false;                   // R3
    buttons[10] = !gpio_get(PIN_GUIDE);   // LOGO
    buttons[11] = !gpio_get(PIN_STRUM_UP);   // DPAD_UP
    buttons[12] = !gpio_get(PIN_STRUM_DOWN); // DPAD_DOWN
    buttons[13] = false;                  // DPAD_LEFT
    buttons[14] = false;                  // DPAD_RIGHT
    
    // Read whammy
    adc_select_input(0);
    whammy_value = adc_read();
}

//--------------------------------------------------------------------+
// XINPUT REPORT GENERATION
//--------------------------------------------------------------------+
static void update_gamepad(void) {
    // Clear gamepad
    memset(&gamepad, 0, sizeof(gamepad));
    
    gamepad.report_id = 0x00;
    gamepad.report_size = 0x14;
    
    // Map buttons
    if (buttons[0]) gamepad.buttons |= BUTTON_A;
    if (buttons[1]) gamepad.buttons |= BUTTON_B;
    if (buttons[2]) gamepad.buttons |= BUTTON_X;
    if (buttons[3]) gamepad.buttons |= BUTTON_Y;
    if (buttons[4]) gamepad.buttons |= BUTTON_LB;
    if (buttons[5]) gamepad.buttons |= BUTTON_RB;
    if (buttons[6]) gamepad.buttons |= BUTTON_BACK;
    if (buttons[7]) gamepad.buttons |= BUTTON_START;
    if (buttons[10]) gamepad.buttons |= BUTTON_LOGO;
    if (buttons[11]) gamepad.buttons |= DPAD_UP;
    if (buttons[12]) gamepad.buttons |= DPAD_DOWN;
    if (buttons[13]) gamepad.buttons |= DPAD_LEFT;
    if (buttons[14]) gamepad.buttons |= DPAD_RIGHT;
    
    // Map whammy to right trigger
    gamepad.right_trigger = (uint8_t)(whammy_value >> 4); // 12-bit to 8-bit
}

//--------------------------------------------------------------------+
// USB CALLBACKS
//--------------------------------------------------------------------+
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    
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

// Vendor control transfer callback
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    if (stage == CONTROL_STAGE_SETUP) {
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
            switch (request->bRequest) {
                case 0x01: // Get capabilities
                    if (request->wIndex == 0x0100) {
                        uint8_t const response[] = {
                            0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00
                        };
                        return tud_control_xfer(rhport, request, (void*)response, sizeof(response));
                    }
                    break;
            }
        }
    }
    return false;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    // Handle rumble data here if needed
    (void)itf; (void)buffer; (void)bufsize;
}

//--------------------------------------------------------------------+
// MAIN APPLICATION  
//--------------------------------------------------------------------+
int main(void) {
    // Initialize hardware
    board_init();
    init_gpio();
    
    // Initialize USB
    tud_init(0);
    
    while (1) {
        // Service USB
        tud_task();
        
        // Read inputs
        read_inputs();
        update_gamepad();
        
        // Send report
        if (tud_vendor_mounted()) {
            tud_vendor_write(&gamepad, sizeof(gamepad));
            tud_vendor_flush();
        }
        
        sleep_ms(1);
    }
    
    return 0;
}
 
 2 1   A u g u s t   2 0 2 5   2 1 : 0 0 : 5 4  
  
  
 