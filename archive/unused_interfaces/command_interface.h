#ifndef COMMAND_INTERFACE_H
#define COMMAND_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Command interface for USB mode switching and configuration
// Uses UART for communication with configuration app

// Initialize command interface
void command_interface_init(void);

// Process incoming commands (call this in main loop)
void command_interface_task(void);

// Available commands:
// "GET_MODE" - Returns current USB mode
// "SET_MODE xinput" - Sets XInput mode
// "SET_MODE hid" - Sets HID mode  
// "GET_CONFIG" - Returns current pin configuration
// "RESTART" - Restarts the device with new USB mode
// "VERSION" - Returns firmware version info

#ifdef __cplusplus
}
#endif

#endif // COMMAND_INTERFACE_H
