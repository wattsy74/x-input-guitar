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
    
    // Force GPIO 23 since user confirmed wiring
    neopixel_pin = 23;
    printf("NeoPixel: Simple initialization on GPIO 23\n");
    
    // Use PIO1 SM0 - avoid potential USB conflicts
    pio = pio1;
    sm = 0;
    
    // Clean state machine setup
    if (pio_sm_is_claimed(pio, sm)) {
        pio_sm_unclaim(pio, sm);
    }
    
    pio_sm_claim(pio, sm);
    offset = pio_add_program(pio, &ws2812_program);
    
    // Initialize GPIO properly
    gpio_init(neopixel_pin);
    gpio_set_dir(neopixel_pin, GPIO_OUT);
    gpio_put(neopixel_pin, 0);
    
    // Configure WS2812 program
    ws2812_program_init(pio, sm, offset, neopixel_pin, 800000, false);
    
    // Simple startup test - single white flash
    printf("NeoPixel: Startup flash test\n");
    neopixel_set_pixel(0, 0xFFFFFF); // Use proper function for RGB->GRB conversion
    neopixel_show();
    sleep_ms(500);
    
    // Clear using proper function
    neopixel_clear();
    neopixel_show();
    
    printf("NeoPixel: Initialization complete\n");
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

void neopixel_update_button_state(const config_t* config, bool fret_states[5], bool strum_up, bool strum_down) {
    if (!config) return;
    
    // Clear all pixels first
    neopixel_clear();
    
    // Map button presses to LED positions based on config LED mappings
    for (int i = 0; i < 5; i++) {
        if (fret_states[i]) {
            uint8_t led_index = 0;
            const char* color_str = NULL;
            
            // Get LED index and color based on button type
            switch (i) {
                case 0: // Green fret
                    led_index = config->GREEN_FRET_led;
                    color_str = (led_index < 7) ? config->led_color[led_index] : config->led_color[0];
                    break;
                case 1: // Red fret
                    led_index = config->RED_FRET_led;
                    color_str = (led_index < 7) ? config->led_color[led_index] : config->led_color[1];
                    break;
                case 2: // Yellow fret
                    led_index = config->YELLOW_FRET_led;
                    color_str = (led_index < 7) ? config->led_color[led_index] : config->led_color[2];
                    break;
                case 3: // Blue fret
                    led_index = config->BLUE_FRET_led;
                    color_str = (led_index < 7) ? config->led_color[led_index] : config->led_color[3];
                    break;
                case 4: // Orange fret
                    led_index = config->ORANGE_FRET_led;
                    color_str = (led_index < 7) ? config->led_color[led_index] : config->led_color[4];
                    break;
            }
            
            if (led_index < num_pixels && color_str) {
                uint32_t color = neopixel_parse_color(color_str);
                neopixel_set_pixel(led_index, color);
            }
        }
    }
    
    // Handle strum indicators if configured
    if (strum_up) {
        // Light up all fret LEDs white for strum up
        for (int i = 0; i < 5; i++) {
            uint8_t led_index = 0;
            switch (i) {
                case 0: led_index = config->GREEN_FRET_led; break;
                case 1: led_index = config->RED_FRET_led; break;
                case 2: led_index = config->YELLOW_FRET_led; break;
                case 3: led_index = config->BLUE_FRET_led; break;
                case 4: led_index = config->ORANGE_FRET_led; break;
            }
            if (led_index < num_pixels) {
                neopixel_set_pixel(led_index, 0xFFFFFF); // White
            }
        }
    }
    
    // Show the updated state
    neopixel_show();
}

void neopixel_test() {
    printf("NeoPixel: Running test sequence\n");
    
    // Test 1: All white
    printf("NeoPixel: Test 1 - All white\n");
    neopixel_set_all(0xFFFFFF);
    neopixel_show();
    sleep_ms(1000);
    
    // Test 2: All red
    printf("NeoPixel: Test 2 - All red\n");
    neopixel_set_all(0xFF0000);
    neopixel_show();
    sleep_ms(1000);
    
    // Test 3: All green
    printf("NeoPixel: Test 3 - All green\n");
    neopixel_set_all(0x00FF00);
    neopixel_show();
    sleep_ms(1000);
    
    // Test 4: All blue
    printf("NeoPixel: Test 4 - All blue\n");
    neopixel_set_all(0x0000FF);
    neopixel_show();
    sleep_ms(1000);
    
    // Clear all
    printf("NeoPixel: Clearing all\n");
    neopixel_clear();
    neopixel_show();
    
    printf("NeoPixel: Test sequence complete\n");
}
