/**
 * @file fs_service.c
 * @brief BLE File System Service Implementation
 * 
 * Provides remote file browsing via BLE.
 */

#include "fs_service.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gatt/ble_svc_gatt.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static const char *TAG = "FS_SERVICE";

// ============================================
// Service and Characteristic UUIDs
// ============================================

// FS Service: 6E400020-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t fs_service_uuid = 
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x20, 0x00, 0x40, 0x6e);

// Path characteristic: 6E400021-B5A3-F393-E0A9-E50E24DCCA9E (Write)
static const ble_uuid128_t fs_path_chr_uuid = 
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x21, 0x00, 0x40, 0x6e);

// Data characteristic: 6E400022-B5A3-F393-E0A9-E50E24DCCA9E (Read)
static const ble_uuid128_t fs_data_chr_uuid = 
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x22, 0x00, 0x40, 0x6e);

// Control characteristic: 6E400023-B5A3-F393-E0A9-E50E24DCCA9E (Write)
static const ble_uuid128_t fs_control_chr_uuid = 
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x23, 0x00, 0x40, 0x6e);

// ============================================
// State Variables
// ============================================

static char current_path[256] = "/flash";  // Current path to list
static uint8_t listing_buffer[1024];       // Buffer for directory listing
static size_t listing_size = 0;            // Size of current listing

// File write state
static FILE *write_file = NULL;            // Current file being written
static char write_path[256] = {0};         // Path of file being written
static uint32_t write_expected_size = 0;   // Expected file size
static uint32_t write_received_size = 0;   // Bytes received so far
static bool write_in_progress = false;     // Write operation active

// ============================================
// Helper Functions
// ============================================

/**
 * @brief Create directory recursively
 */
static int create_directory(const char *path)
{
    // Try to create the directory
    if (mkdir(path, 0755) == 0) {
        ESP_LOGI(TAG, "✓ Created directory: %s", path);
        return 0;
    }
    
    // If it already exists, that's okay
    if (errno == EEXIST) {
        ESP_LOGI(TAG, "Directory already exists: %s", path);
        return 0;
    }
    
    // If parent doesn't exist, create parent first
    if (errno == ENOENT) {
        char parent[256];
        strncpy(parent, path, sizeof(parent) - 1);
        parent[sizeof(parent) - 1] = '\0';
        
        char *last_slash = strrchr(parent, '/');
        if (last_slash && last_slash != parent) {
            *last_slash = '\0';
            if (create_directory(parent) != 0) {
                return -1;
            }
            // Try again after creating parent
            if (mkdir(path, 0755) == 0) {
                ESP_LOGI(TAG, "✓ Created directory: %s", path);
                return 0;
            }
        }
    }
    
    ESP_LOGE(TAG, "Failed to create directory %s: %s", path, strerror(errno));
    return -1;
}

/**
 * @brief Start file write operation
 */
static int start_file_write(const char *path, uint32_t size)
{
    if (write_in_progress) {
        ESP_LOGE(TAG, "Write already in progress");
        return -1;
    }
    
    // Create parent directories if needed
    char parent[256];
    strncpy(parent, path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char *last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        create_directory(parent);
    }
    
    // Open file for writing
    write_file = fopen(path, "wb");
    if (!write_file) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path);
        return -1;
    }
    
    strncpy(write_path, path, sizeof(write_path) - 1);
    write_path[sizeof(write_path) - 1] = '\0';
    write_expected_size = size;
    write_received_size = 0;
    write_in_progress = true;
    
    ESP_LOGI(TAG, "📝 Started writing file: %s (%u bytes)", path, size);
    return 0;
}

/**
 * @brief Write data chunk to file
 */
static int write_file_chunk(const uint8_t *data, size_t len)
{
    if (!write_in_progress || !write_file) {
        ESP_LOGE(TAG, "No write in progress");
        return -1;
    }
    
    size_t written = fwrite(data, 1, len, write_file);
    if (written != len) {
        ESP_LOGE(TAG, "Write failed: expected %zu, wrote %zu", len, written);
        fclose(write_file);
        write_file = NULL;
        write_in_progress = false;
        return -1;
    }
    
    write_received_size += written;
    
    // Check if write is complete
    if (write_received_size >= write_expected_size) {
        fclose(write_file);
        write_file = NULL;
        write_in_progress = false;
        ESP_LOGI(TAG, "✅ File write complete: %s (%u bytes)", write_path, write_received_size);
        return 1;  // Complete
    }
    
    ESP_LOGD(TAG, "Write progress: %u/%u bytes (%.1f%%)", 
             write_received_size, write_expected_size,
             (write_received_size * 100.0) / write_expected_size);
    
    return 0;  // Continue
}

/**
 * @brief Abort file write operation
 */
static void abort_file_write(void)
{
    if (write_file) {
        fclose(write_file);
        write_file = NULL;
    }
    write_in_progress = false;
    write_received_size = 0;
    write_expected_size = 0;
    ESP_LOGW(TAG, "File write aborted");
}

/**
 * @brief Build directory listing in buffer
 * 
 * Format: [count][name1\0][size1][type1][name2\0][size2][type2]...
 * - count: 1 byte (number of entries)
 * - name: null-terminated string
 * - size: 4 bytes (little-endian)
 * - type: 1 byte (0=file, 1=directory)
 */
static int build_directory_listing(const char *path)
{
    // Special handling for root "/" - list VFS mount points
    if (strcmp(path, "/") == 0) {
        ESP_LOGI(TAG, "📂 Listing root VFS mount points");
        
        uint8_t *buf = listing_buffer;
        size_t offset = 1;
        uint8_t count = 0;
        
        // Known mount points on ESP32
        const char *mount_points[] = {
            "flash",
            "sdcard",
            "spiffs",
            NULL
        };
        
        for (int i = 0; mount_points[i] != NULL; i++) {
            char test_path[64];
            snprintf(test_path, sizeof(test_path), "/%s", mount_points[i]);
            
            struct stat st;
            if (stat(test_path, &st) != 0) {
                continue;  // Mount point doesn't exist
            }
            
            size_t name_len = strlen(mount_points[i]) + 1;
            if (offset + name_len + 5 > sizeof(listing_buffer)) {
                break;
            }
            
            // Write name
            memcpy(buf + offset, mount_points[i], name_len);
            offset += name_len;
            
            // Write size (0 for directories)
            buf[offset++] = 0;
            buf[offset++] = 0;
            buf[offset++] = 0;
            buf[offset++] = 0;
            
            // Write type (1 = directory)
            buf[offset++] = 1;
            
            count++;
            ESP_LOGI(TAG, "  📁 /%s", mount_points[i]);
        }
        
        buf[0] = count;
        listing_size = offset;
        ESP_LOGI(TAG, "✓ Listed %d mount points (%zu bytes)", count, listing_size);
        return 0;
    }
    
    // Normal directory listing
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return -1;
    }
    
    ESP_LOGI(TAG, "📂 Listing directory: %s", path);
    
    uint8_t *buf = listing_buffer;
    size_t offset = 1;  // Reserve first byte for count
    uint8_t count = 0;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Build full path for stat
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) {
            ESP_LOGW(TAG, "Failed to stat: %s", full_path);
            continue;
        }
        
        // Check if we have space in buffer
        size_t name_len = strlen(entry->d_name) + 1;  // +1 for null terminator
        if (offset + name_len + 4 + 1 > sizeof(listing_buffer)) {
            ESP_LOGW(TAG, "Listing buffer full, truncating");
            break;
        }
        
        // Write name (null-terminated)
        memcpy(buf + offset, entry->d_name, name_len);
        offset += name_len;
        
        // Write size (4 bytes, little-endian)
        uint32_t size = S_ISDIR(st.st_mode) ? 0 : st.st_size;
        buf[offset++] = (size >> 0) & 0xFF;
        buf[offset++] = (size >> 8) & 0xFF;
        buf[offset++] = (size >> 16) & 0xFF;
        buf[offset++] = (size >> 24) & 0xFF;
        
        // Write type (1 byte: 0=file, 1=directory)
        buf[offset++] = S_ISDIR(st.st_mode) ? 1 : 0;
        
        count++;
        
        ESP_LOGD(TAG, "  %s %s (%u bytes)", 
                 S_ISDIR(st.st_mode) ? "📁" : "📄", 
                 entry->d_name, size);
    }
    
    closedir(dir);
    
    // Write count at beginning
    buf[0] = count;
    listing_size = offset;
    
    ESP_LOGI(TAG, "✓ Listed %d entries (%zu bytes)", count, listing_size);
    
    return 0;
}

// ============================================
// Characteristic Access Callbacks
// ============================================

/**
 * @brief Path characteristic access (Write)
 * Client writes path to list
 */
static int fs_path_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Read path from client
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len >= sizeof(current_path)) {
            ESP_LOGE(TAG, "Path too long: %d bytes", len);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        
        ble_hs_mbuf_to_flat(ctxt->om, current_path, len, NULL);
        current_path[len] = '\0';
        
        ESP_LOGI(TAG, "📝 Path set to: %s", current_path);
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief Data characteristic access (Read/Write)
 * Read: Client reads directory listing
 * Write: Client writes file data chunks
 */
static int fs_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        if (listing_size > 0) {
            int rc = os_mbuf_append(ctxt->om, listing_buffer, listing_size);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to append listing data");
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            ESP_LOGD(TAG, "📤 Sent %zu bytes of listing", listing_size);
        } else {
            ESP_LOGD(TAG, "📤 No listing data available");
        }
        return 0;
    }
    
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Receive file data chunk
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t *data = malloc(len);
        if (!data) {
            ESP_LOGE(TAG, "Failed to allocate buffer for file data");
            abort_file_write();
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        
        ble_hs_mbuf_to_flat(ctxt->om, data, len, NULL);
        
        // Check if this is the file size (first 4 bytes)
        if (!write_in_progress && len == 4) {
            // This is the expected size message
            uint32_t expected_size = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
            ESP_LOGI(TAG, "📏 Expected file size: %u bytes", expected_size);
            write_expected_size = expected_size;
            free(data);
            return 0;
        }
        
        // Write data chunk
        int result = write_file_chunk(data, len);
        free(data);
        
        if (result < 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

/**
 * @brief Control characteristic access (Write)
 * Client writes commands
 */
static int fs_control_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t cmd;
        ble_hs_mbuf_to_flat(ctxt->om, &cmd, 1, NULL);
        
        switch (cmd) {
        case FS_CMD_LIST:
            ESP_LOGI(TAG, "🔍 LIST command received");
            if (build_directory_listing(current_path) != 0) {
                listing_size = 0;  // Clear on error
            }
            break;
            
        case FS_CMD_STAT:
            ESP_LOGI(TAG, "📊 STAT command received (not implemented)");
            break;
            
        case FS_CMD_DELETE:
            ESP_LOGI(TAG, "🗑️ DELETE command received");
            if (remove(current_path) == 0) {
                ESP_LOGI(TAG, "✓ Deleted: %s", current_path);
            } else {
                ESP_LOGE(TAG, "Failed to delete: %s", current_path);
            }
            break;
            
        case FS_CMD_WRITE:
            ESP_LOGI(TAG, "📝 WRITE command received for: %s", current_path);
            // Abort any existing write
            if (write_in_progress) {
                abort_file_write();
            }
            // Start new write (size will come via Data characteristic)
            if (start_file_write(current_path, 0) != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            break;
            
        case FS_CMD_MKDIR:
            ESP_LOGI(TAG, "📁 MKDIR command received for: %s", current_path);
            if (create_directory(current_path) != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown FS command: 0x%02x", cmd);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        
        return 0;
    }
    
    return BLE_ATT_ERR_UNLIKELY;
}

// ============================================
// Service Definition
// ============================================

static const struct ble_gatt_svc_def fs_service_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &fs_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                // Path characteristic (Write)
                .uuid = &fs_path_chr_uuid.u,
                .access_cb = fs_path_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // Data characteristic (Read/Write)
                .uuid = &fs_data_chr_uuid.u,
                .access_cb = fs_data_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                // Control characteristic (Write)
                .uuid = &fs_control_chr_uuid.u,
                .access_cb = fs_control_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                0, // End of characteristics
            }
        },
    },
    {
        0, // End of services
    },
};

// ============================================
// Public API
// ============================================

void fs_service_init(void)
{
    int rc = ble_gatts_count_cfg(fs_service_defs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count service config: %d", rc);
        return;
    }
    
    rc = ble_gatts_add_svcs(fs_service_defs);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add FS service: %d", rc);
        return;
    }
    
    ESP_LOGI(TAG, "✓ File System BLE service initialized");
}
