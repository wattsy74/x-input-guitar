#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration structure matching BGG Windows App config.json format
typedef struct {
    // Metadata
    struct {
        const char* version;
        const char* description;
        const char* lastUpdated;
    } metadata;
    
    // Device info
    const char* device_name;
    
    // GPIO pin assignments (using GP prefix like BGG app)
    const char* UP;               // GP2
    const char* DOWN;             // GP3
    const char* LEFT;             // GP4
    const char* RIGHT;            // GP5
    const char* GREEN_FRET;       // GP10
    const char* RED_FRET;         // GP11
    const char* YELLOW_FRET;      // GP12
    const char* BLUE_FRET;        // GP13
    const char* ORANGE_FRET;      // GP14
    const char* STRUM_UP;         // GP7
    const char* STRUM_DOWN;       // GP8
    const char* TILT;             // GP9
    const char* SELECT;           // GP0
    const char* START;            // GP1
    const char* GUIDE;            // GP6
    const char* WHAMMY;           // GP27
    const char* neopixel_pin;     // GP23
    const char* joystick_x_pin;   // GP28
    const char* joystick_y_pin;   // GP29
    
    // LED assignments for each button
    uint8_t GREEN_FRET_led;
    uint8_t RED_FRET_led;
    uint8_t YELLOW_FRET_led;
    uint8_t BLUE_FRET_led;
    uint8_t ORANGE_FRET_led;
    uint8_t STRUM_UP_led;
    uint8_t STRUM_DOWN_led;
    
    // Settings
    const char* hat_mode;         // "dpad"
    float led_brightness;         // 0.0 to 1.0
    uint32_t whammy_min;
    uint32_t whammy_max;
    bool whammy_reverse;
    bool tilt_wave_enabled;
    
    // LED colors (7 element arrays)
    const char* led_color[7];
    const char* released_color[7];
} config_t;

// Function to initialize configuration system
void config_init(void);

// Function to print current configuration
void config_print_current(void);

// Function to update configuration from JSON string
bool config_update_from_json(const char* json_string);

// Function to validate configuration
bool config_validate(const config_t* config);

// Helper function to convert GP string to GPIO number (e.g. "GP10" -> 10)
uint8_t config_gp_to_gpio(const char* gp_string);

// Helper functions to get GPIO pins as numbers for easy use
uint8_t config_get_green_pin(void);
uint8_t config_get_red_pin(void);
uint8_t config_get_yellow_pin(void);
uint8_t config_get_blue_pin(void);
uint8_t config_get_orange_pin(void);
uint8_t config_get_strum_up_pin(void);
uint8_t config_get_strum_down_pin(void);
uint8_t config_get_start_pin(void);
uint8_t config_get_select_pin(void);
uint8_t config_get_dpad_up_pin(void);
uint8_t config_get_dpad_down_pin(void);
uint8_t config_get_dpad_left_pin(void);
uint8_t config_get_dpad_right_pin(void);
uint8_t config_get_guide_pin(void);
uint8_t config_get_whammy_pin(void);
uint8_t config_get_neopixel_pin(void);
uint8_t config_get_joystick_x_pin(void);
uint8_t config_get_joystick_y_pin(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H
