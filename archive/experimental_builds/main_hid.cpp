#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "pico/stdio.h"
#include "bsp/board.h"
#include "config.h"
#include "tusb.h"
#include "class/hid/hid_device.h"
#include <string.h>

//--------------------------------------------------------------------+
// NEOPIXEL DEBUG FUNCTIONS (Simple Bit-Banged)
//--------------------------------------------------------------------+
#define NEOPIXEL_PIN 23
#define NUM_PIXELS 7

// RGB Color values (GRB format for WS2812)
#define RGB_RED     0x00FF00  // Green=0, Red=255, Blue=0
#define RGB_GREEN   0xFF0000  // Green=255, Red=0, Blue=0
#define RGB_BLUE    0x0000FF  // Green=0, Red=0, Blue=255
#define RGB_YELLOW  0xFFFF00  // Green=255, Red=255, Blue=0
#define RGB_PURPLE  0x00FF80  // Green=0, Red=255, Blue=128
#define RGB_CYAN    0xFF0080  // Green=255, Red=0, Blue=128
#define RGB_WHITE   0xFFFFFF  // Green=255, Red=255, Blue=255
#define RGB_OFF     0x000000  // All off

// Fixed WS2812 timing (800kHz) - Proper microsecond delays
void neopixel_send_bit(bool bit) {
    if (bit) {
        // Send '1': HIGH for 0.8us, LOW for 0.4us
        gpio_put(NEOPIXEL_PIN, 1);
        busy_wait_us_32(1);  // More precise timing
        gpio_put(NEOPIXEL_PIN, 0);
        busy_wait_us_32(1);
    } else {
        // Send '0': HIGH for 0.4us, LOW for 0.8us
        gpio_put(NEOPIXEL_PIN, 1);
        busy_wait_us_32(1);
        gpio_put(NEOPIXEL_PIN, 0);
        busy_wait_us_32(2);
    }
}

void neopixel_send_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        neopixel_send_bit((byte >> i) & 1);
    }
}

void neopixel_send_pixel(uint32_t color) {
    uint8_t green = (color >> 16) & 0xFF;
    uint8_t red = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;
    
    // WS2812 expects GRB order
    neopixel_send_byte(green);
    neopixel_send_byte(red);
    neopixel_send_byte(blue);
}

//--------------------------------------------------------------------+
// HID GAMEPAD REPORT STRUCTURE
//--------------------------------------------------------------------+
// Use TinyUSB's built-in gamepad report structure
static hid_gamepad_report_t hid_report = {0};

//--------------------------------------------------------------------+
// USB DESCRIPTORS
//--------------------------------------------------------------------+

// HID Report Descriptor - Standard Gamepad
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

// Invoked when received GET HID REPORT DESCRIPTOR
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// USB DESCRIPTORS
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// USB DESCRIPTORS
//--------------------------------------------------------------------+

// Device Descriptor
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for HID
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x1234,  // Generic VID (change as needed)
    .idProduct          = 0x5678,  // Generic PID (change as needed)
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

// Configuration Descriptor
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

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; // for multiple configurations
    return desc_configuration;
}

// String Descriptors
char const* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    "BGG",                         // 1: Manufacturer
    "Guitar Hero Controller",      // 2: Product
    "123456",                      // 3: Serials, should use chip ID
};

static uint16_t _desc_str[32];

// Invoked when received GET STRING DESCRIPTOR request
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if ( index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if ( chr_count > 31 ) chr_count = 31;

        // Convert ASCII string into UTF-16
        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}

//--------------------------------------------------------------------+
// USB MODE AND CONFIGURATION
//--------------------------------------------------------------------+

// USB mode enum (simplified for HID-only firmware)
typedef enum {
    USB_MODE_XINPUT = 0,  // Not used in HID firmware
    USB_MODE_HID = 1      // Always HID mode
} usb_mode_enum_t;

// Always HID mode in this firmware
static usb_mode_enum_t current_usb_mode = USB_MODE_HID;

// Forward declarations
usb_mode_enum_t detect_boot_combo(void);

#define USB_MODE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

// Global button state variables for easy access
static bool green, red, yellow, blue, orange;
static bool strum_up, strum_down, start, select, guide;
static bool dpad_up, dpad_down, dpad_left, dpad_right;
static uint16_t whammy_value;
static int16_t tilt_x, tilt_y;

//--------------------------------------------------------------------+
// USB MODE STORAGE (simplified for HID-only)
//--------------------------------------------------------------------+

void usb_mode_save(usb_mode_enum_t mode) {
    // TODO: Save to flash storage
}

usb_mode_enum_t usb_mode_load(void) {
    // Always return HID mode for this firmware
    return USB_MODE_HID;
}

// BOOT COMBO DETECTION
//--------------------------------------------------------------------+
usb_mode_enum_t detect_boot_combo(void) {
    // Check if Green button is pressed at boot - still indicate XInput preference
    if (!gpio_get(device_config.button_pins.green)) {
        printf("BOOT COMBO: Green button detected - XInput preference (but running HID firmware)\\n");
        return USB_MODE_HID;  // Still run HID mode
    }
    
    // Check if Red button is pressed at boot for HID mode
    if (!gpio_get(device_config.button_pins.red)) {
        printf("BOOT COMBO: Red button detected - HID mode selected\\n");
        return USB_MODE_HID;
    }
    
    // No combo detected, use HID mode (this firmware is HID-only)
    printf("BOOT COMBO: No combo detected - using HID mode\\n");
    return USB_MODE_HID;
}

//--------------------------------------------------------------------+
// DEVICE CALLBACKS
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
    gpio_put(25, 1);  // Built-in LED
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    gpio_put(25, 0);  // Built-in LED
}

// Invoked when usb bus is suspended
void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    gpio_put(25, 0);  // Built-in LED
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
    gpio_put(25, 1);  // Built-in LED
}

//--------------------------------------------------------------------+
// HID CALLBACKS
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
// GUITAR BUTTON READING FUNCTION
//--------------------------------------------------------------------+

void read_guitar_buttons(void) {
    // Debug counter for random guide button triggers
    static uint32_t guide_trigger_count = 0;
    static uint32_t last_count_report = 0;
    uint32_t now = time_us_32() / 1000; // Convert to milliseconds
    
    // Read digital button states (active LOW with internal pull-ups) using config values
    green = !gpio_get(device_config.button_pins.green);
    red = !gpio_get(device_config.button_pins.red);
    yellow = !gpio_get(device_config.button_pins.yellow);
    blue = !gpio_get(device_config.button_pins.blue);
    orange = !gpio_get(device_config.button_pins.orange);
    strum_up = !gpio_get(device_config.button_pins.strum_up);
    strum_down = !gpio_get(device_config.button_pins.strum_down);
    start = !gpio_get(device_config.button_pins.start);
    select = !gpio_get(device_config.button_pins.select);
    
    // Initialize GPIO pins for tilt sensor and guide button separately
    static bool extra_gpio_initialized = false;
    if (!extra_gpio_initialized) {
        // Tilt sensor on GPIO 9 - combines with Select for Star Power
        gpio_init(9);
        gpio_set_dir(9, GPIO_IN);
        gpio_pull_up(9);
        
        // Guide button on GPIO 6 - brings up Xbox guide menu (remapped for HID)
        gpio_init(6);
        gpio_set_dir(6, GPIO_IN);
        gpio_pull_up(6);
        
        // Additional D-Pad inputs (independent of strum)
        gpio_init(7);  // GPIO 7 -> both STRUM UP and HAT UP
        gpio_set_dir(7, GPIO_IN);
        gpio_pull_up(7);
        
        gpio_init(8);  // GPIO 8 -> both STRUM DOWN and HAT DOWN
        gpio_set_dir(8, GPIO_IN);
        gpio_pull_up(8);
        
        extra_gpio_initialized = true;
    }
    
    // Read tilt sensor (GPIO 9) with debouncing
    static uint32_t tilt_debounce_time = 0;
    static bool tilt_last_state = false;
    bool tilt_raw = !gpio_get(9);
    
    bool tilt_active = tilt_last_state;  // Default to last stable state
    if (tilt_raw != tilt_last_state) {
        if (now - tilt_debounce_time > 50) {  // 50ms debounce
            tilt_active = tilt_raw;
            tilt_last_state = tilt_raw;
            printf("Tilt sensor %s\\n", tilt_active ? "ACTIVE" : "INACTIVE");
        }
        tilt_debounce_time = now;
    }
    
    // Guide button (GPIO 6) - Back to original simple version that was working
    guide = !gpio_get(6);
    
    // Read all D-Pad inputs independently - no conflicts, just multiple inputs -> same output
    bool gpio2_pressed = !gpio_get(2);    // GPIO 2 -> HAT UP
    bool gpio7_pressed = !gpio_get(7);    // GPIO 7 -> HAT UP (independent of strum)
    bool gpio3_pressed = !gpio_get(3);    // GPIO 3 -> HAT DOWN  
    bool gpio8_pressed = !gpio_get(8);    // GPIO 8 -> HAT DOWN (independent of strum)
    
    dpad_up = !gpio_get(device_config.button_pins.dpad_up);
    dpad_down = !gpio_get(device_config.button_pins.dpad_down);
    dpad_left = !gpio_get(device_config.button_pins.dpad_left);
    dpad_right = !gpio_get(device_config.button_pins.dpad_right);

    // Map Guitar Hero controls to HID gamepad - ZERO INITIALIZATION
    memset(&hid_report, 0, sizeof(hid_report));
    
    // Fret buttons -> HID buttons (bit positions)
    if (green)  hid_report.buttons |= (1 << 0);   // Button 1
    if (red)    hid_report.buttons |= (1 << 1);   // Button 2
    if (yellow) hid_report.buttons |= (1 << 2);   // Button 3
    if (blue)   hid_report.buttons |= (1 << 3);   // Button 4
    if (orange) hid_report.buttons |= (1 << 4);   // Button 5
    
    // Strum bar -> HID buttons
    if (strum_up)   hid_report.buttons |= (1 << 5);  // Button 6
    if (strum_down) hid_report.buttons |= (1 << 6);  // Button 7
    
    // Control buttons -> HID buttons  
    if (start)  hid_report.buttons |= (1 << 7);   // Button 8
    if (select) hid_report.buttons |= (1 << 8);   // Button 9 (Star Power)
    if (guide)  hid_report.buttons |= (1 << 9);   // Button 10 (Menu/Guide)
    
    // Multiple GPIO inputs -> Same HAT directions (completely independent)
    uint8_t hat = 0;  // Center (no direction)
    if (gpio2_pressed || gpio7_pressed || dpad_up) {
        if (dpad_left) hat = 7;          // NW
        else if (dpad_right) hat = 1;    // NE 
        else hat = 0;                    // N
    } else if (gpio3_pressed || gpio8_pressed || dpad_down) {
        if (dpad_left) hat = 5;          // SW
        else if (dpad_right) hat = 3;    // SE
        else hat = 4;                    // S
    } else if (dpad_left) {
        hat = 6;                         // W
    } else if (dpad_right) {
        hat = 2;                         // E
    }
    hid_report.hat = hat;

    // Read analog inputs (ADC)
    // GPIO to ADC mapping: GPIO26=ADC0, GPIO27=ADC1, GPIO28=ADC2, GPIO29=ADC3
    // Use config value for whammy pin - convert GPIO to ADC channel
    uint8_t whammy_adc_channel = device_config.whammy_pin - 26; // GPIO27 -> ADC1
    adc_select_input(whammy_adc_channel);
    uint16_t whammy_raw = adc_read();
    
    // Standard Guitar Hero whammy mapping - 8-bit value (0-255)
    whammy_value = (uint16_t)((whammy_raw * 255UL) / 4095UL);
    
    // Standard Guitar Hero controller mapping - Map to TinyUSB gamepad fields
    // Map whammy to Z axis and tilt to X axis
    hid_report.z = (uint8_t)whammy_value;                    // Whammy bar -> Z axis
    hid_report.x = (int8_t)(tilt_active ? -127 : 0);        // Tilt -> X axis (simple mapping)
    hid_report.y = (int8_t)(0);                             // Y axis unused for now
    
    // Set global variables for interface compatibility
    tilt_x = hid_report.x * 256;  // Scale to 16-bit for compatibility
    tilt_y = hid_report.y * 256;
}

//--------------------------------------------------------------------+
// MAIN FUNCTIONS
//--------------------------------------------------------------------+
int main(void) {
    board_init();

    // Initialize configuration system
    config_init();

    // Initialize built-in LED for status indication
    gpio_init(25);  // Built-in LED (PIN 25)
    gpio_set_dir(25, GPIO_OUT);
    
    // Flash LED to indicate startup
    for (int i = 0; i < 5; i++) {
        gpio_put(25, 1);  // Built-in LED
        sleep_ms(100);
        gpio_put(25, 0);
        sleep_ms(100);
    }

    // Initialize GPIO pins with pull-ups using config values
    // Fret buttons
    gpio_init(device_config.button_pins.green);
    gpio_set_dir(device_config.button_pins.green, GPIO_IN);
    gpio_pull_up(device_config.button_pins.green);

    gpio_init(device_config.button_pins.red);
    gpio_set_dir(device_config.button_pins.red, GPIO_IN);
    gpio_pull_up(device_config.button_pins.red);

    gpio_init(device_config.button_pins.yellow);
    gpio_set_dir(device_config.button_pins.yellow, GPIO_IN);
    gpio_pull_up(device_config.button_pins.yellow);

    gpio_init(device_config.button_pins.blue);
    gpio_set_dir(device_config.button_pins.blue, GPIO_IN);
    gpio_pull_up(device_config.button_pins.blue);

    gpio_init(device_config.button_pins.orange);
    gpio_set_dir(device_config.button_pins.orange, GPIO_IN);
    gpio_pull_up(device_config.button_pins.orange);

    // Strum bar
    gpio_init(device_config.button_pins.strum_up);
    gpio_set_dir(device_config.button_pins.strum_up, GPIO_IN);
    gpio_pull_up(device_config.button_pins.strum_up);

    gpio_init(device_config.button_pins.strum_down);
    gpio_set_dir(device_config.button_pins.strum_down, GPIO_IN);
    gpio_pull_up(device_config.button_pins.strum_down);

    // Control buttons
    gpio_init(device_config.button_pins.start);
    gpio_set_dir(device_config.button_pins.start, GPIO_IN);
    gpio_pull_up(device_config.button_pins.start);

    gpio_init(device_config.button_pins.select);
    gpio_set_dir(device_config.button_pins.select, GPIO_IN);
    gpio_pull_up(device_config.button_pins.select);

    // D-Pad buttons
    gpio_init(device_config.button_pins.dpad_up);
    gpio_set_dir(device_config.button_pins.dpad_up, GPIO_IN);
    gpio_pull_up(device_config.button_pins.dpad_up);

    gpio_init(device_config.button_pins.dpad_down);
    gpio_set_dir(device_config.button_pins.dpad_down, GPIO_IN);
    gpio_pull_up(device_config.button_pins.dpad_down);

    gpio_init(device_config.button_pins.dpad_left);
    gpio_set_dir(device_config.button_pins.dpad_left, GPIO_IN);
    gpio_pull_up(device_config.button_pins.dpad_left);

    gpio_init(device_config.button_pins.dpad_right);
    gpio_set_dir(device_config.button_pins.dpad_right, GPIO_IN);
    gpio_pull_up(device_config.button_pins.dpad_right);

    // Initialize ADC for whammy bar
    adc_init();
    adc_gpio_init(device_config.whammy_pin);  // Configure as ADC input

    // Detect boot combo for initial USB mode (HID firmware always uses HID)
    current_usb_mode = detect_boot_combo();

    // Initialize HID report structure
    memset(&hid_report, 0, sizeof(hid_report));

    // Initialize TinyUSB
    tusb_init();
    
    // Initialize stdio for debug output (this enables serial console)
    stdio_init_all();
    sleep_ms(2000);  // Give time for USB to enumerate before sending debug output
    
    // Flash LED pattern to indicate HID firmware mode
    // HID mode: 3 short flashes (standard gamepad)
    for (int i = 0; i < 3; i++) {
        gpio_put(25, 1);  // Built-in LED
        sleep_ms(200);    // Short flash
        gpio_put(25, 0);
        sleep_ms(200);
    }
    
    printf("BGG Guitar Hero Controller - HID Firmware\\n");
    printf("Boot combos: Green=XInput preference, Red=HID preference\\n");
    printf("Running in HID mode\\n");

    while (1) {
        // TinyUSB device task
        tud_task();
        
        // Read guitar buttons and controls
        read_guitar_buttons();

        // Send HID reports at 125Hz (stable rate)
        static uint32_t last_report_time = 0;
        uint32_t current_time = board_millis();
        if (tud_hid_ready() && (current_time - last_report_time >= 8)) {  // 125Hz - absolutely stable rate
            // Send HID gamepad report
            bool sent = tud_hid_report(0, &hid_report, sizeof(hid_report));
            if (sent) {
                last_report_time = current_time;
            }
        }
    }

    return 0;
}
