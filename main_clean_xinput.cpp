/*
 * BGG XInput Firmware - Clean Implementation
 * Based on fluffymadness XInput approach
 * 
 * This is a fresh start focusing on:
 * 1. Proper XInput controller enumeration
 * 2. Correct button and analog mappings
 * 3. Clean, maintainable code structure
 */

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "bsp/board.h"
#include "tusb.h"
#include <string.h>

//--------------------------------------------------------------------+
// XINPUT CONTROLLER CONFIGURATION
//--------------------------------------------------------------------+
// Use official Xbox 360 Controller identifiers for maximum compatibility
#define XBOX_VID 0x045E
#define XBOX_PID 0x028E

// Guitar Hero button mappings to XInput buttons
#define XINPUT_GREEN     0x1000  // Y button
#define XINPUT_RED       0x2000  // B button  
#define XINPUT_YELLOW    0x8000  // X button
#define XINPUT_BLUE      0x4000  // A button
#define XINPUT_ORANGE    0x0100  // Left shoulder
#define XINPUT_START     0x0010  // Start button
#define XINPUT_SELECT    0x0020  // Back button
#define XINPUT_GUIDE     0x0400  // Guide button
#define XINPUT_STRUM_UP  0x0001  // DPad Up
#define XINPUT_STRUM_DOWN 0x0002 // DPad Down

//--------------------------------------------------------------------+
// GPIO PIN DEFINITIONS
//--------------------------------------------------------------------+
// Button inputs (active low with internal pullups)
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

// Analog inputs
#define PIN_WHAMMY    26  // ADC0
#define PIN_TILT      27  // ADC1 (if needed)

//--------------------------------------------------------------------+
// XINPUT REPORT STRUCTURE
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED {
    uint8_t  report_id;      // 0x00
    uint8_t  report_size;    // 0x14 (20 bytes)
    uint16_t buttons;        // Button bitmask
    uint8_t  left_trigger;   // Left trigger (0-255)
    uint8_t  right_trigger;  // Right trigger (0-255)
    int16_t  left_thumb_x;   // Left stick X (-32768 to 32767)
    int16_t  left_thumb_y;   // Left stick Y (-32768 to 32767)
    int16_t  right_thumb_x;  // Right stick X (-32768 to 32767)
    int16_t  right_thumb_y;  // Right stick Y (-32768 to 32767)
    uint8_t  reserved[6];    // Reserved/padding
} xinput_report_t;

static xinput_report_t xinput_report = {
    .report_id = 0x00,
    .report_size = 0x14,
    .buttons = 0,
    .left_trigger = 0,
    .right_trigger = 0,
    .left_thumb_x = 0,
    .left_thumb_y = 0,
    .right_thumb_x = 0,
    .right_thumb_y = 0,
    .reserved = {0}
};

//--------------------------------------------------------------------+
// USB DESCRIPTORS
//--------------------------------------------------------------------+
// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,    // Vendor specific
    .bDeviceSubClass    = 0xFF,
    .bDeviceProtocol    = 0xFF,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = XBOX_VID,
    .idProduct          = XBOX_PID,
    .bcdDevice          = 0x0114,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration Descriptor
enum {
    ITF_NUM_XINPUT = 0,
    ITF_NUM_TOTAL
};

#define EPNUM_XINPUT_OUT  0x01
#define EPNUM_XINPUT_IN   0x81

// XInput Configuration Descriptor
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
    0x5D,        // bInterfaceSubClass (Xbox)
    0x01,        // bInterfaceProtocol (Controller)
    0x00,        // iInterface

    // Unknown Descriptor (Xbox specific)
    0x11,        // bLength
    0x21,        // bDescriptorType
    0x10, 0x01, 0x01, 0x24, 0x81, 0x14, 0x03, 0x00,
    0x03, 0x13, 0x02, 0x00, 0x03, 0x00,

    // Endpoint Descriptor (IN)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x81,        // bEndpointAddress (IN)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x04,        // bInterval (4ms)

    // Endpoint Descriptor (OUT)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    0x01,        // bEndpointAddress (OUT)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes)
    0x08         // bInterval (8ms)
};

// String Descriptors
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: Language (English)
    "Microsoft",                    // 1: Manufacturer
    "Controller (XBOX 360 For Windows)", // 2: Product
    "1234567890",                  // 3: Serial number
};

//--------------------------------------------------------------------+
// GUITAR INPUT VARIABLES
//--------------------------------------------------------------------+
static bool btn_green = false;
static bool btn_red = false;
static bool btn_yellow = false;
static bool btn_blue = false;
static bool btn_orange = false;
static bool btn_start = false;
static bool btn_select = false;
static bool btn_guide = false;
static bool btn_strum_up = false;
static bool btn_strum_down = false;
static uint16_t whammy_raw = 0;

//--------------------------------------------------------------------+
// HARDWARE INITIALIZATION
//--------------------------------------------------------------------+
static void gpio_init_buttons(void) {
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
}

static void adc_init_whammy(void) {
    // Initialize ADC for whammy bar
    adc_init();
    adc_gpio_init(PIN_WHAMMY);
    adc_select_input(0); // ADC0
}

//--------------------------------------------------------------------+
// INPUT READING FUNCTIONS
//--------------------------------------------------------------------+
static void read_button_inputs(void) {
    // Read all button states (active low)
    btn_green = !gpio_get(PIN_GREEN);
    btn_red = !gpio_get(PIN_RED);
    btn_yellow = !gpio_get(PIN_YELLOW);
    btn_blue = !gpio_get(PIN_BLUE);
    btn_orange = !gpio_get(PIN_ORANGE);
    btn_start = !gpio_get(PIN_START);
    btn_select = !gpio_get(PIN_SELECT);
    btn_guide = !gpio_get(PIN_GUIDE);
    btn_strum_up = !gpio_get(PIN_STRUM_UP);
    btn_strum_down = !gpio_get(PIN_STRUM_DOWN);
}

static void read_analog_inputs(void) {
    // Read whammy bar (12-bit ADC)
    whammy_raw = adc_read();
}

//--------------------------------------------------------------------+
// XINPUT REPORT GENERATION
//--------------------------------------------------------------------+
static void update_xinput_report(void) {
    // Clear previous button state
    xinput_report.buttons = 0;
    
    // Map guitar buttons to XInput buttons
    if (btn_green) xinput_report.buttons |= XINPUT_GREEN;
    if (btn_red) xinput_report.buttons |= XINPUT_RED;
    if (btn_yellow) xinput_report.buttons |= XINPUT_YELLOW;
    if (btn_blue) xinput_report.buttons |= XINPUT_BLUE;
    if (btn_orange) xinput_report.buttons |= XINPUT_ORANGE;
    if (btn_start) xinput_report.buttons |= XINPUT_START;
    if (btn_select) xinput_report.buttons |= XINPUT_SELECT;
    if (btn_guide) xinput_report.buttons |= XINPUT_GUIDE;
    if (btn_strum_up) xinput_report.buttons |= XINPUT_STRUM_UP;
    if (btn_strum_down) xinput_report.buttons |= XINPUT_STRUM_DOWN;
    
    // Map whammy bar to right trigger (0-255 range)
    xinput_report.right_trigger = (uint8_t)(whammy_raw >> 4); // Convert 12-bit to 8-bit
    
    // Clear other analog inputs for now
    xinput_report.left_trigger = 0;
    xinput_report.left_thumb_x = 0;
    xinput_report.left_thumb_y = 0;
    xinput_report.right_thumb_x = 0;
    xinput_report.right_thumb_y = 0;
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
// USB VENDOR CALLBACKS
//--------------------------------------------------------------------+
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // Handle XInput specific control requests
    if (stage == CONTROL_STAGE_SETUP) {
        if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
            switch (request->bRequest) {
                case 0x01: // XInput get capabilities
                    if (request->wIndex == 0x0100) {
                        // Return XInput capabilities
                        static uint8_t const capabilities[] = {
                            0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00
                        };
                        return tud_control_xfer(rhport, request, (void*)capabilities, sizeof(capabilities));
                    }
                    break;
                    
                default:
                    return false;
            }
        }
    }
    
    return false;
}

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    // Handle incoming vendor data (rumble commands, etc.)
    (void)itf;
    (void)buffer;
    (void)bufsize;
}

//--------------------------------------------------------------------+
// MAIN FUNCTIONS
//--------------------------------------------------------------------+
void setup(void) {
    // Initialize Pico SDK
    board_init();
    
    // Initialize hardware
    gpio_init_buttons();
    adc_init_whammy();
    
    // Initialize USB (use port 0 for RP2040)
    tud_init(0);
}

void loop(void) {
    // Process USB tasks
    tud_task();
    
    // Read inputs
    read_button_inputs();
    read_analog_inputs();
    
    // Update XInput report
    update_xinput_report();
    
    // Send report if connected and ready
    if (tud_vendor_mounted()) {
        // Send XInput report
        tud_vendor_write(&xinput_report, sizeof(xinput_report));
        tud_vendor_flush();
    }
    
    // Small delay to prevent overwhelming USB
    sleep_ms(1);
}

//--------------------------------------------------------------------+
// MAIN ENTRY POINT
//--------------------------------------------------------------------+
int main(void) {
    setup();
    
    while (1) {
        loop();
    }
    
    return 0;
}
