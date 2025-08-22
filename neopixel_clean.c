#include "neopixel.h"
#include "ws2812.pio.h"
#include "hardware/clocks.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t pixel_buffer[7]; // Max 7 pixels for BGG
static PIO pio = NEOPIXEL_PIO;
static uint sm = NEOPIXEL_SM;
static uint offset;
static uint8_t neopixel_pin = 23; // Default, will be updated from config
static uint8_t num_pixels = 7;   // Always 7 for BGG

void neopixel_init(const config_t* config) {
    // Clear pixel buffer
    memset(pixel_buffer, 0, sizeof(pixel_buffer));
    
    // Get pin from config, force GPIO 23 if config is invalid
    if (config && config->neopixel_pin && config->neopixel_pin[0]) {
        int pin = config_gp_to_gpio(config->neopixel_pin);
        if (pin >= 0 && pin <= 28) {
            neopixel_pin = pin;
        } else {
            neopixel_pin = 23; // Force GPIO 23 - user confirmed hardware pin
        }
    } else {
        neopixel_pin = 23; // Force GPIO 23 - user confirmed hardware pin
    }
    
    printf("NeoPixel: Initializing on GPIO %d\n", neopixel_pin);
    
    // Initialize PIO program
    offset = pio_add_program(pio, &ws2812_program);
    
    // Configure the WS2812 PIO
    ws2812_program_init(pio, sm, offset, neopixel_pin, 800000, true);
    
    // Show white flash on startup to confirm working
    printf("NeoPixel: Sending white startup flash on GPIO %d\n", neopixel_pin);
    for (int i = 0; i < num_pixels; i++) {
        pixel_buffer[i] = 0xFFFFFF; // Bright white
    }
    neopixel_show();
    sleep_ms(200);
    
    // Clear all pixels after startup flash
    neopixel_clear();
    printf("NeoPixel: Initialized successfully on GPIO %d\n", neopixel_pin);
}

void neopixel_show() {
    // Send all pixels to the strip
    for (int i = 0; i < num_pixels; i++) {
        pio_sm_put_blocking(pio, sm, pixel_buffer[i] << 8u);
    }
}

void neopixel_set_pixel(uint8_t pixel, uint32_t color) {
    if (pixel < num_pixels) {
        // Convert RGB to GRB format for WS2812
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        
        // WS2812 expects GRB format
        pixel_buffer[pixel] = (g << 16) | (r << 8) | b;
    }
}

void neopixel_set_all(uint32_t color) {
    for (int i = 0; i < num_pixels; i++) {
        neopixel_set_pixel(i, color);
    }
}

void neopixel_clear() {
    memset(pixel_buffer, 0, sizeof(pixel_buffer));
}

// Parse hex color from config string (format: "#RRGGBB")
uint32_t neopixel_parse_color(const char* color_str) {
    if (!color_str || color_str[0] != '#' || strlen(color_str) != 7) {
        return 0x000000; // Default to black if invalid
    }
    
    // Parse hex string
    char hex_r[3] = {color_str[1], color_str[2], '\0'};
    char hex_g[3] = {color_str[3], color_str[4], '\0'};
    char hex_b[3] = {color_str[5], color_str[6], '\0'};
    
    uint8_t r = (uint8_t)strtol(hex_r, NULL, 16);
    uint8_t g = (uint8_t)strtol(hex_g, NULL, 16);
    uint8_t b = (uint8_t)strtol(hex_b, NULL, 16);
    
    return (r << 16) | (g << 8) | b;
}

void neopixel_update_button_state(const config_t* config, const bool fret_states[5], bool strum_up, bool strum_down) {
    if (!config) return;
    
    // Clear all pixels first
    neopixel_clear();
    
    // Button colors from config (brighter versions for visibility)
    uint32_t button_colors[5] = {
        neopixel_parse_color(config->green_color),   // Green fret
        neopixel_parse_color(config->red_color),     // Red fret
        neopixel_parse_color(config->yellow_color),  // Yellow fret
        neopixel_parse_color(config->blue_color),    // Blue fret
        neopixel_parse_color(config->orange_color)   // Orange fret
    };
    
    // Map button presses to LED positions based on config
    for (int i = 0; i < 5; i++) {
        if (fret_states[i]) {
            int led_index = config_get_button_led_index(config, i);
            if (led_index >= 0 && led_index < num_pixels) {
                neopixel_set_pixel(led_index, button_colors[i]);
            }
        }
    }
    
    // Handle strum indicators if configured
    if (strum_up) {
        // Light up all fret LEDs white for strum up
        for (int i = 0; i < 5; i++) {
            int led_index = config_get_button_led_index(config, i);
            if (led_index >= 0 && led_index < num_pixels) {
                neopixel_set_pixel(led_index, 0xFFFFFF); // White
            }
        }
    }
    
    // Show the updated state
    neopixel_show();
}
