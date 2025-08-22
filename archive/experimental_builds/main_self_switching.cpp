/*
 * Self-switching firmware that can switch between XInput and HID modes
 * by storing mode preference in flash and rebooting to apply changes.
 * 
 * This firmware is compiled in two versions:
 * - bgg_self_switching_xinput: XInput mode only
 * - bgg_self_switching_hid: HID mode only
 * 
 * The device stores mode preference in flash and can reboot to 
 * switch itself to the other mode when requested.
 */

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "config.h"
#include "tusb.h"
#include "bsp/board.h"
#include <stdio.h>
#include <string.h>

// Flash storage for mode preference (last sector of flash)
#define MODE_STORAGE_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define MODE_MAGIC 0xBEEF
#define MODE_XINPUT 0x01
#define MODE_HID 0x02

typedef struct {
    uint16_t magic;
    uint16_t mode;
    uint32_t checksum;
} mode_config_t;

// Current mode (determined at compile time)
#if defined(COMPILE_FOR_HID)
    static const uint16_t CURRENT_MODE = MODE_HID;
    static const char* MODE_NAME = "HID";
#else
    static const uint16_t CURRENT_MODE = MODE_XINPUT;
    static const char* MODE_NAME = "XInput";
#endif

// Function declarations
uint16_t read_stored_mode(void);
void write_stored_mode(uint16_t mode);
bool check_mode_switch_request(void);
void init_firmware(void);

//--------------------------------------------------------------------+
// FLASH STORAGE FUNCTIONS
//--------------------------------------------------------------------+

uint16_t read_stored_mode(void) {
    const mode_config_t* config = (const mode_config_t*)(XIP_BASE + MODE_STORAGE_OFFSET);
    
    if (config->magic == MODE_MAGIC) {
        uint32_t checksum = config->magic + config->mode;
        if (config->checksum == checksum) {
            return config->mode;
        }
    }
    
    // Default to XInput if no valid config found
    return MODE_XINPUT;
}

void write_stored_mode(uint16_t mode) {
    mode_config_t config = {
        .magic = MODE_MAGIC,
        .mode = mode,
        .checksum = (uint32_t)(MODE_MAGIC + mode)
    };
    
    // Erase and write the flash sector
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(MODE_STORAGE_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(MODE_STORAGE_OFFSET, (const uint8_t*)&config, sizeof(config));
    restore_interrupts(interrupts);
}

bool check_mode_switch_request(void) {
    // Check if user is holding specific button combo at startup
    // For example: hold green + red + blue frets simultaneously
    
    gpio_init(device_config.button_pins.green);
    gpio_init(device_config.button_pins.red);
    gpio_init(device_config.button_pins.blue);
    
    gpio_set_dir(device_config.button_pins.green, GPIO_IN);
    gpio_set_dir(device_config.button_pins.red, GPIO_IN);
    gpio_set_dir(device_config.button_pins.blue, GPIO_IN);
    
    gpio_pull_up(device_config.button_pins.green);
    gpio_pull_up(device_config.button_pins.red);
    gpio_pull_up(device_config.button_pins.blue);
    
    sleep_ms(10); // Debounce
    
    bool green_pressed = !gpio_get(device_config.button_pins.green);
    bool red_pressed = !gpio_get(device_config.button_pins.red);
    bool blue_pressed = !gpio_get(device_config.button_pins.blue);
    
    return green_pressed && red_pressed && blue_pressed;
}

void init_firmware(void) {
    stdio_init_all();
    config_init();
    
    printf("Starting %s firmware\n", MODE_NAME);
    
    // Check if mode switch is requested
    if (check_mode_switch_request()) {
        printf("Mode switch requested!\n");
        
        uint16_t stored_mode = read_stored_mode();
        uint16_t new_mode = (CURRENT_MODE == MODE_XINPUT) ? MODE_HID : MODE_XINPUT;
        
        if (stored_mode != new_mode) {
            printf("Switching from %s to %s mode\n", 
                   MODE_NAME, 
                   (new_mode == MODE_HID) ? "HID" : "XInput");
            
            write_stored_mode(new_mode);
            printf("Mode saved! Rebooting...\n");
            sleep_ms(1000);
            
            // Reboot to apply new mode
            watchdog_enable(1, 1);
            while (1) tight_loop_contents();
        } else {
            printf("Already in requested mode\n");
        }
    }
    
    printf("Running in %s mode\n", MODE_NAME);
}

//--------------------------------------------------------------------+
// USB DESCRIPTORS AND CALLBACKS
//--------------------------------------------------------------------+

#if defined(COMPILE_FOR_HID)
//--------------------------------------------------------------------+
// HID MODE CONFIGURATION  
//--------------------------------------------------------------------+

// HID Report Descriptor
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

// Device Descriptor for HID mode
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x1209,  // Generic
    .idProduct          = 0x0001,  // Generic
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration Descriptor for HID mode
enum { ITF_NUM_HID = 0, ITF_NUM_TOTAL };
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID         0x81

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

// Invoked when received GET STRING DESCRIPTOR request
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
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);
    return _desc_str;
}

// Device callbacks  
void tud_mount_cb(void) {
    printf("Device mounted\n");
}

void tud_umount_cb(void) {
    printf("Device unmounted\n");
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
    printf("Device suspended\n");
}

void tud_resume_cb(void) {
    printf("Device resumed\n");
}

//--------------------------------------------------------------------+
// MAIN FUNCTION
//--------------------------------------------------------------------+

int main(void) {
    init_firmware();
    
    // Initialize TinyUSB
    board_init();
    tusb_init();
    
    printf("Self-switching %s gamepad ready\n", MODE_NAME);
    
    // Main loop
    while (1) {
        tud_task(); // TinyUSB device task
        
#if defined(COMPILE_FOR_HID)
        if (tud_hid_ready()) {
            // Send HID gamepad report
            uint8_t report = 0x00; // Your button state here
            tud_hid_report(0, &report, 1);
        }
#else
        // XInput report sending
        if (tud_vendor_mounted() && tud_vendor_write_available()) {
            // Send XInput report
            // tud_vendor_write(&xinput_report, sizeof(xinput_report));
            // tud_vendor_write_flush();
        }
#endif
        
        sleep_ms(1);
    }
    
    return 0;
}
