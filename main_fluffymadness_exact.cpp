/*
 * BGG XInput Guitar Controller Firmware
 * Based on fluffymadness tinyusb-xinput implementation
 * 
 * Hardware: Raspberry Pi Pico
 * Purpose: Guitar Hero/Rock Band controller with XInput compatibility
 */

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include <string.h>

//--------------------------------------------------------------------+
// HARDWARE CONFIGURATION
//--------------------------------------------------------------------+
// Guitar button pins (active low with internal pullups) - from xInput-Button-Mappings.txt
#define PIN_GREEN       10   // GP10 - GREEN_FRET -> A
#define PIN_RED         11   // GP11 - RED_FRET -> B
#define PIN_YELLOW      12   // GP12 - YELLOW_FRET -> Y
#define PIN_BLUE        13   // GP13 - BLUE_FRET -> X
#define PIN_ORANGE      14   // GP14 - ORANGE_FRET -> Left Shoulder
#define PIN_START       1    // GP1 - START -> Start
#define PIN_SELECT      0    // GP0 - SELECT -> Back
#define PIN_STRUM_UP    7    // GP7 - STRUM_UP -> DPad Up
#define PIN_STRUM_DOWN  8    // GP8 - STRUM_DOWN -> DPad Down
#define PIN_STRUM_UP_2  2    // GP2 - STRUM_UP (secondary) -> DPad Up
#define PIN_STRUM_DOWN_2 3   // GP3 - STRUM_DOWN (secondary) -> DPad Down
#define PIN_DPAD_LEFT   4    // GP4 - LEFT -> DPad Left
#define PIN_DPAD_RIGHT  5    // GP5 - RIGHT -> DPad Right
#define PIN_TILT        9    // GP9 - TILT -> Right Stick Y Axis
#define PIN_GUIDE       6    // GP6 - CENTRE -> Guide Button

// Analog input pins
#define PIN_WHAMMY      27   // GP27 - WHAMMY -> Right Stick X Axis (ADC1)
#define PIN_JOYSTICK_X  28   // GP28 - Joystick X -> Left Stick X Axis (ADC2)
#define PIN_JOYSTICK_Y  29   // GP29 - Joystick Y -> Left Stick Y Axis (ADC3)

// LED pin
#define PIN_NEOPIXEL    23   // GP23 - Neopixel Data Pin

//--------------------------------------------------------------------+
// XINPUT CONTROLLER DEFINITIONS
//--------------------------------------------------------------------+
// XInput button bit masks
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_GUIDE            0x0400
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

// XInput report structure (exactly as fluffymadness)
typedef struct {
    uint8_t rid;
    uint8_t rsize;
    uint8_t digital_buttons_1;
    uint8_t digital_buttons_2;
    uint8_t lt;
    uint8_t rt;
    int16_t l_x;      // Changed: int16_t for proper Xbox 360 format
    int16_t l_y;
    int16_t r_x;
    int16_t r_y;
    uint8_t reserved_1[6];
} ReportDataXinput;

ReportDataXinput XboxButtonData;

//--------------------------------------------------------------------+
// USB DESCRIPTORS (Exact fluffymadness format)
//--------------------------------------------------------------------+

// Device Descriptor
tusb_desc_device_t const xinputDeviceDescriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0xFF,
    .bDeviceSubClass = 0xFF,
    .bDeviceProtocol = 0xFF,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x045E,
    .idProduct = 0x028E,
    .bcdDevice = 0x0572,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration Descriptor (Raw bytes - exact fluffymadness format)
const uint8_t xinputConfigurationDescriptor[] = {
    // Configuration Descriptor:
    0x09,   // bLength
    0x02,   // bDescriptorType
    0x30,0x00,  // wTotalLength (48 bytes)
    0x01,   // bNumInterfaces
    0x01,   // bConfigurationValue
    0x00,   // iConfiguration
    0x80,   // bmAttributes (Bus-powered Device)
    0xFA,   // bMaxPower (500 mA)

    // Interface Descriptor:
    0x09,   // bLength
    0x04,   // bDescriptorType
    0x00,   // bInterfaceNumber
    0x00,   // bAlternateSetting
    0x02,   // bNumEndPoints
    0xFF,   // bInterfaceClass (Vendor specific)
    0x5D,   // bInterfaceSubClass
    0x01,   // bInterfaceProtocol
    0x00,   // iInterface

    // Unknown Descriptor:
    0x10,
    0x21, 
    0x10, 
    0x01, 
    0x01, 
    0x24, 
    0x81, 
    0x14, 
    0x03, 
    0x00, 
    0x03,
    0x13, 
    0x02, 
    0x00, 
    0x03, 
    0x00, 

    // Endpoint Descriptor:
    0x07,   // bLength
    0x05,   // bDescriptorType
    0x81,   // bEndpointAddress (IN endpoint 1)
    0x03,   // bmAttributes (Transfer: Interrupt / Synch: None / Usage: Data)
    0x20,0x00,  // wMaxPacketSize (1 x 32 bytes)
    0x04,   // bInterval (4 frames)

    // Endpoint Descriptor:
    0x07,   // bLength
    0x05,   // bDescriptorType
    0x02,   // bEndpointAddress (OUT endpoint 2)
    0x03,   // bmAttributes (Transfer: Interrupt / Synch: None / Usage: Data)
    0x20,0x00,  // wMaxPacketSize (1 x 32 bytes)
    0x08,   // bInterval (8 frames)
};

// String descriptors
char const *string_desc_arr_xinput[] = {
    (const char[]){0x09, 0x04}, // 0: Language (English)
    "GENERIC",                  // 1: Manufacturer
    "XINPUT CONTROLLER",        // 2: Product
    "1.0"                       // 3: Serial
};

//--------------------------------------------------------------------+
// XINPUT DRIVER (Custom driver like fluffymadness)
//--------------------------------------------------------------------+
uint8_t endpoint_in = 0;
uint8_t endpoint_out = 0;

static inline uint32_t board_millis(void) {
    return to_ms_since_boot(get_absolute_time());
}

static void xinput_init(void) {
    // Driver initialization
}

static void xinput_reset(uint8_t __unused rhport) {
    // Driver reset
}

static uint16_t xinput_open(uint8_t __unused rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
    // +16 is for the unknown descriptor 
    uint16_t const drv_len = sizeof(tusb_desc_interface_t) + itf_desc->bNumEndpoints*sizeof(tusb_desc_endpoint_t) + 16;
    TU_VERIFY(max_len >= drv_len, 0);

    uint8_t const * p_desc = tu_desc_next(itf_desc);
    uint8_t found_endpoints = 0;
    while ( (found_endpoints < itf_desc->bNumEndpoints) && (drv_len <= max_len) ) {
        tusb_desc_endpoint_t const * desc_ep = (tusb_desc_endpoint_t const *) p_desc;
        if ( TUSB_DESC_ENDPOINT == tu_desc_type(desc_ep) ) {
            TU_ASSERT(usbd_edpt_open(rhport, desc_ep));

            if ( tu_edpt_dir(desc_ep->bEndpointAddress) == TUSB_DIR_IN ) {
                endpoint_in = desc_ep->bEndpointAddress;
            } else {
                endpoint_out = desc_ep->bEndpointAddress;
            }
            found_endpoints += 1;
        }
        p_desc = tu_desc_next(p_desc);
    }
    return drv_len;
}

static bool xinput_device_control_request(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return true;
}

static bool xinput_control_complete_cb(uint8_t __unused rhport, tusb_control_request_t __unused const *request) {
    return true;
}

static bool xinput_xfer_cb(uint8_t __unused rhport, uint8_t __unused ep_addr, xfer_result_t __unused result, uint32_t __unused xferred_bytes) {
    return true;
}

static usbd_class_driver_t const xinput_driver = {
    #if CFG_TUSB_DEBUG >= 2
    .name = "XINPUT",
    #endif
    .init             = xinput_init,
    .reset            = xinput_reset,
    .open             = xinput_open,
    .control_xfer_cb  = xinput_device_control_request,
    .xfer_cb          = xinput_xfer_cb,
    .sof              = NULL
};

// Implement callback to add our custom driver
usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
    *driver_count = 1;
    return &xinput_driver;
}

//--------------------------------------------------------------------+
// USB DESCRIPTOR CALLBACKS
//--------------------------------------------------------------------+
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&xinputDeviceDescriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return xinputConfigurationDescriptor;
}

static uint16_t _desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    
    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr_xinput[0], 2);
        chr_count = 1;
    } else {
        if (!(index < sizeof(string_desc_arr_xinput) / sizeof(string_desc_arr_xinput[0])))
            return NULL;

        const char *str = string_desc_arr_xinput[index];
        chr_count = strlen(str);
        if (chr_count > 31) chr_count = 31;

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
    return _desc_str;
}

uint8_t const *tud_hid_descriptor_report_cb(void) { 
    return 0;
}

//--------------------------------------------------------------------+
// HARDWARE INITIALIZATION
//--------------------------------------------------------------------+
static void init_gpio(void) {
    // Initialize button pins with internal pullups
    const uint8_t button_pins[] = {
        PIN_GREEN, PIN_RED, PIN_YELLOW, PIN_BLUE, PIN_ORANGE,
        PIN_START, PIN_SELECT, PIN_STRUM_UP, PIN_STRUM_DOWN,
        PIN_STRUM_UP_2, PIN_STRUM_DOWN_2, PIN_DPAD_LEFT, PIN_DPAD_RIGHT, PIN_TILT, PIN_GUIDE
    };
    
    for (int i = 0; i < sizeof(button_pins); i++) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }
    
    // Initialize ADC for analog inputs
    adc_init();
    adc_gpio_init(PIN_WHAMMY);     // ADC1
    adc_gpio_init(PIN_JOYSTICK_X); // ADC2
    adc_gpio_init(PIN_JOYSTICK_Y); // ADC3
}

//--------------------------------------------------------------------+
// INPUT READING AND REPORT GENERATION
//--------------------------------------------------------------------+
static void read_guitar_inputs(void) {
    // Clear button state
    XboxButtonData.digital_buttons_1 = 0;
    XboxButtonData.digital_buttons_2 = 0;
    
    // Xbox 360 Controller Button Layout per your mapping:
    // digital_buttons_1: [DPad_Right][DPad_Left][DPad_Down][DPad_Up][Start][Back][L3][R3]
    // digital_buttons_2: [Y][X][B][A][unused][unused][RB][LB]
    
    // Map guitar fret buttons exactly as specified
    if (!gpio_get(PIN_GREEN))     XboxButtonData.digital_buttons_2 |= 0x10; // Green -> A (bit 4)
    if (!gpio_get(PIN_RED))       XboxButtonData.digital_buttons_2 |= 0x20; // Red -> B (bit 5)  
    if (!gpio_get(PIN_YELLOW))    XboxButtonData.digital_buttons_2 |= 0x80; // Yellow -> Y (bit 7)
    if (!gpio_get(PIN_BLUE))      XboxButtonData.digital_buttons_2 |= 0x40; // Blue -> X (bit 6)
    if (!gpio_get(PIN_ORANGE))    XboxButtonData.digital_buttons_2 |= 0x01; // Orange -> Left Shoulder (bit 0)
    
    // Map DPad - fix the bit positions
    bool strum_up = !gpio_get(PIN_STRUM_UP) || !gpio_get(PIN_STRUM_UP_2);
    bool strum_down = !gpio_get(PIN_STRUM_DOWN) || !gpio_get(PIN_STRUM_DOWN_2);
    
    if (strum_up)                 XboxButtonData.digital_buttons_1 |= 0x01; // Strum Up -> DPad Up (bit 0)
    if (strum_down)               XboxButtonData.digital_buttons_1 |= 0x02; // Strum Down -> DPad Down (bit 1)
    if (!gpio_get(PIN_DPAD_LEFT)) XboxButtonData.digital_buttons_1 |= 0x04; // Left -> DPad Left (bit 2)
    if (!gpio_get(PIN_DPAD_RIGHT)) XboxButtonData.digital_buttons_1 |= 0x08; // Right -> DPad Right (bit 3)
    
    // Map control buttons
    if (!gpio_get(PIN_START))     XboxButtonData.digital_buttons_1 |= 0x10; // Start -> Start (bit 4)
    if (!gpio_get(PIN_SELECT))    XboxButtonData.digital_buttons_1 |= 0x20; // Select -> Back (bit 5)
    
    // Guide button - needs to be in digital_buttons_2 as per Xbox 360 protocol
    if (!gpio_get(PIN_GUIDE))     XboxButtonData.digital_buttons_2 |= 0x04; // Guide -> bit 10 (0x0400 >> 8 = 0x04)
    
    // Read analog inputs - standard mapping per pin assignments
    // GP26 = ADC0, GP27 = ADC1, GP28 = ADC2, GP29 = ADC3
    
    // Joystick X-Axis (GP28) -> Left Stick X-Axis (direct mapping)
    adc_select_input(2);  // GP28 = ADC2
    uint16_t joy_x = adc_read();
    XboxButtonData.l_x = (int16_t)((joy_x - 2048) << 4);
    
    // Joystick Y-Axis (GP29) -> Left Stick Y-Axis (direct mapping)
    adc_select_input(3);  // GP29 = ADC3
    uint16_t joy_y = adc_read();
    XboxButtonData.l_y = (int16_t)((joy_y - 2048) << 4);
    
    // Whammy (GP27) -> Right Stick X-Axis (direct mapping)
    adc_select_input(1);  // GP27 = ADC1
    uint16_t whammy = adc_read();
    XboxButtonData.r_x = (int16_t)((whammy - 2048) << 4);
    
    // Tilt (GP9, digital) -> Right Stick Y-Axis
    bool tilt_active = !gpio_get(PIN_TILT);
    if (tilt_active) {
        XboxButtonData.r_y = -32768;  // 0% when tilt is pressed/on (inverted)
    } else {
        XboxButtonData.r_y = 32767;   // 100% when tilt is off (inverted)
    }
    
    // Clear triggers
    XboxButtonData.lt = 0;
    XboxButtonData.rt = 0;
    
    // Clear reserved bytes
    memset(XboxButtonData.reserved_1, 0, sizeof(XboxButtonData.reserved_1));
}

static void sendReportData(void) {
    // Poll every 1ms
    const uint32_t interval_ms = 1;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms) return;  // not enough time
    start_ms += interval_ms;

    // Remote wakeup
    if (tud_suspended()) {
        // Wake up host if we are in suspend mode
        // and REMOTE_WAKEUP feature is enabled by host
        tud_remote_wakeup();
    }

    // Update report data
    read_guitar_inputs();
    
    // Set report header
    XboxButtonData.rid = 0;
    XboxButtonData.rsize = 20;
    
    // Send report if ready
    if ((tud_ready()) && ((endpoint_in != 0)) && (!usbd_edpt_busy(0, endpoint_in))) {
        usbd_edpt_claim(0, endpoint_in);
        usbd_edpt_xfer(0, endpoint_in, (uint8_t*)&XboxButtonData, 20);
        usbd_edpt_release(0, endpoint_in);
    }   
}

//--------------------------------------------------------------------+
// MAIN APPLICATION
//--------------------------------------------------------------------+
int main(void) {
    // Initialize hardware
    stdio_init_all();
    init_gpio();
    
    // Initialize USB
    tusb_init();
    
    // Main loop
    while (1) {
        sendReportData();
        tud_task();  // tinyusb device task
    }
    
    return 0;
}
