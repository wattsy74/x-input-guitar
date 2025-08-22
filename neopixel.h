#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "config.h"

// NeoPixel configuration - back to PIO0 like original working version
#define NEOPIXEL_PIO pio0
#define NEOPIXEL_SM 0

#ifdef __cplusplus
extern "C" {
#endif

// Function declarations
void neopixel_init(const config_t* config);
void neopixel_set_pixel(uint8_t pixel, uint32_t color);
void neopixel_set_all(uint32_t color);
void neopixel_clear(void);
void neopixel_show(void);
void neopixel_test(void); // Test function for debugging
void neopixel_update_button_state(const config_t* config, bool fret_states[5], bool strum_up, bool strum_down);
uint32_t neopixel_parse_color(const char* hex_color);

#ifdef __cplusplus
}
#endif

// BGG-compatible color definitions
#define RGB_RED     0xFF0000
#define RGB_GREEN   0x00FF00
#define RGB_BLUE    0x0000FF
#define RGB_YELLOW  0xFFFF00
#define RGB_PURPLE  0x800080
#define RGB_CYAN    0x00FFFF
#define RGB_WHITE   0xFFFFFF
#define RGB_OFF     0x000000

#endif // NEOPIXEL_H
