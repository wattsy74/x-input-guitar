#include "config.h"
#include "config_storage.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Default configuration matching BGG Windows App config.json format
static const config_t default_config = {
    .metadata = {
        .version = "4.0.0",
        .description = "BumbleGum Guitar Controller Configuration",
        .lastUpdated = "2025-08-21"
    },
    .device_name = "Guitar Controller",
    .UP = "GP2",
    .DOWN = "GP3", 
    .LEFT = "GP4",
    .RIGHT = "GP5",
    .GREEN_FRET = "GP10",
    .GREEN_FRET_led = 6,
    .RED_FRET = "GP11",
    .RED_FRET_led = 5,
    .YELLOW_FRET = "GP12",
    .YELLOW_FRET_led = 4,
    .BLUE_FRET = "GP13",
    .BLUE_FRET_led = 3,
    .ORANGE_FRET = "GP14",
    .ORANGE_FRET_led = 2,
    .STRUM_UP = "GP7",
    .STRUM_UP_led = 0,
    .STRUM_DOWN = "GP8",
    .STRUM_DOWN_led = 1,
    .TILT = "GP9",
    .SELECT = "GP0",
    .START = "GP1",
    .GUIDE = "GP6",
    .WHAMMY = "GP27",
    .neopixel_pin = "GP23",
    .joystick_x_pin = "GP28",
    .joystick_y_pin = "GP29",
    .hat_mode = "dpad",
    .led_brightness = 1.0f,
    .whammy_min = 500,
    .whammy_max = 65000,
    .whammy_reverse = false,
    .tilt_wave_enabled = true,
    .led_color = {
        "#FFFFFF", "#FFFFFF", "#B33E00", "#0000FF", 
        "#FFFF00", "#FF0000", "#00FF00"
    },
    .released_color = {
        "#888888", "#888888", "#884400", "#0000FF",
        "#AAAA00", "#AA0000", "#00AA00"
    }
};

// Active configuration (loaded from flash or defaults)
config_t device_config;

// Simple JSON value extractor - finds "key": value pairs
static int extract_json_int(const char* json, const char* key) {
    char search_pattern[64];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\":", key);
    
    char* pos = strstr(json, search_pattern);
    if (!pos) return -1;
    
    pos += strlen(search_pattern);
    
    // Skip whitespace
    while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
    
    return atoi(pos);
}

uint8_t config_gp_to_gpio(const char* gp_string) {
    if (!gp_string || strlen(gp_string) < 3) return 0;
    if (strncmp(gp_string, "GP", 2) != 0) return 0;
    return (uint8_t)atoi(gp_string + 2);  // Skip "GP" prefix
}

// Helper functions to get GPIO pins as numbers
uint8_t config_get_green_pin(void) { return config_gp_to_gpio(device_config.GREEN_FRET); }
uint8_t config_get_red_pin(void) { return config_gp_to_gpio(device_config.RED_FRET); }
uint8_t config_get_yellow_pin(void) { return config_gp_to_gpio(device_config.YELLOW_FRET); }
uint8_t config_get_blue_pin(void) { return config_gp_to_gpio(device_config.BLUE_FRET); }
uint8_t config_get_orange_pin(void) { return config_gp_to_gpio(device_config.ORANGE_FRET); }
uint8_t config_get_strum_up_pin(void) { return config_gp_to_gpio(device_config.STRUM_UP); }
uint8_t config_get_strum_down_pin(void) { return config_gp_to_gpio(device_config.STRUM_DOWN); }
uint8_t config_get_start_pin(void) { return config_gp_to_gpio(device_config.START); }
uint8_t config_get_select_pin(void) { return config_gp_to_gpio(device_config.SELECT); }
uint8_t config_get_dpad_up_pin(void) { return config_gp_to_gpio(device_config.UP); }
uint8_t config_get_dpad_down_pin(void) { return config_gp_to_gpio(device_config.DOWN); }
uint8_t config_get_dpad_left_pin(void) { return config_gp_to_gpio(device_config.LEFT); }
uint8_t config_get_dpad_right_pin(void) { return config_gp_to_gpio(device_config.RIGHT); }
uint8_t config_get_guide_pin(void) { return config_gp_to_gpio(device_config.GUIDE); }
uint8_t config_get_whammy_pin(void) { return config_gp_to_gpio(device_config.WHAMMY); }
uint8_t config_get_neopixel_pin(void) { return config_gp_to_gpio(device_config.neopixel_pin); }
uint8_t config_get_joystick_x_pin(void) { return config_gp_to_gpio(device_config.joystick_x_pin); }
uint8_t config_get_joystick_y_pin(void) { return config_gp_to_gpio(device_config.joystick_y_pin); }

void config_init(void) {
    printf("Config: Initializing configuration system...\n");
    
    // Initialize config storage
    config_storage_init();
    
    // Try to load configuration from flash
    if (config_storage_load_from_flash(&device_config)) {
        printf("Config: Successfully loaded configuration from flash\n");
        config_print_current();
    } else {
        printf("Config: Failed to load from flash, using defaults\n");
        // Use default configuration
        memcpy(&device_config, &default_config, sizeof(config_t));
        config_print_current();
        
        // Try to save defaults to flash for next boot
        char json_buffer[1024];
        if (config_generate_json(&device_config, json_buffer, sizeof(json_buffer))) {
            if (config_storage_save_to_flash(json_buffer, strlen(json_buffer))) {
                printf("Config: Default configuration saved to flash\n");
            } else {
                printf("Config: Warning - failed to save defaults to flash\n");
            }
        }
    }
}

void config_print_current(void) {
    printf("=== Current Configuration ===\n");
    printf("Device: %s\n", device_config.device_name);
    printf("Version: %s (%s)\n", device_config.metadata.version, device_config.metadata.lastUpdated);
    printf("LED Brightness: %.2f\n", device_config.led_brightness);
    printf("Button Pins:\n");
    printf("  Green: %s (LED %d), Red: %s (LED %d), Yellow: %s (LED %d)\n",
           device_config.GREEN_FRET, device_config.GREEN_FRET_led,
           device_config.RED_FRET, device_config.RED_FRET_led,
           device_config.YELLOW_FRET, device_config.YELLOW_FRET_led);
    printf("  Blue: %s (LED %d), Orange: %s (LED %d)\n",
           device_config.BLUE_FRET, device_config.BLUE_FRET_led,
           device_config.ORANGE_FRET, device_config.ORANGE_FRET_led);
    printf("  Strum Up: %s (LED %d), Strum Down: %s (LED %d)\n",
           device_config.STRUM_UP, device_config.STRUM_UP_led,
           device_config.STRUM_DOWN, device_config.STRUM_DOWN_led);
    printf("  Start: %s, Select: %s, Guide: %s, Tilt: %s\n",
           device_config.START, device_config.SELECT, device_config.GUIDE, device_config.TILT);
    printf("  D-Pad - Up: %s, Down: %s, Left: %s, Right: %s\n",
           device_config.UP, device_config.DOWN, device_config.LEFT, device_config.RIGHT);
    printf("Analog Pins:\n");
    printf("  Whammy: %s, Joystick X: %s, Joystick Y: %s\n",
           device_config.WHAMMY, device_config.joystick_x_pin, device_config.joystick_y_pin);
    printf("LED Configuration:\n");
    printf("  NeoPixel Pin: %s, Hat Mode: %s\n", device_config.neopixel_pin, device_config.hat_mode);
    printf("  Whammy Range: %lu - %lu, Reverse: %s, Tilt Wave: %s\n",
           device_config.whammy_min, device_config.whammy_max,
           device_config.whammy_reverse ? "Yes" : "No",
           device_config.tilt_wave_enabled ? "Yes" : "No");
    printf("=============================\n");
}

bool config_update_from_json(const char* json_string) {
    config_t new_config;
    
    // Parse the new configuration
    if (!config_parse_json(json_string, &new_config)) {
        printf("Config: Failed to parse JSON configuration\n");
        return false;
    }
    
    // Validate the configuration
    if (!config_validate(&new_config)) {
        printf("Config: Configuration validation failed\n");
        return false;
    }
    
    // Save to flash first
    if (!config_storage_save_to_flash(json_string, strlen(json_string))) {
        printf("Config: Failed to save configuration to flash\n");
        return false;
    }
    
    // Update active configuration
    memcpy(&device_config, &new_config, sizeof(config_t));
    
    printf("Config: Configuration updated successfully\n");
    config_print_current();
    
    return true;
}

bool config_validate(const config_t* config) {
    if (!config) return false;
    
    // Validate metadata
    if (!config->metadata.version || strlen(config->metadata.version) == 0) {
        printf("Config: Invalid or missing version\n");
        return false;
    }
    if (!config->metadata.description || strlen(config->metadata.description) == 0) {
        printf("Config: Invalid or missing description\n");
        return false;
    }
    if (!config->metadata.lastUpdated || strlen(config->metadata.lastUpdated) == 0) {
        printf("Config: Invalid or missing lastUpdated\n");
        return false;
    }
    
    // Validate device name
    if (!config->device_name || strlen(config->device_name) == 0) {
        printf("Config: Invalid or missing device_name\n");
        return false;
    }
    
    // Validate GP pin strings
    const char* pins[] = {
        config->UP, config->DOWN, config->LEFT, config->RIGHT,
        config->GREEN_FRET, config->RED_FRET, config->YELLOW_FRET, 
        config->BLUE_FRET, config->ORANGE_FRET,
        config->STRUM_UP, config->STRUM_DOWN, config->TILT,
        config->SELECT, config->START, config->GUIDE,
        config->WHAMMY, config->neopixel_pin,
        config->joystick_x_pin, config->joystick_y_pin
    };
    
    const char* pin_names[] = {
        "UP", "DOWN", "LEFT", "RIGHT",
        "GREEN_FRET", "RED_FRET", "YELLOW_FRET", 
        "BLUE_FRET", "ORANGE_FRET",
        "STRUM_UP", "STRUM_DOWN", "TILT",
        "SELECT", "START", "GUIDE",
        "WHAMMY", "neopixel_pin",
        "joystick_x_pin", "joystick_y_pin"
    };
    
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        if (!pins[i] || strncmp(pins[i], "GP", 2) != 0) {
            printf("Config: Invalid GP pin string for %s: %s\n", pin_names[i], pins[i] ? pins[i] : "NULL");
            return false;
        }
        
        // Validate pin number range
        int pin_num = atoi(pins[i] + 2);
        if (pin_num < 0 || pin_num > 29) {
            printf("Config: Invalid pin number for %s: %s (must be GP0-GP29)\n", pin_names[i], pins[i]);
            return false;
        }
    }
    
    // Validate LED assignments (0-6 range)
    if (config->GREEN_FRET_led > 6 || config->RED_FRET_led > 6 || 
        config->YELLOW_FRET_led > 6 || config->BLUE_FRET_led > 6 || 
        config->ORANGE_FRET_led > 6 || config->STRUM_UP_led > 6 || 
        config->STRUM_DOWN_led > 6) {
        printf("Config: Invalid LED assignment (must be 0-6)\n");
        return false;
    }
    
    // Validate brightness (0.0 to 1.0)
    if (config->led_brightness < 0.0f || config->led_brightness > 1.0f) {
        printf("Config: Invalid LED brightness %.2f (must be 0.0-1.0)\n", config->led_brightness);
        return false;
    }
    
    // Validate whammy values
    if (config->whammy_min >= config->whammy_max) {
        printf("Config: Invalid whammy range: min %lu >= max %lu\n", config->whammy_min, config->whammy_max);
        return false;
    }
    
    // Validate hat mode
    if (!config->hat_mode || (strcmp(config->hat_mode, "dpad") != 0 && 
        strcmp(config->hat_mode, "joystick") != 0)) {
        printf("Config: Invalid hat_mode: %s (must be 'dpad' or 'joystick')\n", 
               config->hat_mode ? config->hat_mode : "NULL");
        return false;
    }
    
    // Validate LED color arrays
    for (int i = 0; i < 7; i++) {
        if (!config->led_color[i] || strlen(config->led_color[i]) == 0 || config->led_color[i][0] != '#') {
            printf("Config: Invalid led_color[%d]: %s (must start with #)\n", 
                   i, config->led_color[i] ? config->led_color[i] : "NULL");
            return false;
        }
        if (!config->released_color[i] || strlen(config->released_color[i]) == 0 || config->released_color[i][0] != '#') {
            printf("Config: Invalid released_color[%d]: %s (must start with #)\n", 
                   i, config->released_color[i] ? config->released_color[i] : "NULL");
            return false;
        }
    }
    
    printf("Config: Validation passed\n");
    return true;
}
