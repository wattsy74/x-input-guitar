/*
 * BGG XInput Firmware - Working Version
 * Based on TinyUSB HID Gamepad examples and ArduinoXInput patterns
 * 
 * This version should provide stable USB enumeration and basic functionality
 */

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "bsp/board.h"
#include "tusb.h"
#include "config.h"
#include "neopixel.h"
#include <string.h>

//--------------------------------------------------------------------+
// USB DEVICE DESCRIPTOR
//--------------------------------------------------------------------+
// Use official Xbox 360 Controller VID/PID for maximum compatibility
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,          // USB 2.0
    .bDeviceClass       = 0x00,            // Interface-defined class
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x045E,          // Microsoft
    .idProduct          = 0x028E,          // Xbox 360 Controller
    .bcdDevice          = 0x0114,          // Device version
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

//--------------------------------------------------------------------+
// HID REPORT DESCRIPTOR - Xbox 360 Compatible Gamepad
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

//--------------------------------------------------------------------+
// CONFIGURATION DESCRIPTOR
//--------------------------------------------------------------------+
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID   0x81

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
};

//--------------------------------------------------------------------+
// STRING DESCRIPTORS
//--------------------------------------------------------------------+
char const* string_desc_arr[] = {
    (const char[]) {0x09, 0x04}, // 0: Language (English)
    "Microsoft",                 // 1: Manufacturer
    "Controller (XBOX 360 For Windows)", // 2: Product  
    "1234567890",               // 3: Serial
};

//--------------------------------------------------------------------+
// DEVICE CALLBACKS
//--------------------------------------------------------------------+
void tud_mount_cb(void) {
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

void tud_umount_cb(void) {
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    gpio_put(PICO_DEFAULT_LED_PIN, 0);
}

void tud_resume_cb(void) {
    gpio_put(PICO_DEFAULT_LED_PIN, 1);
}

//--------------------------------------------------------------------+
// USB HID CALLBACKS
//--------------------------------------------------------------------+
// Invoked when received GET_REPORT control request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

// Invoked when received SET_REPORT control request or received data on OUT endpoint
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

//--------------------------------------------------------------------+
// USB DESCRIPTOR CALLBACKS
//--------------------------------------------------------------------+
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    
    static uint16_t _desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

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
// GAMEPAD REPORT STRUCTURE
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED {
    int8_t  x;         ///< Delta x  movement of left analog-stick
    int8_t  y;         ///< Delta y  movement of left analog-stick
    int8_t  z;         ///< Delta z  movement of right analog-joystick
    int8_t  rz;        ///< Delta Rz movement of right analog-joystick
    int8_t  rx;        ///< Delta Rx movement of analog left trigger
    int8_t  ry;        ///< Delta Ry movement of analog right trigger
    uint8_t hat;       ///< Buttons mask for currently pressed buttons (not used)
    uint32_t buttons;  ///< Buttons mask for currently pressed buttons
} bgg_gamepad_report_t;

// Global report structure
static bgg_gamepad_report_t gamepad_report = {0};

//--------------------------------------------------------------------+
// GUITAR BUTTON MAPPING
//--------------------------------------------------------------------+
// Button states  
static bool green, red, yellow, blue, orange;
static bool strum_up, strum_down, start, select, guide;
static bool dpad_up, dpad_down, dpad_left, dpad_right;
static uint16_t whammy_value;

// NeoPixel state
static bool neopixel_initialized = false;
static config_t device_config;

void read_guitar_buttons(void) {
    // Read all button states (active LOW with pull-ups)
    green = !gpio_get(config_get_green_pin());
    red = !gpio_get(config_get_red_pin());
    yellow = !gpio_get(config_get_yellow_pin());
    blue = !gpio_get(config_get_blue_pin());
    orange = !gpio_get(config_get_orange_pin());
    strum_up = !gpio_get(config_get_strum_up_pin());
    strum_down = !gpio_get(config_get_strum_down_pin());
    start = !gpio_get(config_get_start_pin());
    select = !gpio_get(config_get_select_pin());
    
    // Additional inputs
    guide = !gpio_get(6);  // Guide button on GPIO 6
    dpad_up = !gpio_get(config_get_dpad_up_pin());
    dpad_down = !gpio_get(config_get_dpad_down_pin());
    dpad_left = !gpio_get(config_get_dpad_left_pin());
    dpad_right = !gpio_get(config_get_dpad_right_pin());

    // Read whammy bar (analog)
    uint8_t whammy_adc_channel = config_get_whammy_pin() - 26;
    adc_select_input(whammy_adc_channel);
    uint16_t whammy_raw = adc_read();
    whammy_value = (uint16_t)((whammy_raw * 255UL) / 4095UL);

    // Clear previous report
    memset(&gamepad_report, 0, sizeof(gamepad_report));
    
    // Map Guitar Hero buttons to HID gamepad buttons
    uint32_t buttons = 0;
    
    // Fret buttons -> Face buttons  
    if (green) buttons |= GAMEPAD_BUTTON_A;
    if (red) buttons |= GAMEPAD_BUTTON_B;
    if (yellow) buttons |= GAMEPAD_BUTTON_Y;
    if (blue) buttons |= GAMEPAD_BUTTON_X;
    if (orange) buttons |= GAMEPAD_BUTTON_TL;  // Left shoulder
    
    // Control buttons
    if (start) buttons |= GAMEPAD_BUTTON_START;
    if (select) buttons |= GAMEPAD_BUTTON_SELECT;
    if (guide) buttons |= GAMEPAD_BUTTON_MODE;  // Guide button
    
    // Strum -> Right shoulder buttons  
    if (strum_up) buttons |= GAMEPAD_BUTTON_TR;
    if (strum_down) buttons |= GAMEPAD_BUTTON_TL2;

    gamepad_report.buttons = buttons;
    
    // D-Pad mapping
    uint8_t hat = GAMEPAD_HAT_CENTERED;
    if (dpad_up && dpad_right) hat = GAMEPAD_HAT_UP_RIGHT;
    else if (dpad_up && dpad_left) hat = GAMEPAD_HAT_UP_LEFT;
    else if (dpad_down && dpad_right) hat = GAMEPAD_HAT_DOWN_RIGHT;
    else if (dpad_down && dpad_left) hat = GAMEPAD_HAT_DOWN_LEFT;
    else if (dpad_up) hat = GAMEPAD_HAT_UP;
    else if (dpad_down) hat = GAMEPAD_HAT_DOWN;
    else if (dpad_left) hat = GAMEPAD_HAT_LEFT;
    else if (dpad_right) hat = GAMEPAD_HAT_RIGHT;
    
    gamepad_report.hat = hat;
    
    // Analog inputs
    gamepad_report.rx = (int8_t)(whammy_value - 128);  // Whammy as analog trigger
}

//--------------------------------------------------------------------+
// MAIN FUNCTION
//--------------------------------------------------------------------+
int main(void) {
    board_init();
    
    // Initialize LED
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    // Initialize configuration system
    config_init();
    
    // Initialize GPIO pins for buttons (with pull-ups)
    gpio_init(config_get_green_pin());
    gpio_set_dir(config_get_green_pin(), GPIO_IN);
    gpio_pull_up(config_get_green_pin());

    gpio_init(config_get_red_pin());
    gpio_set_dir(config_get_red_pin(), GPIO_IN);
    gpio_pull_up(config_get_red_pin());

    gpio_init(config_get_yellow_pin());
    gpio_set_dir(config_get_yellow_pin(), GPIO_IN);
    gpio_pull_up(config_get_yellow_pin());

    gpio_init(config_get_blue_pin());
    gpio_set_dir(config_get_blue_pin(), GPIO_IN);
    gpio_pull_up(config_get_blue_pin());

    gpio_init(config_get_orange_pin());
    gpio_set_dir(config_get_orange_pin(), GPIO_IN);
    gpio_pull_up(config_get_orange_pin());

    gpio_init(config_get_strum_up_pin());
    gpio_set_dir(config_get_strum_up_pin(), GPIO_IN);
    gpio_pull_up(config_get_strum_up_pin());

    gpio_init(config_get_strum_down_pin());
    gpio_set_dir(config_get_strum_down_pin(), GPIO_IN);
    gpio_pull_up(config_get_strum_down_pin());

    gpio_init(config_get_start_pin());
    gpio_set_dir(config_get_start_pin(), GPIO_IN);
    gpio_pull_up(config_get_start_pin());

    gpio_init(config_get_select_pin());
    gpio_set_dir(config_get_select_pin(), GPIO_IN);
    gpio_pull_up(config_get_select_pin());

    gpio_init(config_get_dpad_up_pin());
    gpio_set_dir(config_get_dpad_up_pin(), GPIO_IN);
    gpio_pull_up(config_get_dpad_up_pin());

    gpio_init(config_get_dpad_down_pin());
    gpio_set_dir(config_get_dpad_down_pin(), GPIO_IN);
    gpio_pull_up(config_get_dpad_down_pin());

    gpio_init(config_get_dpad_left_pin());
    gpio_set_dir(config_get_dpad_left_pin(), GPIO_IN);
    gpio_pull_up(config_get_dpad_left_pin());

    gpio_init(config_get_dpad_right_pin());
    gpio_set_dir(config_get_dpad_right_pin(), GPIO_IN);
    gpio_pull_up(config_get_dpad_right_pin());

    // Guide button
    gpio_init(6);
    gpio_set_dir(6, GPIO_IN);
    gpio_pull_up(6);

    // Initialize ADC
    adc_init();
    adc_gpio_init(config_get_whammy_pin());

    // Initialize TinyUSB
    tusb_init();

    // Wait for USB enumeration
    sleep_ms(2000);
    
    // Initialize NeoPixels after USB
    neopixel_init(&device_config);
    neopixel_initialized = true;
    
    // Show startup pattern
    neopixel_set_all(0xFF000000);  // White
    neopixel_show();
    sleep_ms(500);
    neopixel_set_all(0x00000000);  // Off
    neopixel_show();

    printf("BGG Guitar Hero Controller - HID Gamepad Mode\n");
    printf("USB enumeration should be complete\n");

    // Main loop
    while (1) {
        // TinyUSB device task
        tud_task();
        
        // Read guitar inputs
        read_guitar_buttons();
        
        // Update NeoPixels based on button states
        if (neopixel_initialized) {
            bool fret_states[5] = {green, red, yellow, blue, orange};
            neopixel_update_button_state(&device_config, fret_states, strum_up, strum_down);
        }
        
        // Send HID report
        if (tud_hid_ready()) {
            tud_hid_report(0, &gamepad_report, sizeof(gamepad_report));
        }
        
        // Small delay for stability
        sleep_ms(10);
    }

    return 0;
}
