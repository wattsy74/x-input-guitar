#include "neopixel.h"
#include "ws2812.pio.h"
#include "hardware/clocks.h"
#include <string.h>

static uint32_t pixel_buffer[NUM_PIXELS];
static PIO pio = NEOPIXEL_PIO;
static uint sm = NEOPIXEL_SM;
static uint offset;

void neopixel_init(void) {
    // Clear pixel buffer
    memset(pixel_buffer, 0, sizeof(pixel_buffer));
    
    // Initialize PIO program
    offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, NEOPIXEL_PIN, 800000, false);
    
    // Clear all pixels
    neopixel_clear();
    neopixel_show();
}

void neopixel_set_pixel(uint8_t pixel, uint32_t color) {
    if (pixel < NUM_PIXELS) {
        // Convert RGB to GRB format for WS2812
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = color & 0xFF;
        pixel_buffer[pixel] = (g << 16) | (r << 8) | b;
    }
}

void neopixel_set_all(uint32_t color) {
    for (int i = 0; i < NUM_PIXELS; i++) {
        neopixel_set_pixel(i, color);
    }
}

void neopixel_clear(void) {
    memset(pixel_buffer, 0, sizeof(pixel_buffer));
}

void neopixel_show(void) {
    // Send all pixel data to the PIO
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(pio, sm, pixel_buffer[i]);
    }
}

void neopixel_debug_guide_press(void) {
    // Flash first LED purple for guide button press
    neopixel_clear();
    neopixel_set_pixel(0, RGB_PURPLE);
    neopixel_show();
    sleep_ms(100);
    neopixel_clear();
    neopixel_show();
}

void neopixel_debug_count(uint32_t count) {
    // Display count using yellow LEDs (binary representation)
    neopixel_clear();
    
    for (int i = 0; i < NUM_PIXELS && i < 32; i++) {
        if (count & (1 << i)) {
            neopixel_set_pixel(i, RGB_YELLOW);
        }
    }
    
    neopixel_show();
}
