#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "tusb.h"
#include "src/config_manager.h"
#include "src/serial_api.h"
#include <string.h>

// XInput constants
#define XINPUT_INTERFACE    0
#define XINPUT_ENDPOINT_IN  0x81
#define XINPUT_ENDPOINT_OUT 0x01

// XInput report structure  
typedef struct {
    uint8_t report_id;
    uint8_t report_size;
    uint16_t buttons;
    uint8_t left_trigger;
    uint8_t right_trigger;
    int16_t left_stick_x;
    int16_t left_stick_y;
    int16_t right_stick_x;
    int16_t right_stick_y;
    uint8_t reserved[6];
} __attribute__((packed)) xinput_gamepad_t;

// Configuration
static bgg_config_t device_config;
static xinput_gamepad_t gamepad_report = {0};

// Function prototypes
static void init_hardware(void);
static void read_inputs(void);
static void send_xinput_report(void);
static void process_serial_commands(void);

int main() {
    // Initialize system
    stdio_init_all();
    
    // Load configuration
    if (!config_init() || !config_load(&device_config)) {
        // Config failed, use defaults
        device_config = DEFAULT_CONFIG;
        config_save(&device_config);
    }
    
    // Verify we're supposed to be in XInput mode
    if (strcmp(device_config.usb_mode, "xinput") != 0) {
        // Wrong mode, request switch and reboot
        config_request_mode_switch("xinput");
        watchdog_reboot(0, 0, 0);
    }
    
    // Initialize hardware
    init_hardware();
    
    // Initialize serial API
    serial_api_init();
    
    // Initialize USB
    tusb_init();
    
    // Initialize XInput report
    gamepad_report.report_id = 0;
    gamepad_report.report_size = 20;
    
    while (true) {
        // USB device task
        tud_task();
        
        // Process serial commands for Windows app integration
        process_serial_commands();
        
        // Read controller inputs
        read_inputs();
        
        // Send XInput report
        if (tud_ready()) {
            send_xinput_report();
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
    gamepad_report.buttons = 0;
    
    // Read digital buttons (active low)
    if (!gpio_get(device_config.GREEN_FRET))  gamepad_report.buttons |= 0x1000;  // A
    if (!gpio_get(device_config.RED_FRET))    gamepad_report.buttons |= 0x2000;  // B  
    if (!gpio_get(device_config.YELLOW_FRET)) gamepad_report.buttons |= 0x8000;  // Y
    if (!gpio_get(device_config.BLUE_FRET))   gamepad_report.buttons |= 0x4000;  // X
    if (!gpio_get(device_config.ORANGE_FRET)) gamepad_report.buttons |= 0x0020;  // LB
    
    if (!gpio_get(device_config.SELECT))      gamepad_report.buttons |= 0x0040;  // Back
    if (!gpio_get(device_config.START))       gamepad_report.buttons |= 0x0010;  // Start
    if (!gpio_get(device_config.GUIDE))       gamepad_report.buttons |= 0x0400;  // Guide
    
    // D-pad
    if (!gpio_get(device_config.UP))          gamepad_report.buttons |= 0x0001;
    if (!gpio_get(device_config.DOWN))        gamepad_report.buttons |= 0x0002;
    if (!gpio_get(device_config.LEFT))        gamepad_report.buttons |= 0x0004;
    if (!gpio_get(device_config.RIGHT))       gamepad_report.buttons |= 0x0008;
    
    // Strum as shoulder buttons
    if (!gpio_get(device_config.STRUM_UP))    gamepad_report.left_trigger = 255;
    if (!gpio_get(device_config.STRUM_DOWN))  gamepad_report.right_trigger = 255;
    
    // Whammy bar as left stick Y
    adc_select_input(device_config.WHAMMY - 26);  // Convert GPIO to ADC input
    uint16_t whammy_raw = adc_read();
    
    // Apply calibration
    int32_t whammy_calibrated = whammy_raw;
    if (whammy_calibrated < device_config.whammy_min) whammy_calibrated = device_config.whammy_min;
    if (whammy_calibrated > device_config.whammy_max) whammy_calibrated = device_config.whammy_max;
    
    // Convert to stick range (-32768 to 32767)
    int32_t whammy_range = device_config.whammy_max - device_config.whammy_min;
    int32_t whammy_scaled = ((whammy_calibrated - device_config.whammy_min) * 65535) / whammy_range - 32768;
    
    if (device_config.whammy_reverse) {
        whammy_scaled = -whammy_scaled;
    }
    
    gamepad_report.left_stick_y = (int16_t)whammy_scaled;
    
    // Tilt sensor as right stick X
    if (!gpio_get(device_config.TILT)) {
        gamepad_report.right_stick_x = 32767;  // Full tilt
    } else {
        gamepad_report.right_stick_x = 0;
    }
    
    // TODO: Read analog joystick if configured
    // This would read joystick_x_pin and joystick_y_pin
}

static void send_xinput_report(void) {
    if (tud_vendor_mounted() && tud_vendor_n_ready(0)) {
        tud_vendor_n_write(0, &gamepad_report, sizeof(gamepad_report));
        tud_vendor_n_write_flush(0);
    }
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
            if (strcmp(new_config.usb_mode, "xinput") != 0) {
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

// Vendor interface callbacks for XInput
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
    // Handle XInput control requests
    return true;
}

bool tud_vendor_set_report_cb(uint8_t instance, uint8_t report_id, uint8_t const* buffer, uint16_t bufsize) {
    // Handle XInput output reports (LED commands, etc.)
    return true;
}
