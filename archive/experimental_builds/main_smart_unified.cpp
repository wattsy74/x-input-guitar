#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "pico/stdio.h"
#include "bsp/board.h"
#include "config.h"
#include "tusb.h"
#include <string.h>

//--------------------------------------------------------------------+
// PERSISTENT MODE DETECTION (from bootloader)
//--------------------------------------------------------------------+

// Mode storage definitions (must match bootloader)
#define MODE_STORAGE_REG    0  // Use scratch register 0
#define MODE_MAGIC          0xBEEF0000
#define MODE_XINPUT         (MODE_MAGIC | 0x0001)
#define MODE_HID            (MODE_MAGIC | 0x0002)
#define MODE_USB_STORAGE    (MODE_MAGIC | 0x0003)

// Global mode selection - set BEFORE any TinyUSB initialization
static bool g_use_hid_mode = false;

// Check persistent mode flag from bootloader
bool detect_persistent_mode(void) {
    uint32_t stored_mode = watchdog_hw->scratch[MODE_STORAGE_REG];
    
    printf("BOOT: Checking persistent mode flag: 0x%08x\n", stored_mode);
    
    // Clear the flag so we don't use it again on next boot
    watchdog_hw->scratch[MODE_STORAGE_REG] = 0;
    
    if (stored_mode == MODE_HID) {
        printf("BOOT: Persistent HID mode flag detected\n");
        return true;  // HID mode
    } else if (stored_mode == MODE_XINPUT) {
        printf("BOOT: Persistent XInput mode flag detected\n");
        return false; // XInput mode
    } else {
        printf("BOOT: No valid persistent mode - XInput default\n");
        return false; // XInput mode (default)
    }
}

//--------------------------------------------------------------------+
// USB DESCRIPTORS AND CALLBACKS
//--------------------------------------------------------------------+

#if defined(USE_HID_MODE)
//--------------------------------------------------------------------+
// HID MODE CONFIGURATION
//--------------------------------------------------------------------+

#include "class/hid/hid_device.h"

// HID gamepad report
static hid_gamepad_report_t hid_report = {0};

// HID Report Descriptor
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

// Device Descriptor for HID mode
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x1234,
    .idProduct          = 0x5678,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration Descriptor for HID mode
enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL };
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID   0x81

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), EPNUM_HID, CFG_TUD_HID_EP_BUFSIZE, 8)
};

// HID Callbacks
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

#else
//--------------------------------------------------------------------+
// XINPUT MODE CONFIGURATION
//--------------------------------------------------------------------+

// XInput report structure
typedef struct {
    uint8_t report_id;
    uint8_t report_size;
    uint16_t buttons;
    uint8_t left_trigger;
    uint8_t right_trigger;
    int16_t left_thumb_x;
    int16_t left_thumb_y;
    int16_t right_thumb_x;
    int16_t right_thumb_y;
    uint8_t reserved[6];
} __attribute__((packed)) xinput_report_t;

static xinput_report_t xinput_report = {0};

// Device Descriptor for XInput mode
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0xFF,
    .bDeviceSubClass    = 0xFF,
    .bDeviceProtocol    = 0xFF,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x045E,  // Microsoft
    .idProduct          = 0x028E,  // Xbox 360 Controller
    .bcdDevice          = 0x0114,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration Descriptor for XInput mode
enum { ITF_NUM_VENDOR = 0, ITF_NUM_TOTAL };
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)
#define EPNUM_VENDOR_IN   0x81
#define EPNUM_VENDOR_OUT  0x01

uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 500),
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 0, EPNUM_VENDOR_OUT, EPNUM_VENDOR_IN, 64)
};

// Vendor Callbacks
void tud_vendor_rx_cb(uint8_t itf) {
    (void) itf;
}

#endif

//--------------------------------------------------------------------+
// COMMON USB CALLBACKS
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR
uint8_t const * tud_descriptor_device_cb(void) {
    return (uint8_t const *) &desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

// String Descriptors
char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },
    "BGG",
    "Guitar Hero Controller",
    "123456",
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))) return NULL;
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
// DEVICE CALLBACKS
//--------------------------------------------------------------------+

void tud_mount_cb(void) {
    gpio_put(25, 1);
    printf("USB mounted in %s mode\n", g_use_hid_mode ? "HID" : "XInput");
}

void tud_umount_cb(void) {
    gpio_put(25, 0);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    gpio_put(25, 0);
}

void tud_resume_cb(void) {
    gpio_put(25, 1);
}

//--------------------------------------------------------------------+
// GUITAR CONTROLS
//--------------------------------------------------------------------+

// Global button state variables
static bool green, red, yellow, blue, orange;
static bool strum_up, strum_down, start, select, guide;
static bool dpad_up, dpad_down, dpad_left, dpad_right;
static uint16_t whammy_value;

void read_guitar_buttons(void) {
    // Read digital button states (active LOW with internal pull-ups)
    green = !gpio_get(device_config.button_pins.green);
    red = !gpio_get(device_config.button_pins.red);
    yellow = !gpio_get(device_config.button_pins.yellow);
    blue = !gpio_get(device_config.button_pins.blue);
    orange = !gpio_get(device_config.button_pins.orange);
    strum_up = !gpio_get(device_config.button_pins.strum_up);
    strum_down = !gpio_get(device_config.button_pins.strum_down);
    start = !gpio_get(device_config.button_pins.start);
    select = !gpio_get(device_config.button_pins.select);
    
    // Initialize extra GPIO pins
    static bool extra_gpio_initialized = false;
    if (!extra_gpio_initialized) {
        gpio_init(9); gpio_set_dir(9, GPIO_IN); gpio_pull_up(9);  // Tilt
        gpio_init(6); gpio_set_dir(6, GPIO_IN); gpio_pull_up(6);  // Guide
        gpio_init(7); gpio_set_dir(7, GPIO_IN); gpio_pull_up(7);  // D-Pad
        gpio_init(8); gpio_set_dir(8, GPIO_IN); gpio_pull_up(8);  // D-Pad
        extra_gpio_initialized = true;
    }
    
    // Read extra inputs
    bool tilt_active = !gpio_get(9);
    guide = !gpio_get(6);
    bool gpio2_pressed = !gpio_get(2);
    bool gpio7_pressed = !gpio_get(7);
    bool gpio3_pressed = !gpio_get(3);
    bool gpio8_pressed = !gpio_get(8);
    
    dpad_up = !gpio_get(device_config.button_pins.dpad_up);
    dpad_down = !gpio_get(device_config.button_pins.dpad_down);
    dpad_left = !gpio_get(device_config.button_pins.dpad_left);
    dpad_right = !gpio_get(device_config.button_pins.dpad_right);

    // Read analog inputs (ADC)
    uint8_t whammy_adc_channel = device_config.whammy_pin - 26;
    adc_select_input(whammy_adc_channel);
    uint16_t whammy_raw = adc_read();
    whammy_value = (uint16_t)((whammy_raw * 255UL) / 4095UL);
    
    // Generate reports based on current mode
    if (g_use_hid_mode) {
#if defined(USE_HID_MODE)
        // Generate HID report
        memset(&hid_report, 0, sizeof(hid_report));
        
        if (green)  hid_report.buttons |= (1 << 0);
        if (red)    hid_report.buttons |= (1 << 1);
        if (yellow) hid_report.buttons |= (1 << 2);
        if (blue)   hid_report.buttons |= (1 << 3);
        if (orange) hid_report.buttons |= (1 << 4);
        if (strum_up)   hid_report.buttons |= (1 << 5);
        if (strum_down) hid_report.buttons |= (1 << 6);
        if (start)  hid_report.buttons |= (1 << 7);
        if (select) hid_report.buttons |= (1 << 8);
        if (guide)  hid_report.buttons |= (1 << 9);
        
        // HAT switch for D-Pad
        uint8_t hat = 0;
        if (gpio2_pressed || gpio7_pressed || dpad_up) {
            if (dpad_left) hat = 7;
            else if (dpad_right) hat = 1;
            else hat = 0;
        } else if (gpio3_pressed || gpio8_pressed || dpad_down) {
            if (dpad_left) hat = 5;
            else if (dpad_right) hat = 3;
            else hat = 4;
        } else if (dpad_left) {
            hat = 6;
        } else if (dpad_right) {
            hat = 2;
        }
        hid_report.hat = hat;
        
        hid_report.z = (uint8_t)whammy_value;
        hid_report.x = (int8_t)(tilt_active ? -127 : 0);
        hid_report.y = 0;
#endif
    } else {
#if !defined(USE_HID_MODE)
        // Generate XInput report
        memset(&xinput_report, 0, sizeof(xinput_report));
        xinput_report.report_id = 0x00;
        xinput_report.report_size = 0x14;
        
        if (green)      xinput_report.buttons |= 0x1000;  // Y
        if (red)        xinput_report.buttons |= 0x2000;  // X  
        if (yellow)     xinput_report.buttons |= 0x8000;  // A
        if (blue)       xinput_report.buttons |= 0x4000;  // B
        if (orange)     xinput_report.buttons |= 0x0020;  // LB
        if (strum_up)   xinput_report.buttons |= 0x0001;  // D-up
        if (strum_down) xinput_report.buttons |= 0x0002;  // D-down
        if (start)      xinput_report.buttons |= 0x0010;  // Start
        if (select)     xinput_report.buttons |= 0x0008;  // Back
        if (guide)      xinput_report.buttons |= 0x0400;  // Guide
        
        if (gpio2_pressed || gpio7_pressed || dpad_up)    xinput_report.buttons |= 0x0001;
        if (gpio3_pressed || gpio8_pressed || dpad_down)  xinput_report.buttons |= 0x0002;
        if (dpad_left)  xinput_report.buttons |= 0x0004;
        if (dpad_right) xinput_report.buttons |= 0x0008;
        
        xinput_report.right_thumb_x = (int16_t)(whammy_value * 256 - 32768);
        xinput_report.right_thumb_y = (int16_t)(tilt_active ? -32767 : 0);
#endif
    }
}

//--------------------------------------------------------------------+
// MAIN FUNCTION
//--------------------------------------------------------------------+

int main(void) {
    board_init();
    
    // Check persistent mode flag from bootloader
    g_use_hid_mode = detect_persistent_mode();
    
    config_init();

    // Initialize built-in LED
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    // Flash LED to indicate startup
    for (int i = 0; i < 5; i++) {
        gpio_put(25, 1);
        sleep_ms(100);
        gpio_put(25, 0);
        sleep_ms(100);
    }

    // CRITICAL: Detect boot combo BEFORE TinyUSB initialization
    g_use_hid_mode = detect_persistent_mode();
    
    // Initialize all GPIO pins (the ones not done in early detection)
    gpio_init(device_config.button_pins.yellow);
    gpio_set_dir(device_config.button_pins.yellow, GPIO_IN);
    gpio_pull_up(device_config.button_pins.yellow);

    gpio_init(device_config.button_pins.blue);
    gpio_set_dir(device_config.button_pins.blue, GPIO_IN);
    gpio_pull_up(device_config.button_pins.blue);

    gpio_init(device_config.button_pins.orange);
    gpio_set_dir(device_config.button_pins.orange, GPIO_IN);
    gpio_pull_up(device_config.button_pins.orange);

    gpio_init(device_config.button_pins.strum_up);
    gpio_set_dir(device_config.button_pins.strum_up, GPIO_IN);
    gpio_pull_up(device_config.button_pins.strum_up);

    gpio_init(device_config.button_pins.strum_down);
    gpio_set_dir(device_config.button_pins.strum_down, GPIO_IN);
    gpio_pull_up(device_config.button_pins.strum_down);

    gpio_init(device_config.button_pins.start);
    gpio_set_dir(device_config.button_pins.start, GPIO_IN);
    gpio_pull_up(device_config.button_pins.start);

    gpio_init(device_config.button_pins.select);
    gpio_set_dir(device_config.button_pins.select, GPIO_IN);
    gpio_pull_up(device_config.button_pins.select);

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

    // Initialize ADC
    adc_init();
    adc_gpio_init(device_config.whammy_pin);
    
    // Initialize TinyUSB with selected mode
    tusb_init();
    
    // Initialize stdio
    stdio_init_all();
    sleep_ms(2000);
    
    // Flash LED pattern to indicate mode
    if (g_use_hid_mode) {
        // HID mode: 3 short flashes
        for (int i = 0; i < 3; i++) {
            gpio_put(25, 1); sleep_ms(200);
            gpio_put(25, 0); sleep_ms(200);
        }
        printf("BGG Guitar Hero Controller - Unified Firmware\n");
        printf("Running in HID mode\n");
    } else {
        // XInput mode: 2 long flashes
        for (int i = 0; i < 2; i++) {
            gpio_put(25, 1); sleep_ms(500);
            gpio_put(25, 0); sleep_ms(300);
        }
        printf("BGG Guitar Hero Controller - Unified Firmware\n");
        printf("Running in XInput mode\n");
    }

    while (1) {
        tud_task();
        read_guitar_buttons();

        // Send reports at 125Hz
        static uint32_t last_report_time = 0;
        uint32_t current_time = board_millis();
        if (current_time - last_report_time >= 8) {
            
            if (g_use_hid_mode) {
#if defined(USE_HID_MODE)
                if (tud_hid_ready()) {
                    bool sent = tud_hid_report(0, &hid_report, sizeof(hid_report));
                    if (sent) last_report_time = current_time;
                }
#endif
            } else {
#if !defined(USE_HID_MODE)
                if (tud_vendor_mounted() && tud_vendor_write_available()) {
                    tud_vendor_write(&xinput_report, sizeof(xinput_report));
                    tud_vendor_write_flush();
                    last_report_time = current_time;
                }
#endif
            }
        }
    }

    return 0;
}
