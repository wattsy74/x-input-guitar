#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "tusb.h"
#include "src/config_manager.h"
#include "src/serial_api.h"
#include <string.h>

// HID gamepad report structure (standard USB gamepad)
typedef struct {
    uint8_t buttons1;      // Buttons 1-8
    uint8_t buttons2;      // Buttons 9-16  
    uint8_t hat;           // D-pad (hat switch)
    uint8_t x;             // Left stick X
    uint8_t y;             // Left stick Y
    uint8_t z;             // Right stick X
    uint8_t rz;            // Right stick Y
    uint8_t brake;         // Left trigger
    uint8_t accelerator;   // Right trigger
} __attribute__((packed)) hid_gamepad_report_t;

// Configuration
static bgg_config_t device_config;
static hid_gamepad_report_t gamepad_report = {0};

// Function prototypes
static void init_hardware(void);
static void read_inputs(void);
static void send_hid_report(void);
static void process_serial_commands(void);
static uint8_t calculate_hat_value(bool up, bool down, bool left, bool right);

int main() {
    // Initialize system
    stdio_init_all();
    
    // Load configuration
    if (!config_init() || !config_load(&device_config)) {
        // Config failed, use defaults
        device_config = DEFAULT_CONFIG;
        // Set to HID mode for this firmware
        strcpy(device_config.usb_mode, "hid");
        config_save(&device_config);
    }
    
    // Verify we're supposed to be in HID mode
    if (strcmp(device_config.usb_mode, "hid") != 0) {
        // Wrong mode, request switch and reboot
        config_request_mode_switch("hid");
        watchdog_reboot(0, 0, 0);
    }
    
    // Initialize hardware
    init_hardware();
    
    // Initialize serial API
    serial_api_init();
    
    // Initialize USB
    tusb_init();
    
    while (true) {
        // USB device task
        tud_task();
        
        // Process serial commands for Windows app integration
        process_serial_commands();
        
        // Read controller inputs
        read_inputs();
        
        // Send HID report
        if (tud_hid_ready()) {
            send_hid_report();
        }
        
        sleep_ms(1);  // 1000Hz update rate
    }
}

static void init_hardware(void) {
    // Initialize button pins
    const uint8_t button_pins[] = {
        device_config.UP, device_config.DOWN, device_config.LEFT, device_config.RIGHT,
        device_config.GREEN_FRET, device_config.RED_FRET, device_config.YELLOW_FRET,
        device_config.BLUE_FRET, device_config.ORANGE_FRET, device_config.STRUM_UP,
        device_config.STRUM_DOWN, device_config.TILT, device_config.SELECT,
        device_config.START, device_config.GUIDE
    };
    
    for (size_t i = 0; i < sizeof(button_pins); i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
    
    // Initialize ADC for whammy bar
    adc_init();
    adc_gpio_init(device_config.WHAMMY);
    
    // Initialize analog joystick if configured
    if (device_config.joystick_x_pin <= 29) {
        adc_gpio_init(device_config.joystick_x_pin);
    }
    if (device_config.joystick_y_pin <= 29) {
        adc_gpio_init(device_config.joystick_y_pin);
    }
    
    // TODO: Initialize NeoPixel LEDs
    // This would integrate with the BGG LED system
}

static void read_inputs(void) {
    // Clear previous state
    gamepad_report.buttons1 = 0;
    gamepad_report.buttons2 = 0;
    
    // Map guitar buttons to standard gamepad buttons
    if (!gpio_get(device_config.GREEN_FRET))  gamepad_report.buttons1 |= 0x01;  // Button 1
    if (!gpio_get(device_config.RED_FRET))    gamepad_report.buttons1 |= 0x02;  // Button 2
    if (!gpio_get(device_config.YELLOW_FRET)) gamepad_report.buttons1 |= 0x04;  // Button 3
    if (!gpio_get(device_config.BLUE_FRET))   gamepad_report.buttons1 |= 0x08;  // Button 4
    if (!gpio_get(device_config.ORANGE_FRET)) gamepad_report.buttons1 |= 0x10;  // Button 5
    
    if (!gpio_get(device_config.SELECT))      gamepad_report.buttons1 |= 0x20;  // Button 6
    if (!gpio_get(device_config.START))       gamepad_report.buttons1 |= 0x40;  // Button 7
    if (!gpio_get(device_config.GUIDE))       gamepad_report.buttons1 |= 0x80;  // Button 8
    
    // Strum as additional buttons
    if (!gpio_get(device_config.STRUM_UP))    gamepad_report.buttons2 |= 0x01;  // Button 9
    if (!gpio_get(device_config.STRUM_DOWN))  gamepad_report.buttons2 |= 0x02;  // Button 10
    if (!gpio_get(device_config.TILT))        gamepad_report.buttons2 |= 0x04;  // Button 11
    
    // D-pad using hat switch
    bool up = !gpio_get(device_config.UP);
    bool down = !gpio_get(device_config.DOWN);
    bool left = !gpio_get(device_config.LEFT);
    bool right = !gpio_get(device_config.RIGHT);
    gamepad_report.hat = calculate_hat_value(up, down, left, right);
    
    // Whammy bar as Y axis
    adc_select_input(device_config.WHAMMY - 26);  // Convert GPIO to ADC input
    uint16_t whammy_raw = adc_read();
    
    // Apply calibration and convert to 8-bit range
    int32_t whammy_calibrated = whammy_raw;
    if (whammy_calibrated < device_config.whammy_min) whammy_calibrated = device_config.whammy_min;
    if (whammy_calibrated > device_config.whammy_max) whammy_calibrated = device_config.whammy_max;
    
    int32_t whammy_range = device_config.whammy_max - device_config.whammy_min;
    int32_t whammy_scaled = ((whammy_calibrated - device_config.whammy_min) * 255) / whammy_range;
    
    if (device_config.whammy_reverse) {
        whammy_scaled = 255 - whammy_scaled;
    }
    
    gamepad_report.y = (uint8_t)whammy_scaled;
    
    // Default other axes to center
    gamepad_report.x = 127;
    gamepad_report.z = 127;
    gamepad_report.rz = 127;
    gamepad_report.brake = 0;
    gamepad_report.accelerator = 0;
    
    // TODO: Read analog joystick if configured
    // This would read joystick_x_pin and joystick_y_pin and map to x/z axes
}

static uint8_t calculate_hat_value(bool up, bool down, bool left, bool right) {
    // Hat switch values for 8-direction D-pad
    // 0=North, 1=NE, 2=East, 3=SE, 4=South, 5=SW, 6=West, 7=NW, 8=Center
    
    if (up && !down && !left && !right) return 0;      // North
    if (up && !down && !left && right)  return 1;      // Northeast  
    if (!up && !down && !left && right) return 2;      // East
    if (!up && down && !left && right)  return 3;      // Southeast
    if (!up && down && !left && !right) return 4;      // South
    if (!up && down && left && !right)  return 5;      // Southwest
    if (!up && !down && left && !right) return 6;      // West
    if (up && !down && left && !right)  return 7;      // Northwest
    
    return 8;  // Center (no direction pressed)
}

static void send_hid_report(void) {
    tud_hid_report(0, &gamepad_report, sizeof(gamepad_report));
}

static void process_serial_commands(void) {
    // Process serial API commands for Windows app integration
    serial_api_task();
    
    // Check if config was updated and needs reload
    static uint32_t last_config_check = 0;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - last_config_check > 1000) {  // Check every second
        bgg_config_t new_config;
        if (config_load(&new_config)) {
            // Check if mode changed
            if (strcmp(new_config.usb_mode, "hid") != 0) {
                // Mode switch requested, reboot to bootloader
                watchdog_reboot(0, 0, 0);
            }
            
            // Update config if changed
            if (memcmp(&device_config, &new_config, sizeof(bgg_config_t)) != 0) {
                device_config = new_config;
                // TODO: Reinitialize hardware if pin mappings changed
            }
        }
        last_config_check = now;
    }
}

// TinyUSB callbacks
void tud_mount_cb(void) {
    // Device mounted
}

void tud_umount_cb(void) {
    // Device unmounted
}

void tud_suspend_cb(bool remote_wakeup_en) {
    // Device suspended
}

void tud_resume_cb(void) {
    // Device resumed
}

// HID callbacks
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    // Standard USB gamepad descriptor
    static uint8_t const desc_hid_report[] = {
        0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
        0x09, 0x05,        // Usage (Game Pad)
        0xa1, 0x01,        // Collection (Application)
        0x15, 0x00,        //   Logical Minimum (0)
        0x25, 0x01,        //   Logical Maximum (1)
        0x35, 0x00,        //   Physical Minimum (0)
        0x45, 0x01,        //   Physical Maximum (1)
        0x75, 0x01,        //   Report Size (1)
        0x95, 0x10,        //   Report Count (16)
        0x05, 0x09,        //   Usage Page (Button)
        0x19, 0x01,        //   Usage Minimum (0x01)
        0x29, 0x10,        //   Usage Maximum (0x10)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
        0x25, 0x07,        //   Logical Maximum (7)
        0x46, 0x3b, 0x01,  //   Physical Maximum (315)
        0x75, 0x04,        //   Report Size (4)
        0x95, 0x01,        //   Report Count (1)
        0x65, 0x14,        //   Unit (System: English Rotation, Length: Centimeter)
        0x09, 0x39,        //   Usage (Hat switch)
        0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
        0x65, 0x00,        //   Unit (None)
        0x95, 0x01,        //   Report Count (1)
        0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x26, 0xff, 0x00,  //   Logical Maximum (255)
        0x46, 0xff, 0x00,  //   Physical Maximum (255)
        0x09, 0x30,        //   Usage (X)
        0x09, 0x31,        //   Usage (Y)
        0x09, 0x32,        //   Usage (Z)
        0x09, 0x35,        //   Usage (Rz)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x04,        //   Report Count (4)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x06, 0x00, 0xff,  //   Usage Page (Vendor Defined 0xFF00)
        0x09, 0x20,        //   Usage (0x20)
        0x09, 0x21,        //   Usage (0x21)
        0x95, 0x02,        //   Report Count (2)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0xc0,              // End Collection
    };
    
    return desc_hid_report;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    // Return current gamepad state
    if (report_type == HID_REPORT_TYPE_INPUT) {
        memcpy(buffer, &gamepad_report, sizeof(gamepad_report));
        return sizeof(gamepad_report);
    }
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    // Handle output reports (LED commands, etc.)
}
