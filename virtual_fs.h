#ifndef VIRTUAL_FS_H
#define VIRTUAL_FS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Virtual filesystem configuration
#define VFS_SECTOR_SIZE       512
#define VFS_SECTOR_COUNT      64      // 32KB total virtual disk
#define VFS_DISK_SIZE         (VFS_SECTOR_COUNT * VFS_SECTOR_SIZE)

// File system layout
#define VFS_MAX_FILES         8
#define VFS_FILENAME_LENGTH   32
#define VFS_FILE_MAX_SIZE     8192

// File allocation table entry
typedef struct {
    char filename[VFS_FILENAME_LENGTH];
    uint32_t start_sector;
    uint32_t size_bytes;
    bool in_use;
    uint8_t reserved[8];
} vfs_file_entry_t;

// Virtual filesystem header (stored in first sector)
typedef struct {
    char signature[8];           // "BGGVFS\0\0"
    uint32_t version;
    uint32_t total_sectors;
    uint32_t file_count;
    vfs_file_entry_t files[VFS_MAX_FILES];
    uint8_t padding[VFS_SECTOR_SIZE - 16 - (VFS_MAX_FILES * sizeof(vfs_file_entry_t))];
} vfs_header_t;

// Function prototypes
bool vfs_init(void);
bool vfs_format(void);
bool vfs_is_mounted(void);

// File operations
bool vfs_file_exists(const char* filename);
bool vfs_read_file(const char* filename, uint8_t* buffer, uint32_t buffer_size, uint32_t* bytes_read);
bool vfs_write_file(const char* filename, const uint8_t* data, uint32_t size);
bool vfs_delete_file(const char* filename);

// Directory operations
uint32_t vfs_get_file_count(void);
bool vfs_get_file_info(uint32_t index, char* filename, uint32_t* size);

// MSC interface
bool vfs_read_sector(uint32_t sector, uint8_t* buffer);
bool vfs_write_sector(uint32_t sector, const uint8_t* buffer);

// Default file creation
bool vfs_create_default_files(void);

#ifdef __cplusplus
}
#endif

#endif // VIRTUAL_FS_H
