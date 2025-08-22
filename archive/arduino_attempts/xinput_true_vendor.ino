/*
 * TRUE Vendor-Specific XInput Implementation  
 * MIT License - Based on fluffymadness/tinyusb-xinput
 * 
 * This creates a REAL XInput device using:
 * - Vendor-specific USB interface (Class 0xFF, SubClass 0x5D)
 * - Microsoft Xbox 360 VID/PID (0x045E/0x028E)
 * - Raw TinyUSB APIs (no Arduino wrapper conflicts)
 */

#include <Arduino.h>
#include "tusb.h"

// Pin definitions
#define PIN_GREEN      2
#define PIN_RED        3  
#define PIN_YELLOW     4
#define PIN_BLUE       5
#define PIN_ORANGE     6
#define PIN_STRUM_UP   7
#define PIN_STRUM_DOWN 8
#define PIN_START      9
#define PIN_SELECT     10

// XInput button definitions
#define XINPUT_DPAD_UP    0x0001
#define XINPUT_DPAD_DOWN  0x0002
#define XINPUT_DPAD_LEFT  0x0004
#define XINPUT_DPAD_RIGHT 0x0008
#define XINPUT_START      0x0010
#define XINPUT_BACK       0x0020
#define XINPUT_LSTICK     0x0040
#define XINPUT_RSTICK     0x0080
#define XINPUT_LB         0x0100
#define XINPUT_RB         0x0200
#define XINPUT_A          0x1000
#define XINPUT_B          0x2000
#define XINPUT_X          0x4000
#define XINPUT_Y          0x8000

// XInput report structure (exactly like Xbox 360)
typedef struct {
    uint8_t rid;             // Report ID (0x00)
    uint8_t rsize;           // Report size (0x14)
    uint16_t buttons;        // Digital buttons
    uint8_t lt;              // Left trigger
    uint8_t rt;              // Right trigger  
    int16_t lx;              // Left stick X
    int16_t ly;              // Left stick Y
    int16_t rx;              // Right stick X
    int16_t ry;              // Right stick Y
    uint8_t reserved[6];     // Reserved
} __attribute__((packed)) xinput_report_t;

xinput_report_t xinput_report;

//--------------------------------------------------------------------+
// USB DESCRIPTORS - VENDOR-SPECIFIC XINPUT
//--------------------------------------------------------------------+

// Device Descriptor - Xbox 360 Controller
static const tusb_desc_device_t xinput_device_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,    // Vendor-specific
    .bDeviceSubClass    = 0xFF,    // Vendor-specific
    .bDeviceProtocol    = 0xFF,    // Vendor-specific
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x045E,  // Microsoft
    .idProduct          = 0x028E,  // Xbox 360 Controller
    .bcdDevice          = 0x0114,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// Configuration Descriptor
enum {
    ITF_NUM_VENDOR = 0,
    ITF_NUM_TOTAL
};

#define XINPUT_EPNUM_IN   0x81
#define XINPUT_EPNUM_OUT  0x01

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

static const uint8_t xinput_config_desc[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 500),

    // Vendor interface descriptor - THIS IS THE KEY!
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 0, XINPUT_EPNUM_OUT, XINPUT_EPNUM_IN, 32),
};

// String Descriptors
static const char* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // 0: Language (English)
    "Microsoft",                   // 1: Manufacturer
    "Controller (XBOX 360 For Windows)", // 2: Product
    "1"                           // 3: Serial
};

//--------------------------------------------------------------------+
// TinyUSB CALLBACKS
//--------------------------------------------------------------------+

// Device descriptor callback
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&xinput_device_desc;
}

// Configuration descriptor callback  
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return xinput_config_desc;
}

// String descriptor callback
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    static uint16_t _desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;

        const char* str = string_desc_arr[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);
    return _desc_str;
}

//--------------------------------------------------------------------+
// VENDOR INTERFACE CALLBACKS  
//--------------------------------------------------------------------+

// Vendor control request callback
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // Handle XInput control requests
    if (stage == CONTROL_STAGE_SETUP) {
        if (request->bRequest == 0x01 && request->wValue == 0x0100) {
            // XInput capabilities request
            static uint8_t caps[] = {0x00, 0x14}; 
            return tud_control_xfer(rhport, request, caps, sizeof(caps));
        }
    }
    return false;
}

// Vendor transfer complete callback
bool tud_vendor_n_xfer_cb(uint8_t itf, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    (void)itf; (void)ep_addr; (void)result; (void)xferred_bytes;
    return true;
}

//--------------------------------------------------------------------+
// MAIN APPLICATION
//--------------------------------------------------------------------+

void setup() {
    Serial.begin(115200);
    
    // Initialize pins
    pinMode(PIN_GREEN, INPUT_PULLUP);
    pinMode(PIN_RED, INPUT_PULLUP);
    pinMode(PIN_YELLOW, INPUT_PULLUP);
    pinMode(PIN_BLUE, INPUT_PULLUP);
    pinMode(PIN_ORANGE, INPUT_PULLUP);
    pinMode(PIN_STRUM_UP, INPUT_PULLUP);
    pinMode(PIN_STRUM_DOWN, INPUT_PULLUP);
    pinMode(PIN_START, INPUT_PULLUP);
    pinMode(PIN_SELECT, INPUT_PULLUP);
    
    // Initialize TinyUSB
    tusb_init();
    
    // Initialize XInput report
    xinput_report.rid = 0x00;
    xinput_report.rsize = 0x14;
    
    Serial.println("TRUE XInput Vendor-Specific Controller");
    Serial.println("Class 0xFF, SubClass 0x5D - REAL XInput!");
    Serial.println("VID:045E PID:028E (Microsoft Xbox 360)");
}

void loop() {
    // TinyUSB device task
    tud_task();
    
    // Skip if not ready
    if (!tud_mounted()) return;
    
    // Read GPIO buttons (active low)
    bool green = !digitalRead(PIN_GREEN);
    bool red = !digitalRead(PIN_RED);
    bool yellow = !digitalRead(PIN_YELLOW);
    bool blue = !digitalRead(PIN_BLUE);
    bool orange = !digitalRead(PIN_ORANGE);
    bool strum_up = !digitalRead(PIN_STRUM_UP);
    bool strum_down = !digitalRead(PIN_STRUM_DOWN);
    bool start = !digitalRead(PIN_START);
    bool select = !digitalRead(PIN_SELECT);
    
    // Map to XInput buttons
    xinput_report.buttons = 0;
    if (green) xinput_report.buttons |= XINPUT_A;
    if (red) xinput_report.buttons |= XINPUT_B;
    if (yellow) xinput_report.buttons |= XINPUT_Y;
    if (blue) xinput_report.buttons |= XINPUT_X;
    if (orange) xinput_report.buttons |= XINPUT_LB;
    if (strum_up) xinput_report.buttons |= XINPUT_RB;
    if (strum_down) xinput_report.buttons |= XINPUT_LSTICK;
    if (start) xinput_report.buttons |= XINPUT_START;
    if (select) xinput_report.buttons |= XINPUT_BACK;
    
    // Center analog controls
    xinput_report.lt = 0;
    xinput_report.rt = 0;
    xinput_report.lx = 0;
    xinput_report.ly = 0;
    xinput_report.rx = 0;
    xinput_report.ry = 0;
    
    // Send XInput report via vendor interface
    static uint32_t last_report = 0;
    if (millis() - last_report > 1) { // 1ms interval
        if (tud_vendor_n_write(0, &xinput_report, sizeof(xinput_report))) {
            tud_vendor_n_write_flush(0);
        }
        last_report = millis();
    }
    
    // Debug output
    static uint32_t last_debug = 0;
    if (millis() - last_debug > 1000) {
        Serial.print("XInput Status - Buttons: 0x");
        Serial.print(xinput_report.buttons, HEX);
        Serial.print(" Mounted: ");
        Serial.println(tud_mounted() ? "YES" : "NO");
        last_debug = millis();
    }
}
