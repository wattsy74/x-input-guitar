#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include "pico/stdlib.h"
#include "hardware/pio.h"

// NeoPixel configuration
#define NEOPIXEL_PIN 23
#define NUM_PIXELS 7
#define NEOPIXEL_PIO pio0
#define NEOPIXEL_SM 0

// RGB color definitions
#define RGB_RED     0xFF0000
#define RGB_GREEN   0x00FF00
#define RGB_BLUE    0x0000FF
#define RGB_YELLOW  0xFFFF00
#define RGB_PURPLE  0x800080
#define RGB_CYAN    0x00FFFF
#define RGB_WHITE   0xFFFFFF
#define RGB_OFF     0x000000

// Function declarations
void neopixel_init(void);
void neopixel_set_pixel(uint8_t pixel, uint32_t color);
void neopixel_set_all(uint32_t color);
void neopixel_clear(void);
void neopixel_show(void);
void neopixel_debug_guide_press(void);
void neopixel_debug_count(uint32_t count);

#endif // NEOPIXEL_H
