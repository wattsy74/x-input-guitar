#include "hid_interface.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "class/hid/hid_device.h"
#include <string.h>
#include <stdio.h>

//--------------------------------------------------------------------+
// HID CONSTANTS
//--------------------------------------------------------------------+
#define HID_VID           0x1209  // Generic VID
#define HID_PID           0x0001  // Generic PID for Guitar Hero controller

//--------------------------------------------------------------------+
// HID REPORT STRUCTURE
//--------------------------------------------------------------------+
typedef struct {
    uint8_t report_id;      // Report ID
    uint8_t buttons1;       // First 8 buttons
    uint8_t buttons2;       // Second 8 buttons  
    uint8_t dpad;           // D-Pad (hat switch)
    uint8_t whammy;         // Whammy bar (0-255)
    int8_t joy_x;           // Joystick X (-127 to 127)
    int8_t joy_y;           // Joystick Y (-127 to 127)
    uint8_t reserved;       // Reserved for future use
} __attribute__((packed)) hid_report_t;

// Global HID report
static hid_report_t hid_report;

//--------------------------------------------------------------------+
// HID REPORT DESCRIPTOR
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] = {
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      ),
    HID_USAGE      ( HID_USAGE_DESKTOP_GAMEPAD   ),
    HID_COLLECTION ( HID_COLLECTION_APPLICATION  ),
      // Report ID
      HID_REPORT_ID   ( 1 )
      
      // 16 buttons
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON ),
      HID_USAGE_MIN   ( 1                                      ),
      HID_USAGE_MAX   ( 16                                     ),
      HID_LOGICAL_MIN ( 0                                      ),
      HID_LOGICAL_MAX ( 1                                      ),
      HID_REPORT_COUNT( 16                                     ),
      HID_REPORT_SIZE ( 1                                      ),
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

      // HAT switch
      HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP                 ),
      HID_USAGE       ( HID_USAGE_DESKTOP_HAT_SWITCH           ),
      HID_LOGICAL_MIN ( 1                                      ),
      HID_LOGICAL_MAX ( 8                                      ),
      HID_PHYSICAL_MIN( 0                                      ),
      HID_PHYSICAL_MAX_N( 315,                             2   ),
      HID_REPORT_COUNT( 1                                      ),
      HID_REPORT_SIZE ( 8                                      ),
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

      // Whammy bar
      HID_USAGE       ( HID_USAGE_DESKTOP_Z                    ),
      HID_LOGICAL_MIN ( 0x00                                   ),
      HID_LOGICAL_MAX ( 0xff                                   ),
      HID_REPORT_COUNT( 1                                      ),
      HID_REPORT_SIZE ( 8                                      ),
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

      // X, Y joysticks
      HID_USAGE       ( HID_USAGE_DESKTOP_X                    ),
      HID_USAGE       ( HID_USAGE_DESKTOP_Y                    ),
      HID_LOGICAL_MIN ( 0x81                                   ),
      HID_LOGICAL_MAX ( 0x7f                                   ),
      HID_REPORT_COUNT( 2                                      ),
      HID_REPORT_SIZE ( 8                                      ),
      HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

      // Reserved byte
      HID_REPORT_COUNT( 1                                      ),
      HID_REPORT_SIZE ( 8                                      ),
      HID_INPUT       ( HID_CONSTANT                           ),

    HID_COLLECTION_END
};

//--------------------------------------------------------------------+
// USB DESCRIPTORS (STANDARD HID)
//--------------------------------------------------------------------+

// NOTE: Device descriptor moved to usb_descriptors.c to prevent conflicts

// Configuration Descriptor
enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),

    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), 0x81, 16, 10)
};

// String Descriptors
//--------------------------------------------------------------------+
// STRING DESCRIPTOR HANDLER
//--------------------------------------------------------------------+

static const char* hid_get_string_descriptor(uint8_t index) {
    static const char* string_desc_arr[] = {
        "",                         // 0: language descriptor (handled separately)
        "BumbleGum Guitars",        // 1: Manufacturer  
        "Guitar Hero Controller",   // 2: Product
        "1.0",                      // 3: Serials
    };
    
    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) {
        return NULL;
    }
    
    return string_desc_arr[index];
}

//--------------------------------------------------------------------+
// USB DESCRIPTOR CALLBACKS
//--------------------------------------------------------------------+

//--------------------------------------------------------------------+
// DESCRIPTOR HANDLERS
//--------------------------------------------------------------------+

static const uint8_t* hid_get_device_descriptor(void) {
    return (uint8_t const *) &desc_device;
}

static const uint8_t* hid_get_configuration_descriptor(uint8_t index) {
    (void) index;
    return desc_configuration;
}

// Invoked when received GET HID REPORT DESCRIPTOR
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance) {
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// HID CALLBACKS
//--------------------------------------------------------------------+

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
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
// HID INTERFACE IMPLEMENTATION
//--------------------------------------------------------------------+

static void hid_init(void) {
    // Initialize TinyUSB
    tusb_init();
    
    // Initialize HID report structure
    memset(&hid_report, 0, sizeof(hid_report));
    
    printf("HID interface initialized\n");
}

static void hid_task(void) {
    // TinyUSB device task
    tud_task();
}

static void hid_send_report(const button_state_t* state) {
    // Map Guitar Hero controls to HID gamepad
    memset(&hid_report, 0, sizeof(hid_report));
    
    // Fret buttons -> HID buttons
    if (state->green)  hid_report.buttons1 |= (1 << 0);    // Button 1
    if (state->red)    hid_report.buttons1 |= (1 << 1);    // Button 2
    if (state->yellow) hid_report.buttons1 |= (1 << 2);    // Button 3
    if (state->blue)   hid_report.buttons1 |= (1 << 3);    // Button 4
    if (state->orange) hid_report.buttons1 |= (1 << 4);    // Button 5
    
    // Strum bar -> Buttons
    if (state->strum_up)   hid_report.buttons1 |= (1 << 5); // Button 6
    if (state->strum_down) hid_report.buttons1 |= (1 << 6); // Button 7
    
    // Control buttons
    if (state->start)  hid_report.buttons1 |= (1 << 7);    // Button 8
    if (state->select) hid_report.buttons2 |= (1 << 0);    // Button 9
    if (state->guide)  hid_report.buttons2 |= (1 << 1);    // Button 10
    
    // D-Pad -> Hat switch (0=center, 1=N, 2=NE, 3=E, 4=SE, 5=S, 6=SW, 7=W, 8=NW)
    uint8_t hat = 0;  // Center
    if (state->dpad_up && !state->dpad_left && !state->dpad_right) hat = 1;      // N
    else if (state->dpad_up && state->dpad_right) hat = 2;                        // NE
    else if (state->dpad_right && !state->dpad_up && !state->dpad_down) hat = 3; // E
    else if (state->dpad_down && state->dpad_right) hat = 4;                      // SE
    else if (state->dpad_down && !state->dpad_left && !state->dpad_right) hat = 5; // S
    else if (state->dpad_down && state->dpad_left) hat = 6;                       // SW
    else if (state->dpad_left && !state->dpad_up && !state->dpad_down) hat = 7;  // W
    else if (state->dpad_up && state->dpad_left) hat = 8;                         // NW
    
    hid_report.dpad = hat;
    
    // Analog controls
    hid_report.whammy = (uint8_t)((state->whammy * 255UL) / 4095UL);  // Convert to 8-bit
    hid_report.joy_x = (int8_t)(state->joy_x / 256);                   // Convert to 8-bit signed
    hid_report.joy_y = (int8_t)(state->joy_y / 256);                   // Convert to 8-bit signed
    
    // Send the report if USB is ready
    if (tud_hid_ready()) {
        tud_hid_report(1, &hid_report, sizeof(hid_report));
    }
}

static bool hid_ready(void) {
    return tud_hid_ready();
}

// HID interface definition
const usb_interface_t hid_interface = {
    .init = hid_init,
    .task = hid_task,
    .send_report = hid_send_report,
    .ready = hid_ready,
    .get_string_descriptor = hid_get_string_descriptor,
    .get_device_descriptor = hid_get_device_descriptor,
    .get_configuration_descriptor = hid_get_configuration_descriptor
};
