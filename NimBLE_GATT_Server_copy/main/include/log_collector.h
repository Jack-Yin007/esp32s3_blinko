/**
 * @file log_collector.h
 * @brief BLE Log Collector - Circular Buffer in RAM
 * 
 * Collects all ESP_LOG output to a circular buffer for remote debugging via BLE.
 * 
 * Features:
 *   - 32KB RAM circular buffer (no Flash wear)
 *   - Retains ~10-20 minutes of logs (depending on log rate)
 *   - Real-time streaming via BLE notifications
 *   - Batch download via BLE read
 *   - Thread-safe (mutex protected)
 * 
 * Usage:
 *   1. Call log_collector_init() at startup
 *   2. Call log_collector_install_hook() to capture all logs
 *   3. Use BLE service to read/stream logs
 */

#ifndef LOG_COLLECTOR_H
#define LOG_COLLECTOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * Configuration
 */
#define LOG_BUFFER_SIZE      (16 * 1024)  // 16KB circular buffer in RAM (reduced from 32KB)
#define LOG_LINE_MAX_LEN     256          // Max length per log line

/**
 * Log level (matches esp_log_level_t)
 * Renamed to avoid conflict with NimBLE's LOG_LEVEL_NONE macro
 */
typedef enum {
    LOG_COLLECT_NONE = 0,
    LOG_COLLECT_ERROR,
    LOG_COLLECT_WARN,
    LOG_COLLECT_INFO,
    LOG_COLLECT_DEBUG,
    LOG_COLLECT_VERBOSE
} log_level_t;

/**
 * Log statistics
 */
typedef struct {
    size_t total_written;     // Total bytes written (lifetime)
    size_t buffer_size;       // Buffer size (bytes)
    size_t current_size;      // Current readable size (bytes)
    bool buffer_wrapped;      // Whether buffer has wrapped around
    uint32_t log_count;       // Total log entries (lifetime)
    uint32_t dropped_count;   // Dropped entries (if any)
} log_stats_t;

/**
 * @brief Initialize log collector
 * 
 * Allocates circular buffer and initializes mutex.
 * Must be called before any other log_collector functions.
 */
void log_collector_init(void);

/**
 * @brief Install ESP-IDF log hook to capture all logs
 * 
 * Intercepts all ESP_LOGI/LOGW/LOGE/etc calls and copies to circular buffer.
 * Original console output is preserved.
 */
void log_collector_install_hook(void);

/**
 * @brief Read logs from circular buffer
 * 
 * Reads up to max_len bytes starting from offset.
 * Used for batch download via BLE.
 * 
 * @param offset Starting offset (0 = oldest available log)
 * @param buffer Output buffer
 * @param max_len Maximum bytes to read
 * @return Actual bytes read (0 = end of logs)
 * 
 * Example - Read all logs in chunks:
 *   size_t offset = 0;
 *   uint8_t chunk[512];
 *   while (true) {
 *       size_t read = log_collector_read(offset, chunk, sizeof(chunk));
 *       if (read == 0) break;
 *       ble_send(chunk, read);
 *       offset += read;
 *   }
 */
size_t log_collector_read(size_t offset, uint8_t *buffer, size_t max_len);

/**
 * @brief Get current readable log size
 * @return Number of bytes available to read
 */
size_t log_collector_get_size(void);

/**
 * @brief Get snapshot of buffer state (write position and wrapped flag)
 * 
 * Used for freezing the read endpoint when starting a download.
 * 
 * @param out_write_pos Output: current write position
 * @param out_wrapped Output: whether buffer has wrapped
 */
void log_collector_get_snapshot(size_t *out_write_pos, bool *out_wrapped);

/**
 * @brief Read logs using snapshot state (for frozen reads)
 * 
 * Similar to log_collector_read(), but uses provided snapshot state
 * instead of current buffer state. This ensures consistent reads even
 * when buffer continues to be written during transfer.
 * 
 * @param offset Starting offset (0 = oldest available log in snapshot)
 * @param buffer Output buffer
 * @param max_len Maximum bytes to read
 * @param snapshot_size Size of buffer at snapshot moment
 * @param snapshot_write_pos Write position at snapshot moment
 * @param snapshot_wrapped Whether buffer was wrapped at snapshot moment
 * @return Actual bytes read (0 = end of logs)
 */
size_t log_collector_read_snapshot(size_t offset, uint8_t *buffer, size_t max_len,
                                   size_t snapshot_size, size_t snapshot_write_pos, 
                                   bool snapshot_wrapped);

/**
 * @brief Get statistics
 * @return Pointer to statistics structure (read-only)
 */
const log_stats_t* log_collector_get_stats(void);

/**
 * @brief Clear log buffer
 * 
 * Resets write position and clears buffer.
 * Statistics counters are preserved.
 */
void log_collector_clear(void);

/**
 * @brief Set minimum log level filter
 * 
 * Only logs at this level or higher will be captured.
 * Default: LOG_LEVEL_INFO
 * 
 * @param level Minimum level to capture
 */
void log_collector_set_level(log_level_t level);

/**
 * @brief Enable/disable real-time streaming
 * 
 * When enabled, new logs trigger BLE notification immediately.
 * 
 * @param enable true to enable streaming
 * @param conn_handle BLE connection handle (for notifications)
 * @param attr_handle BLE attribute handle (for notifications)
 */
void log_collector_set_streaming(bool enable, uint16_t conn_handle, uint16_t attr_handle);

#endif // LOG_COLLECTOR_H
