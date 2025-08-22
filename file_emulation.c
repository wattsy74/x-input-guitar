#include "file_emulation.h"
#include "config_storage.h"
#include "config.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Virtual file storage
static virtual_file_t virtual_files[MAX_VIRTUAL_FILES];
static char line_buffer[512];
static bool writing_file = false;
static char write_filename[MAX_FILENAME_LENGTH];
static char write_buffer[MAX_FILE_CONTENT];
static uint32_t write_pos = 0;

// Default file contents using BGG Windows App format
static const char* default_config_json = "{\n"
"    \"_metadata\": {\n"
"        \"version\": \"4.0.0\",\n"
"        \"description\": \"BumbleGum Guitar Controller Configuration\",\n"
"        \"lastUpdated\": \"2025-08-21\"\n"
"    },\n"
"    \"device_name\": \"Guitar Controller\",\n"
"    \"UP\": \"GP2\",\n"
"    \"DOWN\": \"GP3\",\n"
"    \"LEFT\": \"GP4\",\n"
"    \"RIGHT\": \"GP5\",\n"
"    \"GREEN_FRET\": \"GP10\",\n"
"    \"GREEN_FRET_led\": 6,\n"
"    \"RED_FRET\": \"GP11\",\n"
"    \"RED_FRET_led\": 5,\n"
"    \"YELLOW_FRET\": \"GP12\",\n"
"    \"YELLOW_FRET_led\": 4,\n"
"    \"BLUE_FRET\": \"GP13\",\n"
"    \"BLUE_FRET_led\": 3,\n"
"    \"ORANGE_FRET\": \"GP14\",\n"
"    \"ORANGE_FRET_led\": 2,\n"
"    \"STRUM_UP\": \"GP7\",\n"
"    \"STRUM_UP_led\": 0,\n"
"    \"STRUM_DOWN\": \"GP8\",\n"
"    \"STRUM_DOWN_led\": 1,\n"
"    \"TILT\": \"GP9\",\n"
"    \"SELECT\": \"GP0\",\n"
"    \"START\": \"GP1\",\n"
"    \"GUIDE\": \"GP6\",\n"
"    \"WHAMMY\": \"GP27\",\n"
"    \"neopixel_pin\": \"GP23\",\n"
"    \"joystick_x_pin\": \"GP28\",\n"
"    \"joystick_y_pin\": \"GP29\",\n"
"    \"hat_mode\": \"dpad\",\n"
"    \"led_brightness\": 1.0,\n"
"    \"whammy_min\": 500,\n"
"    \"whammy_max\": 65000,\n"
"    \"whammy_reverse\": false,\n"
"    \"tilt_wave_enabled\": true,\n"
"    \"led_color\": [\n"
"        \"#FFFFFF\",\n"
"        \"#FFFFFF\",\n"
"        \"#B33E00\",\n"
"        \"#0000FF\",\n"
"        \"#FFFF00\",\n"
"        \"#FF0000\",\n"
"        \"#00FF00\"\n"
"    ],\n"
"    \"released_color\": [\n"
"        \"#454545\",\n"
"        \"#454545\",\n"
"        \"#521C00\",\n"
"        \"#000091\",\n"
"        \"#696B00\",\n"
"        \"#8C0009\",\n"
"        \"#003D00\"\n"
"    ]\n"
"}";

static const char* default_presets_json = "{\n"
"  \"_metadata\": {\n"
"    \"version\": \"4.0\",\n"
"    \"device_type\": \"bgg_xinput\",\n"
"    \"created\": \"2025-08-21\"\n"
"  },\n"
"  \"presets\": {\n"
"    \"default\": {\n"
"      \"name\": \"Default Colors\",\n"
"      \"strum-up-active\": \"#ffffff\",\n"
"      \"strum-down-active\": \"#ffffff\",\n"
"      \"orange-fret-pressed\": \"#ff8000\",\n"
"      \"blue-fret-pressed\": \"#0080ff\",\n"
"      \"yellow-fret-pressed\": \"#ffff00\",\n"
"      \"red-fret-pressed\": \"#ff0000\",\n"
"      \"green-fret-pressed\": \"#00ff00\"\n"
"    }\n"
"  }\n"
"}";

static const char* default_user_presets_json = "{\n"
"  \"user_presets\": {}\n"
"}";

bool file_emu_init(void) {
    // Initialize virtual files array
    memset(virtual_files, 0, sizeof(virtual_files));
    
    // Create default files
    return file_emu_create_default_files();
}

bool file_emu_create_default_files(void) {
    // Initialize config.json - try to load from flash first
    strncpy(virtual_files[0].filename, "config.json", MAX_FILENAME_LENGTH - 1);
    
    // Try to get the current config from flash
    char config_buffer[MAX_FILE_CONTENT];
    uint32_t config_size;
    
    if (config_storage_get_json(config_buffer, sizeof(config_buffer), &config_size)) {
        // Use config from flash
        memcpy(virtual_files[0].content, config_buffer, config_size);
        virtual_files[0].size = config_size;
        printf("File emulation: Loaded config.json from flash (%lu bytes)\n", config_size);
    } else {
        // Fall back to default config
        strncpy(virtual_files[0].content, default_config_json, MAX_FILE_CONTENT - 1);
        virtual_files[0].size = strlen(default_config_json);
        printf("File emulation: Using default config.json\n");
    }
    virtual_files[0].exists = true;
    
    // Initialize presets.json
    strncpy(virtual_files[1].filename, "presets.json", MAX_FILENAME_LENGTH - 1);
    strncpy(virtual_files[1].content, default_presets_json, MAX_FILE_CONTENT - 1);
    virtual_files[1].size = strlen(default_presets_json);
    virtual_files[1].exists = true;
    
    // Initialize user_presets.json
    strncpy(virtual_files[2].filename, "user_presets.json", MAX_FILENAME_LENGTH - 1);
    strncpy(virtual_files[2].content, default_user_presets_json, MAX_FILE_CONTENT - 1);
    virtual_files[2].size = strlen(default_user_presets_json);
    virtual_files[2].exists = true;
    
    printf("File emulation: Created virtual files\n");
    return true;
}

bool file_emu_exists(const char* filename) {
    for (int i = 0; i < MAX_VIRTUAL_FILES; i++) {
        if (virtual_files[i].exists && strcmp(virtual_files[i].filename, filename) == 0) {
            return true;
        }
    }
    return false;
}

bool file_emu_read(const char* filename, char* buffer, uint32_t buffer_size, uint32_t* actual_size) {
    for (int i = 0; i < MAX_VIRTUAL_FILES; i++) {
        if (virtual_files[i].exists && strcmp(virtual_files[i].filename, filename) == 0) {
            uint32_t copy_size = virtual_files[i].size;
            if (copy_size >= buffer_size) {
                copy_size = buffer_size - 1;
            }
            
            memcpy(buffer, virtual_files[i].content, copy_size);
            buffer[copy_size] = '\0';
            
            if (actual_size) {
                *actual_size = copy_size;
            }
            
            return true;
        }
    }
    return false;
}

bool file_emu_write(const char* filename, const char* content, uint32_t size) {
    // Find existing file or create new one
    int file_index = -1;
    
    // Look for existing file
    for (int i = 0; i < MAX_VIRTUAL_FILES; i++) {
        if (virtual_files[i].exists && strcmp(virtual_files[i].filename, filename) == 0) {
            file_index = i;
            break;
        }
    }
    
    // If not found, find empty slot
    if (file_index == -1) {
        for (int i = 0; i < MAX_VIRTUAL_FILES; i++) {
            if (!virtual_files[i].exists) {
                file_index = i;
                break;
            }
        }
    }
    
    if (file_index == -1) {
        printf("File emulation: No space for file %s\n", filename);
        return false;
    }
    
    if (size >= MAX_FILE_CONTENT) {
        printf("File emulation: File %s too large (%lu bytes)\n", filename, size);
        return false;
    }
    
    // Write file data
    strncpy(virtual_files[file_index].filename, filename, MAX_FILENAME_LENGTH - 1);
    memcpy(virtual_files[file_index].content, content, size);
    virtual_files[file_index].content[size] = '\0';
    virtual_files[file_index].size = size;
    virtual_files[file_index].exists = true;
    
    printf("File emulation: Wrote %s (%lu bytes)\n", filename, size);
    
    // If this is config.json, update the runtime config
    if (strcmp(filename, "config.json") == 0) {
        printf("File emulation: Config.json updated, applying new configuration...\n");
        
        // Update the runtime configuration
        if (config_update_from_json(content)) {
            printf("File emulation: Configuration successfully updated and saved to flash\n");
        } else {
            printf("File emulation: ERROR - Failed to update configuration\n");
            return false;
        }
    }
    
    return true;
}

void file_emu_send_response(const char* response) {
    if (tud_cdc_connected()) {
        tud_cdc_write_str(response);
        tud_cdc_write_flush();
    }
}

void file_emu_send_file_content(const char* filename) {
    for (int i = 0; i < MAX_VIRTUAL_FILES; i++) {
        if (virtual_files[i].exists && strcmp(virtual_files[i].filename, filename) == 0) {
            if (tud_cdc_connected()) {
                // Send START marker
                char start_marker[64];
                snprintf(start_marker, sizeof(start_marker), "START_%s\n", filename);
                tud_cdc_write_str(start_marker);
                
                // Send file content
                tud_cdc_write(virtual_files[i].content, virtual_files[i].size);
                
                // Send END marker
                char end_marker[64];
                snprintf(end_marker, sizeof(end_marker), "\nEND_%s\n", filename);
                tud_cdc_write_str(end_marker);
                
                tud_cdc_write_flush();
                
                printf("File emulation: Sent file %s (%lu bytes)\n", filename, virtual_files[i].size);
            }
            return;
        }
    }
    
    // File not found
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg), "ERROR: File not found: %s\n", filename);
    file_emu_send_response(error_msg);
}

void file_emu_process_serial_command(const char* command) {
    printf("File emulation: Processing command: %s\n", command);
    
    // Handle READFILE command
    if (strncmp(command, "READFILE:", 9) == 0) {
        const char* filename = command + 9;
        printf("File emulation: Read request for %s\n", filename);
        file_emu_send_file_content(filename);
        return;
    }
    
    // Handle WRITEFILE command
    if (strncmp(command, "WRITEFILE:", 10) == 0) {
        const char* filename = command + 10;
        printf("File emulation: Write request for %s\n", filename);
        
        // Start write mode
        writing_file = true;
        strncpy(write_filename, filename, MAX_FILENAME_LENGTH - 1);
        write_filename[MAX_FILENAME_LENGTH - 1] = '\0';
        write_pos = 0;
        memset(write_buffer, 0, sizeof(write_buffer));
        
        file_emu_send_response("READY\n");
        return;
    }
    
    // Handle file content during write mode
    if (writing_file) {
        // Check for END marker
        if (strstr(command, "END_FILE") != NULL) {
            // Finish writing
            writing_file = false;
            
            if (file_emu_write(write_filename, write_buffer, write_pos)) {
                file_emu_send_response("FILE_WRITTEN\n");
            } else {
                file_emu_send_response("ERROR: Write failed\n");
            }
            return;
        }
        
        // Append data to write buffer
        uint32_t cmd_len = strlen(command);
        if (write_pos + cmd_len < MAX_FILE_CONTENT - 1) {
            memcpy(write_buffer + write_pos, command, cmd_len);
            write_pos += cmd_len;
            // Add newline if not present
            if (cmd_len > 0 && command[cmd_len - 1] != '\n') {
                write_buffer[write_pos++] = '\n';
            }
        }
        return;
    }
    
    // Handle other commands
    if (strcmp(command, "version") == 0) {
        file_emu_send_response("BGG XInput Firmware v1.0\n");
        return;
    }
    
    // Unknown command
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg), "ERROR: Unknown command: %s\n", command);
    file_emu_send_response(error_msg);
}
