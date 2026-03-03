/**
 * @file fs_service.h
 * @brief BLE File System Service - Browse files via BLE
 * 
 * Allows remote clients to:
 * - List files and directories
 * - Get file sizes and types
 * - Navigate file system hierarchy
 * 
 * Service UUID: 0x6E400020-B5A3-F393-E0A9-E50E24DCCA9E
 * 
 * Characteristics:
 * - Path (0x6E400021): Write path to list
 * - Data (0x6E400022): Read directory listing
 * - Control (0x6E400023): Write commands
 */

#ifndef FS_SERVICE_H
#define FS_SERVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize file system BLE service
 * 
 * Registers GATT service and characteristics.
 * Must be called after BLE stack initialization.
 */
void fs_service_init(void);

/**
 * @brief Commands for FS Control characteristic
 */
typedef enum {
    FS_CMD_LIST = 0x01,      // List directory
    FS_CMD_STAT = 0x02,      // Get file stats
    FS_CMD_DELETE = 0x03,    // Delete file
    FS_CMD_WRITE = 0x04,     // Write file (upload)
    FS_CMD_MKDIR = 0x05,     // Create directory
} fs_command_t;

#endif // FS_SERVICE_H
