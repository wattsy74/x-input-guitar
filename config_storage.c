#include "config_storage.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Default config JSON embedded at build time (BGG format)
static const char default_config_json[] = "{\n"
"  \"version\": \"4.0.0\",\n"
"  \"description\": \"BumbleGum Guitar Controller Configuration\",\n"
"  \"lastUpdated\": \"2025-08-21\",\n"
"  \"device_name\": \"Guitar Controller\",\n"
"  \"UP\": \"GP2\",\n"
"  \"DOWN\": \"GP3\",\n"
"  \"LEFT\": \"GP4\",\n"
"  \"RIGHT\": \"GP5\",\n"
"  \"GREEN_FRET\": \"GP10\",\n"
"  \"RED_FRET\": \"GP11\",\n"
"  \"YELLOW_FRET\": \"GP12\",\n"
"  \"BLUE_FRET\": \"GP13\",\n"
"  \"ORANGE_FRET\": \"GP14\",\n"
"  \"STRUM_UP\": \"GP7\",\n"
"  \"STRUM_DOWN\": \"GP8\",\n"
"  \"TILT\": \"GP9\",\n"
"  \"SELECT\": \"GP0\",\n"
"  \"START\": \"GP1\",\n"
"  \"GUIDE\": \"GP6\",\n"
"  \"WHAMMY\": \"GP27\",\n"
"  \"neopixel_pin\": \"GP23\",\n"
"  \"joystick_x_pin\": \"GP28\",\n"
"  \"joystick_y_pin\": \"GP29\",\n"
"  \"GREEN_FRET_led\": 6,\n"
"  \"RED_FRET_led\": 5,\n"
"  \"YELLOW_FRET_led\": 4,\n"
"  \"BLUE_FRET_led\": 3,\n"
"  \"ORANGE_FRET_led\": 2,\n"
"  \"STRUM_UP_led\": 0,\n"
"  \"STRUM_DOWN_led\": 1,\n"
"  \"hat_mode\": \"dpad\",\n"
"  \"led_brightness\": 1.0,\n"
"  \"whammy_min\": 500,\n"
"  \"whammy_max\": 65000,\n"
"  \"whammy_reverse\": false,\n"
"  \"tilt_wave_enabled\": true,\n"
"  \"led_color\": [\n"
"    \"#FFFFFF\", \"#FFFFFF\", \"#B33E00\", \"#0000FF\",\n"
"    \"#FFFF00\", \"#FF0000\", \"#00FF00\"\n"
"  ],\n"
"  \"released_color\": [\n"
"    \"#454545\", \"#454545\", \"#521C00\", \"#000091\",\n"
"    \"#696B00\", \"#8C0009\", \"#003D00\"\n"
"  ]\n"
"}";

// CRC32 calculation (simple implementation)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

uint32_t config_storage_calculate_crc32(const void* data, uint32_t size) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    
    for (uint32_t i = 0; i < size; i++) {
        uint8_t index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool config_storage_init(void) {
    printf("Config storage: Initializing...\n");
    
    // Check if valid config exists in flash
    if (config_storage_is_valid()) {
        printf("Config storage: Valid config found in flash\n");
        return true;
    }
    
    printf("Config storage: No valid config found, using default\n");
    
    // Save default config to flash
    return config_storage_save_to_flash(default_config_json, strlen(default_config_json));
}

bool config_storage_is_valid(void) {
    const config_storage_t* flash_config = (const config_storage_t*)(XIP_BASE + CONFIG_FLASH_OFFSET);
    
    // Check magic header
    if (flash_config->header.magic != CONFIG_MAGIC_HEADER) {
        return false;
    }
    
    // Check version
    if (flash_config->header.version != CONFIG_VERSION) {
        return false;
    }
    
    // Check JSON size
    if (flash_config->header.json_size == 0 || flash_config->header.json_size > CONFIG_JSON_MAX_SIZE) {
        return false;
    }
    
    // Verify checksum
    uint32_t calculated_crc = config_storage_calculate_crc32(flash_config->json_data, flash_config->header.json_size);
    if (calculated_crc != flash_config->header.checksum) {
        return false;
    }
    
    return true;
}

bool config_storage_save_to_flash(const char* json_data, uint32_t json_size) {
    if (json_size > CONFIG_JSON_MAX_SIZE) {
        printf("Config storage: JSON too large (%lu > %d)\n", json_size, CONFIG_JSON_MAX_SIZE);
        return false;
    }
    
    // Prepare config data in RAM
    config_storage_t config_data = {0};
    config_data.header.magic = CONFIG_MAGIC_HEADER;
    config_data.header.version = CONFIG_VERSION;
    config_data.header.json_size = json_size;
    config_data.header.checksum = config_storage_calculate_crc32(json_data, json_size);
    
    memcpy(config_data.json_data, json_data, json_size);
    
    printf("Config storage: Saving to flash (size: %lu bytes)\n", json_size);
    
    // Disable interrupts during flash operation
    uint32_t interrupts = save_and_disable_interrupts();
    
    // Erase the sector
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    
    // Write the config data
    flash_range_program(CONFIG_FLASH_OFFSET, (const uint8_t*)&config_data, FLASH_SECTOR_SIZE);
    
    // Re-enable interrupts
    restore_interrupts(interrupts);
    
    printf("Config storage: Flash write completed\n");
    
    // Verify the write
    if (!config_storage_is_valid()) {
        printf("Config storage: Flash write verification failed\n");
        return false;
    }
    
    printf("Config storage: Flash write verified successfully\n");
    return true;
}

bool config_storage_get_json(char* buffer, uint32_t buffer_size, uint32_t* actual_size) {
    if (!config_storage_is_valid()) {
        return false;
    }
    
    const config_storage_t* flash_config = (const config_storage_t*)(XIP_BASE + CONFIG_FLASH_OFFSET);
    
    if (actual_size) {
        *actual_size = flash_config->header.json_size;
    }
    
    if (buffer_size < flash_config->header.json_size + 1) {
        return false; // Buffer too small
    }
    
    memcpy(buffer, flash_config->json_data, flash_config->header.json_size);
    buffer[flash_config->header.json_size] = '\0'; // Null terminate
    
    return true;
}

bool config_storage_load_from_flash(config_t* config) {
    char json_buffer[CONFIG_JSON_MAX_SIZE + 1];
    
    if (!config_storage_get_json(json_buffer, sizeof(json_buffer), NULL)) {
        printf("Config storage: Failed to get JSON from flash\n");
        return false;
    }
    
    return config_parse_json(json_buffer, config);
}

void config_storage_format(void) {
    printf("Config storage: Formatting flash...\n");
    
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
    
    printf("Config storage: Format completed\n");
}

// Simple JSON parser for BGG config format
bool config_parse_json(const char* json, config_t* config) {
    // Note: This is a simplified parser for BGG JSON structure
    // For production, consider using a proper JSON parser like cJSON
    
    // Helper function to extract string values
    char* extract_string_value(const char* json, const char* key, char* buffer, size_t buffer_size) {
        char search_str[64];
        snprintf(search_str, sizeof(search_str), "\"%s\":", key);
        
        char* pos = strstr(json, search_str);
        if (!pos) return NULL;
        
        pos += strlen(search_str);
        while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
        
        if (*pos != '"') return NULL;
        pos++; // Skip opening quote
        
        char* end = strchr(pos, '"');
        if (!end) return NULL;
        
        size_t len = end - pos;
        if (len >= buffer_size) len = buffer_size - 1;
        
        strncpy(buffer, pos, len);
        buffer[len] = '\0';
        return buffer;
    }
    
    // Helper function to extract integer values
    int extract_int_value(const char* json, const char* key) {
        char search_str[64];
        snprintf(search_str, sizeof(search_str), "\"%s\":", key);
        
        char* pos = strstr(json, search_str);
        if (!pos) return -1;
        
        pos += strlen(search_str);
        while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
        
        return atoi(pos);
    }
    
    // Helper function to extract float values
    float extract_float_value(const char* json, const char* key) {
        char search_str[64];
        snprintf(search_str, sizeof(search_str), "\"%s\":", key);
        
        char* pos = strstr(json, search_str);
        if (!pos) return -1.0f;
        
        pos += strlen(search_str);
        while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
        
        return (float)atof(pos);
    }
    
    // Helper function to extract boolean values
    bool extract_bool_value(const char* json, const char* key, bool default_value) {
        char search_str[64];
        snprintf(search_str, sizeof(search_str), "\"%s\":", key);
        
        char* pos = strstr(json, search_str);
        if (!pos) return default_value;
        
        pos += strlen(search_str);
        while (*pos == ' ' || *pos == '\t' || *pos == '\n') pos++;
        
        if (strncmp(pos, "true", 4) == 0) return true;
        if (strncmp(pos, "false", 5) == 0) return false;
        
        return default_value;
    }
    
    // Static buffers for string storage (since config stores const char*)
    static char device_name_buf[64];
    static char up_buf[8], down_buf[8], left_buf[8], right_buf[8];
    static char green_fret_buf[8], red_fret_buf[8], yellow_fret_buf[8], blue_fret_buf[8], orange_fret_buf[8];
    static char strum_up_buf[8], strum_down_buf[8], tilt_buf[8], select_buf[8], start_buf[8], guide_buf[8];
    static char whammy_buf[8], neopixel_buf[8], joystick_x_buf[8], joystick_y_buf[8];
    static char hat_mode_buf[16];
    static char version_buf[16], description_buf[64], lastUpdated_buf[16];
    static char led_colors[7][8], released_colors[7][8];
    
    // Extract metadata
    if (extract_string_value(json, "version", version_buf, sizeof(version_buf))) {
        config->metadata.version = version_buf;
    } else {
        config->metadata.version = "4.0.0";
    }
    
    if (extract_string_value(json, "description", description_buf, sizeof(description_buf))) {
        config->metadata.description = description_buf;
    } else {
        config->metadata.description = "BumbleGum Guitar Controller Configuration";
    }
    
    if (extract_string_value(json, "lastUpdated", lastUpdated_buf, sizeof(lastUpdated_buf))) {
        config->metadata.lastUpdated = lastUpdated_buf;
    } else {
        config->metadata.lastUpdated = "2025-08-21";
    }
    
    // Extract device name
    if (extract_string_value(json, "device_name", device_name_buf, sizeof(device_name_buf))) {
        config->device_name = device_name_buf;
    } else {
        config->device_name = "Guitar Controller";
    }
    
    // Extract GPIO pins
    config->UP = extract_string_value(json, "UP", up_buf, sizeof(up_buf)) ? up_buf : "GP2";
    config->DOWN = extract_string_value(json, "DOWN", down_buf, sizeof(down_buf)) ? down_buf : "GP3";
    config->LEFT = extract_string_value(json, "LEFT", left_buf, sizeof(left_buf)) ? left_buf : "GP4";
    config->RIGHT = extract_string_value(json, "RIGHT", right_buf, sizeof(right_buf)) ? right_buf : "GP5";
    config->GREEN_FRET = extract_string_value(json, "GREEN_FRET", green_fret_buf, sizeof(green_fret_buf)) ? green_fret_buf : "GP10";
    config->RED_FRET = extract_string_value(json, "RED_FRET", red_fret_buf, sizeof(red_fret_buf)) ? red_fret_buf : "GP11";
    config->YELLOW_FRET = extract_string_value(json, "YELLOW_FRET", yellow_fret_buf, sizeof(yellow_fret_buf)) ? yellow_fret_buf : "GP12";
    config->BLUE_FRET = extract_string_value(json, "BLUE_FRET", blue_fret_buf, sizeof(blue_fret_buf)) ? blue_fret_buf : "GP13";
    config->ORANGE_FRET = extract_string_value(json, "ORANGE_FRET", orange_fret_buf, sizeof(orange_fret_buf)) ? orange_fret_buf : "GP14";
    config->STRUM_UP = extract_string_value(json, "STRUM_UP", strum_up_buf, sizeof(strum_up_buf)) ? strum_up_buf : "GP7";
    config->STRUM_DOWN = extract_string_value(json, "STRUM_DOWN", strum_down_buf, sizeof(strum_down_buf)) ? strum_down_buf : "GP8";
    config->TILT = extract_string_value(json, "TILT", tilt_buf, sizeof(tilt_buf)) ? tilt_buf : "GP9";
    config->SELECT = extract_string_value(json, "SELECT", select_buf, sizeof(select_buf)) ? select_buf : "GP0";
    config->START = extract_string_value(json, "START", start_buf, sizeof(start_buf)) ? start_buf : "GP1";
    config->GUIDE = extract_string_value(json, "GUIDE", guide_buf, sizeof(guide_buf)) ? guide_buf : "GP6";
    config->WHAMMY = extract_string_value(json, "WHAMMY", whammy_buf, sizeof(whammy_buf)) ? whammy_buf : "GP27";
    config->neopixel_pin = extract_string_value(json, "neopixel_pin", neopixel_buf, sizeof(neopixel_buf)) ? neopixel_buf : "GP23";
    config->joystick_x_pin = extract_string_value(json, "joystick_x_pin", joystick_x_buf, sizeof(joystick_x_buf)) ? joystick_x_buf : "GP28";
    config->joystick_y_pin = extract_string_value(json, "joystick_y_pin", joystick_y_buf, sizeof(joystick_y_buf)) ? joystick_y_buf : "GP29";
    
    // Extract LED assignments
    int val;
    val = extract_int_value(json, "GREEN_FRET_led");
    config->GREEN_FRET_led = (val >= 0) ? (uint8_t)val : 6;
    
    val = extract_int_value(json, "RED_FRET_led");
    config->RED_FRET_led = (val >= 0) ? (uint8_t)val : 5;
    
    val = extract_int_value(json, "YELLOW_FRET_led");
    config->YELLOW_FRET_led = (val >= 0) ? (uint8_t)val : 4;
    
    val = extract_int_value(json, "BLUE_FRET_led");
    config->BLUE_FRET_led = (val >= 0) ? (uint8_t)val : 3;
    
    val = extract_int_value(json, "ORANGE_FRET_led");
    config->ORANGE_FRET_led = (val >= 0) ? (uint8_t)val : 2;
    
    val = extract_int_value(json, "STRUM_UP_led");
    config->STRUM_UP_led = (val >= 0) ? (uint8_t)val : 0;
    
    val = extract_int_value(json, "STRUM_DOWN_led");
    config->STRUM_DOWN_led = (val >= 0) ? (uint8_t)val : 1;
    
    // Extract settings
    config->hat_mode = extract_string_value(json, "hat_mode", hat_mode_buf, sizeof(hat_mode_buf)) ? hat_mode_buf : "dpad";
    
    float brightness = extract_float_value(json, "led_brightness");
    config->led_brightness = (brightness >= 0.0f) ? brightness : 1.0f;
    
    val = extract_int_value(json, "whammy_min");
    config->whammy_min = (val >= 0) ? (uint32_t)val : 500;
    
    val = extract_int_value(json, "whammy_max");
    config->whammy_max = (val >= 0) ? (uint32_t)val : 65000;
    
    config->whammy_reverse = extract_bool_value(json, "whammy_reverse", false);
    config->tilt_wave_enabled = extract_bool_value(json, "tilt_wave_enabled", true);
    
    // Extract LED color arrays (simplified - just use defaults for now)
    // TODO: Implement proper array parsing
    const char* default_led_colors[] = {
        "#FFFFFF", "#FFFFFF", "#B33E00", "#0000FF", 
        "#FFFF00", "#FF0000", "#00FF00"
    };
    const char* default_released_colors[] = {
        "#454545", "#454545", "#521C00", "#000091",
        "#696B00", "#8C0009", "#003D00"
    };
    
    for (int i = 0; i < 7; i++) {
        strncpy(led_colors[i], default_led_colors[i], sizeof(led_colors[i]) - 1);
        led_colors[i][sizeof(led_colors[i]) - 1] = '\0';
        config->led_color[i] = led_colors[i];
        
        strncpy(released_colors[i], default_released_colors[i], sizeof(released_colors[i]) - 1);
        released_colors[i][sizeof(released_colors[i]) - 1] = '\0';
        config->released_color[i] = released_colors[i];
    }
    
    return true;
}

bool config_generate_json(const config_t* config, char* buffer, uint32_t buffer_size) {
    // Generate BGG-compatible JSON format
    int written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"version\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"lastUpdated\": \"%s\",\n"
        "  \"device_name\": \"%s\",\n"
        "  \"UP\": \"%s\",\n"
        "  \"DOWN\": \"%s\",\n"
        "  \"LEFT\": \"%s\",\n"
        "  \"RIGHT\": \"%s\",\n"
        "  \"GREEN_FRET\": \"%s\",\n"
        "  \"RED_FRET\": \"%s\",\n"
        "  \"YELLOW_FRET\": \"%s\",\n"
        "  \"BLUE_FRET\": \"%s\",\n"
        "  \"ORANGE_FRET\": \"%s\",\n"
        "  \"STRUM_UP\": \"%s\",\n"
        "  \"STRUM_DOWN\": \"%s\",\n"
        "  \"TILT\": \"%s\",\n"
        "  \"SELECT\": \"%s\",\n"
        "  \"START\": \"%s\",\n"
        "  \"GUIDE\": \"%s\",\n"
        "  \"WHAMMY\": \"%s\",\n"
        "  \"neopixel_pin\": \"%s\",\n"
        "  \"joystick_x_pin\": \"%s\",\n"
        "  \"joystick_y_pin\": \"%s\",\n"
        "  \"GREEN_FRET_led\": %d,\n"
        "  \"RED_FRET_led\": %d,\n"
        "  \"YELLOW_FRET_led\": %d,\n"
        "  \"BLUE_FRET_led\": %d,\n"
        "  \"ORANGE_FRET_led\": %d,\n"
        "  \"STRUM_UP_led\": %d,\n"
        "  \"STRUM_DOWN_led\": %d,\n"
        "  \"hat_mode\": \"%s\",\n"
        "  \"led_brightness\": %.2f,\n"
        "  \"whammy_min\": %lu,\n"
        "  \"whammy_max\": %lu,\n"
        "  \"whammy_reverse\": %s,\n"
        "  \"tilt_wave_enabled\": %s,\n"
        "  \"led_color\": [\n"
        "    \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"\n"
        "  ],\n"
        "  \"released_color\": [\n"
        "    \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\"\n"
        "  ]\n"
        "}",
        config->metadata.version,
        config->metadata.description,
        config->metadata.lastUpdated,
        config->device_name,
        config->UP, config->DOWN, config->LEFT, config->RIGHT,
        config->GREEN_FRET, config->RED_FRET, config->YELLOW_FRET, config->BLUE_FRET, config->ORANGE_FRET,
        config->STRUM_UP, config->STRUM_DOWN, config->TILT,
        config->SELECT, config->START, config->GUIDE,
        config->WHAMMY, config->neopixel_pin,
        config->joystick_x_pin, config->joystick_y_pin,
        config->GREEN_FRET_led, config->RED_FRET_led, config->YELLOW_FRET_led,
        config->BLUE_FRET_led, config->ORANGE_FRET_led,
        config->STRUM_UP_led, config->STRUM_DOWN_led,
        config->hat_mode,
        config->led_brightness,
        config->whammy_min, config->whammy_max,
        config->whammy_reverse ? "true" : "false",
        config->tilt_wave_enabled ? "true" : "false",
        config->led_color[0], config->led_color[1], config->led_color[2],
        config->led_color[3], config->led_color[4], config->led_color[5], config->led_color[6],
        config->released_color[0], config->released_color[1], config->released_color[2],
        config->released_color[3], config->released_color[4], config->released_color[5], config->released_color[6]
    );
    
    return (written > 0 && written < (int)buffer_size);
}
