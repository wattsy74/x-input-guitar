/*
 * BGG Guitar Controller - Simple HID Gamepad
 * Basic implementation to test USB enumeration
 * 
 * Hardware: Raspberry Pi Pico
 * Purpose: Simple HID gamepad that Windows will definitely recognize
 */

#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"

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

// Analog input pins
#define PIN_WHAMMY      26  // ADC0

//--------------------------------------------------------------------+
// USB HID REPORT DESCRIPTOR
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

//--------------------------------------------------------------------+
// USB DESCRIPTORS
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x1234,  // Generic VID
    .idProduct          = 0x5678,  // Generic PID
    .bcdDevice          = 0x0100,
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), 0x81, CFG_TUD_HID_EP_BUFSIZE, 10)
};

char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 }, // Language
    "BGG",                         // Manufacturer
    "Guitar Controller",           // Product
    "123",                        // Serial
};

//--------------------------------------------------------------------+
// HID GAMEPAD REPORT
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED {
    int8_t  x;         ///< Delta x  movement of left analog-stick
    int8_t  y;         ///< Delta y  movement of left analog-stick
    int8_t  z;         ///< Delta z  movement of right analog-stick
    int8_t  rz;        ///< Delta Rz movement of right analog-stick
    int8_t  rx;        ///< Delta Rx movement of analog left trigger
    int8_t  ry;        ///< Delta Ry movement of analog right trigger
    uint8_t hat;       ///< Buttons mask for currently pressed buttons (see HID_GAMEPAD_BUTTON_* masks)
    uint32_t buttons;  ///< Buttons mask for currently pressed buttons (see HID_GAMEPAD_BUTTON_* masks)
} hid_gamepad_report_t;

// Global gamepad state
static hid_gamepad_report_t gamepad_report = {0};

//--------------------------------------------------------------------+
// HARDWARE INITIALIZATION
//--------------------------------------------------------------------+
static void init_gpio(void) {
    // Initialize button pins with internal pullups
    const uint8_t button_pins[] = {
        PIN_GREEN, PIN_RED, PIN_YELLOW, PIN_BLUE, PIN_ORANGE,
        PIN_START, PIN_SELECT
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
    // Clear previous button state
    gamepad_report.buttons = 0;
    
    // Read digital buttons (active low) and map to gamepad buttons
    if (!gpio_get(PIN_GREEN))  gamepad_report.buttons |= GAMEPAD_BUTTON_A;     // Green -> A
    if (!gpio_get(PIN_RED))    gamepad_report.buttons |= GAMEPAD_BUTTON_B;     // Red -> B
    if (!gpio_get(PIN_YELLOW)) gamepad_report.buttons |= GAMEPAD_BUTTON_X;     // Yellow -> X
    if (!gpio_get(PIN_BLUE))   gamepad_report.buttons |= GAMEPAD_BUTTON_Y;     // Blue -> Y
    if (!gpio_get(PIN_ORANGE)) gamepad_report.buttons |= GAMEPAD_BUTTON_TL;    // Orange -> L1
    if (!gpio_get(PIN_START))  gamepad_report.buttons |= GAMEPAD_BUTTON_START; // Start -> Start
    if (!gpio_get(PIN_SELECT)) gamepad_report.buttons |= GAMEPAD_BUTTON_SELECT;// Select -> Select
    
    // Read whammy bar (12-bit ADC) and map to right trigger
    adc_select_input(0);
    uint16_t whammy = adc_read();
    gamepad_report.ry = (int8_t)((whammy >> 5) - 64); // Convert to signed 8-bit
    
    // Clear other analog inputs
    gamepad_report.x = 0;
    gamepad_report.y = 0;
    gamepad_report.z = 0;
    gamepad_report.rz = 0;
    gamepad_report.rx = 0;
    gamepad_report.hat = 0;
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

uint8_t const* tud_hid_descriptor_report_cb(uint8_t itf) {
    (void)itf;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// HID CALLBACKS
//--------------------------------------------------------------------+
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void)itf;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
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
        
        // Send HID report if USB is ready
        if (tud_hid_ready()) {
            tud_hid_report(0, &gamepad_report, sizeof(gamepad_report));
        }
        
        // Small delay
        sleep_ms(10);
    }
    
    return 0;
}
