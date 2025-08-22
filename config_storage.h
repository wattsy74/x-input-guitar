#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Flash storage configuration
#define CONFIG_FLASH_OFFSET     (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)  // Last 4KB sector
#define CONFIG_MAGIC_HEADER     0x42474743  // "BGGC" in hex
#define CONFIG_JSON_MAX_SIZE    2048        // Maximum config JSON size
#define CONFIG_VERSION          1           // Config format version

// Config storage header
typedef struct {
    uint32_t magic;         // Magic number for validation
    uint32_t version;       // Config format version
    uint32_t json_size;     // Size of JSON data
    uint32_t checksum;      // CRC32 checksum of JSON data
    uint8_t reserved[16];   // Reserved for future use
} config_header_t;

// Complete config storage structure
typedef struct {
    config_header_t header;
    char json_data[CONFIG_JSON_MAX_SIZE];
} config_storage_t;

// Function prototypes
bool config_storage_init(void);
bool config_storage_load_from_flash(config_t* config);
bool config_storage_save_to_flash(const char* json_data, uint32_t json_size);
bool config_storage_get_json(char* buffer, uint32_t buffer_size, uint32_t* actual_size);
bool config_storage_is_valid(void);
void config_storage_format(void);
uint32_t config_storage_calculate_crc32(const void* data, uint32_t size);

// Parse JSON config into config_t structure
bool config_parse_json(const char* json, config_t* config);

// Generate JSON from config_t structure  
bool config_generate_json(const config_t* config, char* buffer, uint32_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_STORAGE_H
