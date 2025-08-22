#ifndef FILE_EMULATION_H
#define FILE_EMULATION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// File emulation system - mimics CircuitPython file I/O for BGG app compatibility
#define MAX_FILENAME_LENGTH     32
#define MAX_FILE_CONTENT        8192
#define MAX_VIRTUAL_FILES       4

// Virtual file structure
typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    char content[MAX_FILE_CONTENT];
    uint32_t size;
    bool exists;
} virtual_file_t;

// File emulation functions
bool file_emu_init(void);
bool file_emu_create_default_files(void);

// File operations (matches CircuitPython behavior)
bool file_emu_exists(const char* filename);
bool file_emu_read(const char* filename, char* buffer, uint32_t buffer_size, uint32_t* actual_size);
bool file_emu_write(const char* filename, const char* content, uint32_t size);

// Serial command processing (matches BGG app expectations)
void file_emu_process_serial_command(const char* command);

// CDC communication
void file_emu_send_response(const char* response);
void file_emu_send_file_content(const char* filename);

#ifdef __cplusplus
}
#endif

#endif // FILE_EMULATION_H
