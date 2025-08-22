#include "command_interface.h"
#include "usb_mode_storage.h"
#include "config.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include <stdio.h>
#include <string.h>

//--------------------------------------------------------------------+
// COMMAND PROCESSING
//--------------------------------------------------------------------+

#define CMD_BUFFER_SIZE 128
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_buffer_pos = 0;

static void send_response(const char* response) {
    printf("%s\n", response);
}

static void send_json_response(const char* key, const char* value) {
    printf("{\"status\":\"ok\",\"%s\":\"%s\"}\n", key, value);
}

static void send_error_response(const char* error) {
    printf("{\"status\":\"error\",\"message\":\"%s\"}\n", error);
}

static void process_command(const char* cmd) {
    // Trim whitespace
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    
    if (strncmp(cmd, "GET_MODE", 8) == 0) {
        usb_mode_enum_t current_mode = usb_mode_storage_get();
        send_json_response("mode", usb_mode_to_string(current_mode));
        
    } else if (strncmp(cmd, "SET_MODE ", 9) == 0) {
        const char* mode_str = cmd + 9;
        usb_mode_enum_t new_mode = usb_mode_from_string(mode_str);
        
        if (usb_mode_storage_set(new_mode)) {
            send_json_response("mode", usb_mode_to_string(new_mode));
            send_response("# Device restart required - send RESTART command");
        } else {
            send_error_response("Failed to save USB mode");
        }
        
    } else if (strncmp(cmd, "GET_CONFIG", 10) == 0) {
        // Send current pin configuration as JSON
        printf("{\"status\":\"ok\",\"config\":{\n");
        printf("  \"usb_mode\":\"%s\",\n", usb_mode_to_string(usb_mode_storage_get()));
        printf("  \"led_brightness\":%.2f,\n", device_config.led_brightness);
        printf("  \"button_pins\":{\n");
        printf("    \"green\":%d,\"red\":%d,\"yellow\":%d,\"blue\":%d,\"orange\":%d,\n",
               device_config.button_pins.green, device_config.button_pins.red,
               device_config.button_pins.yellow, device_config.button_pins.blue,
               device_config.button_pins.orange);
        printf("    \"strum_up\":%d,\"strum_down\":%d,\n",
               device_config.button_pins.strum_up, device_config.button_pins.strum_down);
        printf("    \"select\":%d,\"start\":%d,\n",
               device_config.button_pins.select, device_config.button_pins.start);
        printf("    \"dpad_up\":%d,\"dpad_down\":%d,\"dpad_left\":%d,\"dpad_right\":%d\n",
               device_config.button_pins.dpad_up, device_config.button_pins.dpad_down,
               device_config.button_pins.dpad_left, device_config.button_pins.dpad_right);
        printf("  },\n");
        printf("  \"whammy_pin\":%d,\n", device_config.whammy_pin);
        printf("  \"led_pin\":%d,\"led_count\":%d\n", device_config.led_pin, device_config.led_count);
        printf("}}\n");
        
    } else if (strncmp(cmd, "RESTART", 7) == 0) {
        send_response("# Restarting device...");
        sleep_ms(100);  // Give time for response to send
        reset_usb_boot(0, 0);  // Restart in USB boot mode
        
    } else if (strncmp(cmd, "VERSION", 7) == 0) {
        printf("{\"status\":\"ok\",\"version\":{\n");
        printf("  \"firmware\":\"BGG Guitar Hero Controller\",\n");
        printf("  \"version\":\"2.0\",\n");
        printf("  \"build_date\":\"%s\",\n", __DATE__);
        printf("  \"features\":[\"xinput\",\"hid\",\"config_switching\"]\n");
        printf("}}\n");
        
    } else if (strncmp(cmd, "HELP", 4) == 0) {
        send_response("# Available commands:");
        send_response("# GET_MODE - Get current USB mode");
        send_response("# SET_MODE <xinput|hid> - Set USB mode");
        send_response("# GET_CONFIG - Get device configuration");
        send_response("# RESTART - Restart device");
        send_response("# VERSION - Get firmware info");
        send_response("# Boot combos: Green=XInput, Red=HID");
        
    } else {
        send_error_response("Unknown command. Send HELP for available commands.");
    }
}

//--------------------------------------------------------------------+
// PUBLIC FUNCTIONS
//--------------------------------------------------------------------+

void command_interface_init(void) {
    // UART is already initialized by pico_stdlib
    cmd_buffer_pos = 0;
    printf("# BGG Guitar Hero Controller Command Interface Ready\n");
    printf("# Send HELP for available commands\n");
    printf("# Current mode: %s\n", usb_mode_to_string(usb_mode_storage_get()));
}

void command_interface_task(void) {
    // Check for incoming characters
    int c = getchar_timeout_us(0);  // Non-blocking read
    
    if (c != PICO_ERROR_TIMEOUT && c >= 0) {
        if (c == '\r' || c == '\n') {
            // End of command
            if (cmd_buffer_pos > 0) {
                cmd_buffer[cmd_buffer_pos] = '\0';
                process_command(cmd_buffer);
                cmd_buffer_pos = 0;
            }
        } else if (cmd_buffer_pos < CMD_BUFFER_SIZE - 1) {
            // Add character to buffer
            cmd_buffer[cmd_buffer_pos++] = (char)c;
        }
    }
}
