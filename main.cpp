#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/pio.h"
#include "pico/stdio.h"
#include "bsp/board.h"
#include "config.h"
#include "config_storage.h"
#include "file_emulation.h"
#include "neopixel.h"
#include "ws2812.pio.h"
#include "tusb.h"
#include "device/usbd.h"
#include "class/hid/hid_device.h"
#include "class/vendor/vendor_device.h"
#include "class/cdc/cdc_device.h"
// // // #include "usb_interface.h"      // Disabled for now      // Disabled for now      // Disabled for now
// #include "hid_interface.h"      // Disabled for now  
// // #include "xinput_interface.h"   // Disabled for now   // Disabled for now
#include <string.h>

// Global variables
static config_t device_config;
static bool neopixel_initialized = false;

//--------------------------------------------------------------------+
// WORKING XINPUT + MINIMAL BOOT COMBO DETECTION
//--------------------------------------------------------------------+
// Based on the proven working simple implementation
// Adds only boot combo detection (Green=force XInput, Red=show message)

//--------------------------------------------------------------------+
// USB MODE SELECTION
//--------------------------------------------------------------------+
// USB mode enum (temporary definition until we re-enable interface system)
typedef enum {
    USB_MODE_XINPUT = 0,
    USB_MODE_HID = 1
} usb_mode_enum_t;

// Use the enum from usb_mode_storage.h to avoid conflicts
static usb_mode_enum_t current_usb_mode = USB_MODE_XINPUT;

// Forward declarations
usb_mode_enum_t detect_boot_combo(void);

// Simple flash storage for future use
#define USB_MODE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

//--------------------------------------------------------------------+
// XINPUT REPORT STRUCTURE (EXACT COPY FROM WORKING VERSION)
//--------------------------------------------------------------------+
typedef struct {
    uint16_t buttons;        // Digital buttons
    uint8_t lt;              // Left trigger
    uint8_t rt;              // Right trigger
    int16_t lx;              // Left stick X
    int16_t ly;              // Left stick Y
    int16_t rx;              // Right stick X
    int16_t ry;              // Right stick Y
    uint8_t reserved[6];     // Reserved
} __attribute__((packed)) xinput_report_t;

// XInput button bit masks (EXACT COPY FROM WORKING VERSION)
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

// USB constants
#define XINPUT_VID        0x045E  // Microsoft
#define XINPUT_PID        0x028E  // Xbox 360 Controller (wired)
#define XINPUT_BCD        0x0572

//--------------------------------------------------------------------+
// USB DESCRIPTORS (EXACT COPY FROM WORKING VERSION)
//--------------------------------------------------------------------+

// MINIMAL TEST: Basic vendor device that Windows should accept
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,  // USB 1.1 for maximum compatibility
    .bDeviceClass       = 0x00,    // Interface-specific
    .bDeviceSubClass    = 0x00,    
    .bDeviceProtocol    = 0x00,    
    .bMaxPacketSize0    = 64,      // Standard size
    .idVendor           = 0x1234,  // Test vendor ID (not Microsoft)
    .idProduct          = 0x5678,  // Test product ID  
    .bcdDevice          = 0x0100,  // Version 1.0
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = 3,
    .bNumConfigurations = 1
};

// Configuration Descriptor with dual interfaces for testing
enum {
    ITF_NUM_VENDOR = 0,
    ITF_NUM_CDC = 1,
    ITF_NUM_CDC_DATA = 2,
    ITF_NUM_TOTAL
};

#define EPNUM_VENDOR_IN   0x81
#define EPNUM_VENDOR_OUT  0x01
#define EPNUM_CDC_NOTIF   0x82
#define EPNUM_CDC_IN      0x83
#define EPNUM_CDC_OUT     0x03

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + 9 + 17 + 7 + 7 + TUD_CDC_DESC_LEN)

uint8_t const desc_configuration[] = {
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
    0x00, 0x00, 0x13, 0x01, 0x08, 0x00, 0x00,

    // Endpoint Descriptor (IN)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    EPNUM_VENDOR_IN,        // bEndpointAddress (IN, endpoint 1)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes for XInput)
    0x01,        // bInterval (1ms)

    // Endpoint Descriptor (OUT)
    0x07,        // bLength
    0x05,        // bDescriptorType (Endpoint)
    EPNUM_VENDOR_OUT,        // bEndpointAddress (OUT, endpoint 1)
    0x03,        // bmAttributes (Interrupt)
    0x20, 0x00,  // wMaxPacketSize (32 bytes for XInput)
    0x08,        // bInterval (8ms)

    // CDC Interface (Control and Data) - temporary for enumeration test
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64)
};

// String Descriptors  
char const* string_desc_arr[] = {
    (const char[]){0x09, 0x04}, // 0: Language (English)
    "Microsoft",                // 1: Manufacturer  
    "Controller (XBOX 360 For Windows)", // 2: Product
    "1234567890ABCDEF",        // 3: Serial number
    "BGG Test Port",           // 4: CDC Interface
};

//--------------------------------------------------------------------+
// USB MODE STORAGE (PLACEHOLDER FOR FUTURE)
//--------------------------------------------------------------------+
void usb_mode_save(usb_mode_enum_t mode) {
    // Future implementation
    (void)mode;
}

usb_mode_enum_t usb_mode_load(void) {
    // For now, use boot combo detection - later can add flash storage
    return detect_boot_combo();
}

//--------------------------------------------------------------------+
// BOOT COMBO DETECTION
//--------------------------------------------------------------------+
usb_mode_enum_t detect_boot_combo(void) {
    // Check if Green button is pressed at boot for XInput mode (already default)
    if (!gpio_get(config_get_green_pin())) {
        printf("BOOT COMBO: Green button detected - XInput mode selected\n");
        return USB_MODE_XINPUT;
    }
    
    // Check if Red button is pressed at boot for HID mode
    if (!gpio_get(config_get_red_pin())) {
        printf("BOOT COMBO: Red button detected - HID mode selected\n");
        return USB_MODE_HID;
    }
    
    // No combo detected, use default
    printf("BOOT COMBO: No combo detected - using default XInput mode\n");
    return USB_MODE_XINPUT;
}

//--------------------------------------------------------------------+
// DEVICE CALLBACKS (EXACT COPY FROM WORKING VERSION)
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
// USB DESCRIPTOR CALLBACKS (EXACT COPY FROM WORKING VERSION)
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index; // for multiple configurations
    return desc_configuration;
}

// Invoked when received GET STRING DESCRIPTOR request
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    
    static uint16_t _desc_str[32];
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else if (index == 0xEE) {
        // Microsoft OS String Descriptor - CRITICAL for XInput recognition!
        static const char msft_os_desc[] = "MSFT100\x01"; // Microsoft OS 1.0, vendor code 0x01
        chr_count = strlen(msft_os_desc);
        if (chr_count > 31) chr_count = 31;
        
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = msft_os_desc[i];
        }
    } else {
        // Regular string descriptors
        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

        const char* str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

    return _desc_str;
}

//--------------------------------------------------------------------+
// VENDOR INTERFACE CALLBACKS (XINPUT PROTOCOL) - EXACT FROM WORKING VERSION
//--------------------------------------------------------------------+

// Invoked when received control transfer with vendor request type
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    if (stage != CONTROL_STAGE_SETUP) return true;

    // Handle XInput-specific control requests
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR) {
        switch (request->bRequest) {
            case 0x01: // Get capabilities
                if (request->wValue == 0x0100) {
                    // XInput gamepad capabilities (20 bytes)
                    static const uint8_t xinput_caps[] = {
                        0x00, 0x14,       // Length: 20 bytes
                        0x00, 0x01,       // Type: 1 (gamepad)
                        0x00, 0x01,       // Subtype: 1 (standard gamepad)
                        0xFF, 0xFF,       // Buttons: all supported
                        0x00, 0x00,       // Left/Right triggers
                        0x01, 0x01,       // Left stick X/Y
                        0x01, 0x01,       // Right stick X/Y  
                        0x00, 0x00,       // Reserved
                        0x00, 0x00,       // Reserved
                        0x00, 0x00        // Reserved
                    };
                    return tud_control_xfer(rhport, request, (void*)xinput_caps, sizeof(xinput_caps));
                }
                break;

            case 0x02: // Get LED state  
                {
                    static const uint8_t led_state[] = { 0x00, 0x00, 0x00 };
                    return tud_control_xfer(rhport, request, (void*)led_state, sizeof(led_state));
                }

            case 0x03: // Set LED state
                return tud_control_status(rhport, request);

            default:
                break;
        }
    }

    // Handle Microsoft OS Descriptor requests (CRITICAL for XInput!)
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR && 
        request->bRequest == 0x01 && // Microsoft OS descriptor vendor request
        request->wIndex == 0x0004) { // OS Feature descriptor
        
        // Microsoft OS Compatible ID Feature Descriptor
        static const uint8_t ms_os_desc[] = {
            0x28, 0x00, 0x00, 0x00, // dwLength (40 bytes)
            0x00, 0x01,             // bcdVersion (1.0)
            0x04, 0x00,             // wIndex (4 = Compatible ID)
            0x01,                   // bCount (1 interface)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Reserved
            
            // Interface 0
            0x00,                   // bInterfaceNumber
            0x01,                   // Reserved
            'X','I','N','P','U','T',0x00,0x00, // compatibleID "XINPUT"
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // subCompatibleID
            0x00,0x00,0x00,0x00,0x00,0x00  // Reserved
        };
        
        return tud_control_xfer(rhport, request, (void*)ms_os_desc, sizeof(ms_os_desc));
    }

    return false; // Stall unknown requests
}

// Invoked when received data from host
void tud_vendor_rx_cb(uint8_t itf) {
    (void) itf;
    
    uint8_t buf[64];
    uint32_t count = tud_vendor_read(buf, sizeof(buf));
    
    // Handle rumble/LED commands here if needed
    (void) count;
}

//--------------------------------------------------------------------+
// CDC (Serial) CALLBACKS for BGG App Compatibility
//--------------------------------------------------------------------+
static char cdc_line_buffer[256];
static uint32_t cdc_line_pos = 0;

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void) itf;
    (void) rts;
    
    // DTR = true means terminal connected
    if (dtr) {
        printf("CDC: Terminal connected\n");
        // Send welcome message
        tud_cdc_write_str("BGG XInput Firmware v1.0 Ready\n");
        tud_cdc_write_flush();
    } else {
        printf("CDC: Terminal disconnected\n");
    }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf) {
    (void) itf;
    
    char buf[64];
    uint32_t count = tud_cdc_read(buf, sizeof(buf) - 1);
    
    if (count > 0) {
        buf[count] = '\0';
        
        // Process character by character to handle line buffering
        for (uint32_t i = 0; i < count; i++) {
            char c = buf[i];
            
            if (c == '\n' || c == '\r') {
                // End of line - process command
                if (cdc_line_pos > 0) {
                    cdc_line_buffer[cdc_line_pos] = '\0';
                    printf("CDC: Received command: %s\n", cdc_line_buffer);
                    
                    // Process the command through file emulation
                    file_emu_process_serial_command(cdc_line_buffer);
                    
                    // Reset line buffer
                    cdc_line_pos = 0;
                }
            } else if (cdc_line_pos < sizeof(cdc_line_buffer) - 1) {
                // Add character to line buffer
                cdc_line_buffer[cdc_line_pos++] = c;
            }
        }
    }
}

//--------------------------------------------------------------------+
// GLOBAL VARIABLES
//--------------------------------------------------------------------+
static xinput_report_t xinput_report;  // Remove volatile, try different approach

// Global button state variables for USB interface system
static bool green, red, yellow, blue, orange;
static bool strum_up, strum_down, start, select, guide;
static bool dpad_up, dpad_down, dpad_left, dpad_right;
static uint16_t whammy_value;
static int16_t tilt_x, tilt_y;

//--------------------------------------------------------------------+
// GUITAR HERO BUTTON MAPPING (EXACT FROM WORKING VERSION)
//--------------------------------------------------------------------+
void read_guitar_buttons(void) {
    // Debug counter for random guide button triggers
    static uint32_t guide_trigger_count = 0;
    static uint32_t last_count_report = 0;
    uint32_t now = time_us_32() / 1000; // Convert to milliseconds
    
    // Read digital button states (active LOW with internal pull-ups) using config values
    green = !gpio_get(config_get_green_pin());
    red = !gpio_get(config_get_red_pin());
    yellow = !gpio_get(config_get_yellow_pin());
    blue = !gpio_get(config_get_blue_pin());
    orange = !gpio_get(config_get_orange_pin());
    strum_up = !gpio_get(config_get_strum_up_pin());
    strum_down = !gpio_get(config_get_strum_down_pin());
    start = !gpio_get(config_get_start_pin());
    select = !gpio_get(config_get_select_pin());
    
    // Initialize GPIO pins for tilt sensor and guide button separately
    static bool extra_gpio_initialized = false;
    if (!extra_gpio_initialized) {
        // Tilt sensor on GPIO 9 - combines with Select for Star Power
        gpio_init(9);
        gpio_set_dir(9, GPIO_IN);
        gpio_pull_up(9);
        
        // Guide button on GPIO 6 - brings up Xbox guide menu
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
            tilt_debounce_time = now;
            printf("Tilt sensor %s\n", tilt_active ? "ACTIVE" : "INACTIVE");
        }
    }
    
    // Guide button (GPIO 6) - Back to original simple version that was working
    guide = !gpio_get(6);
    
    // Read all D-Pad inputs independently - no conflicts, just multiple inputs -> same output
    bool gpio2_pressed = !gpio_get(2);    // GPIO 2 -> HAT UP
    bool gpio7_pressed = !gpio_get(7);    // GPIO 7 -> HAT UP (independent of strum)
    bool gpio3_pressed = !gpio_get(3);    // GPIO 3 -> HAT DOWN  
    bool gpio8_pressed = !gpio_get(8);    // GPIO 8 -> HAT DOWN (independent of strum)
    
    dpad_up = !gpio_get(config_get_dpad_up_pin());
    dpad_down = !gpio_get(config_get_dpad_down_pin());
    dpad_left = !gpio_get(config_get_dpad_left_pin());
    dpad_right = !gpio_get(config_get_dpad_right_pin());

    // Map Guitar Hero controls to XInput gamepad - COMPLETELY ZERO TO PREVENT GHOST SIGNALS
    // COMPLETELY zero out the entire structure to prevent any ghost Guide button signals
    memset(&xinput_report, 0, sizeof(xinput_report));
    xinput_report.buttons = 0;  // Explicitly clear buttons
    
    // Fret buttons -> Face buttons (EXACT MAPPING FROM WORKING VERSION)
    if (green) xinput_report.buttons |= XINPUT_A;        // Green -> A (should be Button 1)
    if (red) xinput_report.buttons |= XINPUT_B;          // Red -> B (should be Button 2) 
    if (yellow) xinput_report.buttons |= XINPUT_Y;       // Yellow -> Y (should be Button 4)
    if (blue) xinput_report.buttons |= XINPUT_X;         // Blue -> X (should be Button 3)
    if (orange) xinput_report.buttons |= XINPUT_LB;      // Orange -> LB (should be Button 5)
    
    // Strum bar -> D-Pad mapping for Clone Hero compatibility (EXACT FROM WORKING VERSION)
    if (strum_up) xinput_report.buttons |= XINPUT_DPAD_UP;      // Strum up -> D-Pad Up
    if (strum_down) xinput_report.buttons |= XINPUT_DPAD_DOWN;  // Strum down -> D-Pad Down
    
    // Control buttons
    if (start) xinput_report.buttons |= XINPUT_START;    // Start -> Start button
    
    // Star Power logic: Only Select button (tilt moved to Right Stick Y-axis)
    if (select) {
        xinput_report.buttons |= XINPUT_BACK;    // Select -> Back button (Star Power in games)
        // Debug removed for simplicity
    }
    
    if (guide) {
        xinput_report.buttons |= XINPUT_GUIDE;    // Guide button (GPIO 6) -> Xbox Guide menu
        guide_trigger_count++;  // Count every frame the guide is active
        
        // NO CONSTANT NEOPIXEL UPDATES - only flash on new presses to prevent flickering
    } else {
        // NO CONSTANT OFF UPDATES - keep LEDs stable
    }
    
    // DISABLED - No more NeoPixel count reporting to prevent LED corruption
    if (now - last_count_report > 5000) {  // Every 5 seconds
        last_count_report = now;
        guide_trigger_count = 0;  // Reset counter
    }
    
    // Multiple GPIO inputs -> Same HAT directions (completely independent)
    if (gpio2_pressed || gpio7_pressed) {
        xinput_report.buttons |= XINPUT_DPAD_UP;        // GPIO 2 OR GPIO 7 -> HAT UP ONLY
    }
    if (gpio3_pressed || gpio8_pressed) {
        xinput_report.buttons |= XINPUT_DPAD_DOWN;      // GPIO 3 OR GPIO 8 -> HAT DOWN ONLY
    }
    if (dpad_left) xinput_report.buttons |= XINPUT_DPAD_LEFT;             // GPIO 4 -> HAT LEFT
    if (dpad_right) xinput_report.buttons |= XINPUT_DPAD_RIGHT;           // GPIO 5 -> HAT RIGHT
    
    // Strum bar -> Shoulder buttons (REMOVED - no longer using GPIO 7/8 for strum)
    // if (strum_up) xinput_report.buttons |= XINPUT_RB;          // DISABLED
    // if (strum_down) xinput_report.buttons |= XINPUT_LSTICK;    // DISABLED

    // Read analog inputs (ADC) - EXACT FROM WORKING VERSION
    // GPIO to ADC mapping: GPIO26=ADC0, GPIO27=ADC1, GPIO28=ADC2, GPIO29=ADC3
    // Use config value for whammy pin - convert GPIO to ADC channel
    uint8_t whammy_adc_channel = config_get_whammy_pin() - 26; // GPIO27 -> ADC1
    adc_select_input(whammy_adc_channel);
    uint16_t whammy_raw = adc_read();
    
    // Standard Guitar Hero whammy mapping - 8-bit trigger (0-255) - EXACT FROM WORKING VERSION
    whammy_value = (uint16_t)((whammy_raw * 255UL) / 4095UL);
    
    adc_select_input(2); // GPIO 28 = ADC channel 2 (Joystick X)
    uint16_t joy_x_raw = adc_read();
    int16_t joy_x_value = (int16_t)((joy_x_raw - 2048) * 16); // Convert to signed 16-bit, centered
    
    adc_select_input(3); // GPIO 29 = ADC channel 3 (Joystick Y)
    uint16_t joy_y_raw = adc_read();
    int16_t joy_y_value = (int16_t)((joy_y_raw - 2048) * 16); // Convert to signed 16-bit, centered

    // Standard Guitar Hero controller mapping - UPDATED FOR TILT/WHAMMY ON RIGHT STICK
    xinput_report.lt = 0;                                 // Left trigger -> unused
    xinput_report.rt = 0;                                 // Right trigger -> unused (whammy moved to stick)
    xinput_report.lx = joy_x_value;                       // Left stick X -> Joystick X
    xinput_report.ly = joy_y_value;                       // Left stick Y -> Joystick Y  
    
    // Right stick mapping - NEW!
    // Whammy bar -> Right stick X-axis (0-255 mapped to -32768 to 32767)
    int16_t whammy_stick_value = (int16_t)((whammy_value * 65535UL) / 255UL) - 32768;
    xinput_report.rx = whammy_stick_value;                // Right stick X -> Whammy bar
    tilt_x = whammy_stick_value;                          // For USB interface system
    
    // Tilt sensor -> Right stick Y-axis (INVERTED: 0=on, 255=off)
    int16_t tilt_stick_value = tilt_active ? -32768 : 32767;
    xinput_report.ry = tilt_stick_value;                  // Right stick Y -> Tilt sensor (inverted: 0% when tilted, 100% when not tilted)
    tilt_y = tilt_stick_value;                            // For USB interface system
}

//--------------------------------------------------------------------+
// MAIN FUNCTIONS
//--------------------------------------------------------------------+
int main(void) {
    board_init();

    // Initialize configuration system
    config_init();
    
    // Initialize config storage system
    config_storage_init();
    
    // Load device configuration from flash
    if (!config_storage_load_from_flash(&device_config)) {
        printf("Warning: Failed to load config from flash, using defaults\n");
        // Config should still be valid from config_init()
    }
    
    // Initialize file emulation for BGG app compatibility
    file_emu_init();

    // MOVE NeoPixel initialization AFTER USB to prevent interference
    // neopixel_init(&device_config);
    // neopixel_initialized = true;
    // printf("NeoPixel initialized successfully\n");
    
    // MOVE NeoPixel test sequence AFTER USB to prevent interference
    // printf("Running NeoPixel test sequence...\n");
    // neopixel_test();
    // printf("NeoPixel test sequence complete\n");

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

    // Initialize GPIO pins with pull-ups using config values - EXACT FROM WORKING VERSION
    // Fret buttons
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

    // Strum bar
    gpio_init(config_get_strum_up_pin());
    gpio_set_dir(config_get_strum_up_pin(), GPIO_IN);
    gpio_pull_up(config_get_strum_up_pin());

    gpio_init(config_get_strum_down_pin());
    gpio_set_dir(config_get_strum_down_pin(), GPIO_IN);
    gpio_pull_up(config_get_strum_down_pin());

    // Control buttons
    gpio_init(config_get_start_pin());
    gpio_set_dir(config_get_start_pin(), GPIO_IN);
    gpio_pull_up(config_get_start_pin());

    gpio_init(config_get_select_pin());
    gpio_set_dir(config_get_select_pin(), GPIO_IN);
    gpio_pull_up(config_get_select_pin());

    // D-Pad
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

    // Initialize ADC for analog inputs using config values - EXACT FROM WORKING VERSION
    adc_init();
    adc_gpio_init(config_get_whammy_pin());   // Whammy bar
    adc_gpio_init(config_get_joystick_x_pin());    // Joystick X
    adc_gpio_init(config_get_joystick_y_pin());    // Joystick Y

    // Detect boot combo for initial USB mode
    current_usb_mode = detect_boot_combo();

    // TODO: Re-enable USB interface system when issues are fixed
    // printf("Initializing USB interface for mode: %s\n", 
    //        current_usb_mode == USB_MODE_HID ? "HID" : "XInput");
    // usb_interface_init((usb_mode_enum_t)current_usb_mode);

    // Initialize XInput report structure
    memset(&xinput_report, 0, sizeof(xinput_report));

    // Initialize TinyUSB
    tusb_init();
    
    // Initialize stdio for debug output (this enables serial console)
    stdio_init_all();
    sleep_ms(2000);  // Give time for USB to enumerate before sending debug output
    
    // NOW initialize NeoPixels AFTER USB to prevent PIO conflicts
    printf("Initializing NeoPixels after USB...\n");
    neopixel_init(&device_config);
    neopixel_initialized = true;
    printf("NeoPixel initialized successfully after USB\n");
    
    // Run NeoPixel test sequence
    printf("Running NeoPixel test sequence...\n");
    neopixel_test();
    printf("NeoPixel test sequence complete\n");
    
    // Initialize NeoPixels for debugging random menu popups - DISABLED to prevent white LED issues
    // neopixel_init();
    // neopixel_set_all(RGB_GREEN);    // Green = System Ready  
    // sleep_ms(1000);
    // neopixel_set_all(RGB_OFF);
    
    // Flash LED pattern to indicate boot combo detected
    if (current_usb_mode == USB_MODE_XINPUT) {
        // XInput mode: 2 long flashes (like Xbox button) 
        for (int i = 0; i < 2; i++) {
            gpio_put(25, 1);  // Built-in LED
            sleep_ms(500);    // Long flash
            gpio_put(25, 0);
            sleep_ms(500);
        }
    }

    printf("Guitar Hero Controller with Boot Combo Detection\n");
    printf("Current mode: XInput (Green button detected at boot forces XInput)\n");
    printf("Boot combos: Green=XInput, Red=Future HID mode\n");
    
    // Initialize NeoPixel system with proper config loading
    neopixel_init(&device_config);
    neopixel_initialized = true;
    printf("NeoPixel system initialized successfully\n");
    
    // Set startup colors to indicate successful boot
    neopixel_set_all(0xFF000000);  // White startup indicator  
    neopixel_show();
    sleep_ms(500);
    
    // Show RGB test pattern
    neopixel_set_all(0x00FF0000);  // Red
    neopixel_show();
    sleep_ms(300);
    neopixel_set_all(0x0000FF00);  // Green  
    neopixel_show();
    sleep_ms(300);
    neopixel_set_all(0x000000FF);  // Blue
    neopixel_show();
    sleep_ms(300);
    neopixel_set_all(0x00000000);  // Off
    neopixel_show();

    while (1) {
        // TinyUSB device task
        tud_task();
        
        // Read guitar buttons and controls
        read_guitar_buttons();

        // Update NeoPixel LEDs based on button states
        if (neopixel_initialized) {
            bool fret_states[5] = {green, red, yellow, blue, orange};
            neopixel_update_button_state(&device_config, fret_states, strum_up, strum_down);
        }

        // TODO: Re-enable USB interface system calls when ready
        // // Create button state for USB interface system
        // button_state_t button_state = {0};
        // // Map internal state to button_state structure
        // button_state.green = green;
        // // ... (mapping code) ...
        // // Send reports through USB interface system
        // const usb_interface_t* interface = usb_interface_get();
        // if (interface && interface->ready && interface->ready()) {
        //     if (interface->send_report) {
        //         interface->send_report(&button_state);
        //     }
        // }

        // XInput code (works for both modes until HID is fully implemented)
        // Send XInput reports regardless of mode for now - both work as XInput
        {
            // Send XInput report at 500Hz if device is ready
            static uint32_t last_report_time = 0;
            uint32_t current_time = board_millis();
            if (tud_vendor_mounted() && (current_time - last_report_time >= 8)) {  // 125Hz - absolutely stable rate
                // CRITICAL: Disable all interrupts during packet creation to prevent corruption
                uint32_t saved_interrupts = save_and_disable_interrupts();
                
                // Send XInput input report (20 bytes data + 2 byte header = 22 bytes total)
                uint8_t report_packet[22];
                memset(report_packet, 0, sizeof(report_packet));  // CRITICAL: Clear all garbage data
                
                report_packet[0] = 0x00; // Message type: input report
                report_packet[1] = 0x14; // Report size: 20 bytes
                
                // Copy the XInput report data (20 bytes) with interrupts disabled
                memcpy(&report_packet[2], (void*)&xinput_report, sizeof(xinput_report));
                
                // Re-enable interrupts before USB write
                restore_interrupts(saved_interrupts);

                if (tud_vendor_write_available() >= sizeof(report_packet)) {
                    tud_vendor_write(report_packet, sizeof(report_packet));
                    tud_vendor_write_flush();
                    last_report_time = current_time;
                }
            }
        }
        // TODO: Add HID mode support here when interface system is ready
    }

    return 0;
}
