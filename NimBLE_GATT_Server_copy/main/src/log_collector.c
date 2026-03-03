/**
 * @file log_collector.c
 * @brief BLE Log Collector Implementation
 */

#include "log_collector.h"
#include "drivers/gatt_svc.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "LogCollector"

// Circular buffer (in RAM)
static uint8_t log_buffer[LOG_BUFFER_SIZE];
static size_t log_write_pos = 0;
static bool buffer_wrapped = false;

// Statistics
static log_stats_t stats = {0};

// Configuration
static log_level_t min_log_level = LOG_COLLECT_INFO;

// Real-time streaming
static volatile bool streaming_enabled = false;

// Thread safety
static SemaphoreHandle_t log_mutex = NULL;

// Prevent recursion in log hook
static volatile bool in_hook = false;

// Original vprintf function (before hooking)
static vprintf_like_t original_vprintf = NULL;

// ============================================
// Initialization
// ============================================

void log_collector_init(void)
{
    // Create mutex
    log_mutex = xSemaphoreCreateMutex();
    if (!log_mutex) {
        printf("ERROR: Failed to create log mutex!\n");
        return;
    }
    
    // Initialize buffer
    memset(log_buffer, 0, LOG_BUFFER_SIZE);
    log_write_pos = 0;
    buffer_wrapped = false;
    
    // Initialize stats
    memset(&stats, 0, sizeof(stats));
    stats.buffer_size = LOG_BUFFER_SIZE;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Log Collector Initialized");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Buffer size:  %d KB (RAM)", LOG_BUFFER_SIZE / 1024);
    ESP_LOGI(TAG, "Storage:      Memory only (no Flash)");
    ESP_LOGI(TAG, "Retention:    ~5-10 minutes (circular)");
    ESP_LOGI(TAG, "Flash impact: NONE (zero risk)");
    ESP_LOGI(TAG, "========================================");
}

// ============================================
// Internal write function
// ============================================

static void internal_write(const char *str, size_t len)
{
    if (!log_mutex || len == 0) return;
    
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    
    // Write to circular buffer
    for (size_t i = 0; i < len; i++) {
        log_buffer[log_write_pos] = str[i];
        log_write_pos = (log_write_pos + 1) % LOG_BUFFER_SIZE;
        stats.total_written++;
        
        // Check if wrapping
        if (log_write_pos == 0 && !buffer_wrapped) {
            buffer_wrapped = true;
        }
    }
    
    stats.log_count++;
    
    xSemaphoreGive(log_mutex);
}

// ============================================
// ESP-IDF log hook
// ============================================

static int log_vprintf_hook(const char *format, va_list args)
{
    // Prevent recursion
    if (in_hook) {
        if (original_vprintf) {
            return original_vprintf(format, args);
        }
        return vprintf(format, args);
    }
    
    in_hook = true;
    
    char buffer[LOG_LINE_MAX_LEN];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    
    if (len > 0 && len < (int)sizeof(buffer)) {
        internal_write(buffer, len);
        
        // Send via BLE if streaming enabled
        // Only skip "GATT procedure initiated: notify" to prevent infinite loop
        if (streaming_enabled && strstr(buffer, "GATT procedure initiated: notify") == NULL) {
            log_send_notification((const uint8_t *)buffer, len);
        }
    }
    
    in_hook = false;
    
    // Continue to original output (console)
    if (original_vprintf) {
        return original_vprintf(format, args);
    } else {
        return vprintf(format, args);
    }
}

void log_collector_install_hook(void)
{
    original_vprintf = esp_log_set_vprintf(log_vprintf_hook);
    ESP_LOGI(TAG, "✓ Log hook installed (capturing all logs)");
}

// ============================================
// UTF-8 helper: check if byte is UTF-8 continuation byte
// ============================================
static inline bool is_utf8_continuation(uint8_t byte)
{
    return (byte & 0xC0) == 0x80;
}

// ============================================
// Read interface (with UTF-8 boundary protection)
// ============================================

size_t log_collector_read(size_t offset, uint8_t *buffer, size_t max_len)
{
    if (!log_mutex || !buffer || max_len == 0) return 0;
    
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    
    size_t available;
    size_t start_pos;
    
    // 获取当前写入位置（快照时刻的位置）
    size_t current_write_pos = log_write_pos;
    bool current_wrapped = buffer_wrapped;
    
    if (current_wrapped) {
        // 缓冲区已满，包含完整的 LOG_BUFFER_SIZE 字节
        // 读取顺序：从 write_pos 开始（最旧）到 write_pos-1（最新）
        // 但这样读到的是旧数据！应该读最新的数据
        // 
        // 正确做法：读取最新的 LOG_BUFFER_SIZE 字节
        // 最新的一个字节在 (write_pos - 1 + BUFFER_SIZE) % BUFFER_SIZE
        // 往前推 BUFFER_SIZE 字节，起点就是 write_pos（刚好绕一圈）
        available = LOG_BUFFER_SIZE;
        start_pos = current_write_pos;  // 从最旧位置开始，这样能读到完整的循环数据
    } else {
        // 缓冲区未满，数据从 0 到 write_pos
        available = current_write_pos;
        start_pos = 0;
    }
    
    // Check offset
    if (offset >= available) {
        xSemaphoreGive(log_mutex);
        return 0;
    }
    
    // Calculate read length
    size_t to_read = available - offset;
    if (to_read > max_len) {
        to_read = max_len;
    }
    
    // Calculate read position
    size_t read_pos = (start_pos + offset) % LOG_BUFFER_SIZE;
    
    // Skip broken UTF-8 continuation bytes at start (up to 3 bytes)
    // This handles case where circular buffer wrapped mid-character
    for (int skip = 0; skip < 3 && to_read > 0; skip++) {
        if (!is_utf8_continuation(log_buffer[read_pos])) {
            break;  // Found valid start byte
        }
        // Skip this continuation byte
        read_pos = (read_pos + 1) % LOG_BUFFER_SIZE;
        to_read--;
        offset++;
    }
    
    // Read data
    size_t copied = 0;
    for (size_t i = 0; i < to_read; i++) {
        buffer[copied++] = log_buffer[read_pos];
        read_pos = (read_pos + 1) % LOG_BUFFER_SIZE;
    }
    
    xSemaphoreGive(log_mutex);
    return copied;
}

// ============================================
// Query interface
// ============================================

size_t log_collector_get_size(void)
{
    if (buffer_wrapped) {
        return LOG_BUFFER_SIZE;
    } else {
        return log_write_pos;
    }
}

void log_collector_get_snapshot(size_t *out_write_pos, bool *out_wrapped)
{
    if (out_write_pos) {
        *out_write_pos = log_write_pos;
    }
    if (out_wrapped) {
        *out_wrapped = buffer_wrapped;
    }
}

size_t log_collector_read_snapshot(size_t offset, uint8_t *buffer, size_t max_len,
                                   size_t snapshot_size, size_t snapshot_write_pos, 
                                   bool snapshot_wrapped)
{
    if (!log_mutex || !buffer || max_len == 0) return 0;
    
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    
    size_t available;
    size_t start_pos;
    
    // 使用快照时刻的状态
    if (snapshot_wrapped) {
        // 缓冲区在快照时已满
        available = snapshot_size;  // Should be LOG_BUFFER_SIZE
        start_pos = snapshot_write_pos;  // 从最旧位置开始读
    } else {
        // 缓冲区在快照时未满
        available = snapshot_size;  // Should be snapshot_write_pos
        start_pos = 0;
    }
    
    // Check offset
    if (offset >= available) {
        xSemaphoreGive(log_mutex);
        return 0;
    }
    
    // Calculate read length
    size_t to_read = available - offset;
    if (to_read > max_len) {
        to_read = max_len;
    }
    
    // Calculate read position
    size_t read_pos = (start_pos + offset) % LOG_BUFFER_SIZE;
    
    // Skip broken UTF-8 continuation bytes at start
    for (int skip = 0; skip < 3 && to_read > 0; skip++) {
        if (!is_utf8_continuation(log_buffer[read_pos])) {
            break;
        }
        read_pos = (read_pos + 1) % LOG_BUFFER_SIZE;
        to_read--;
        offset++;
    }
    
    // Read data
    size_t copied = 0;
    for (size_t i = 0; i < to_read; i++) {
        buffer[copied++] = log_buffer[read_pos];
        read_pos = (read_pos + 1) % LOG_BUFFER_SIZE;
    }
    
    xSemaphoreGive(log_mutex);
    return copied;
}

const log_stats_t* log_collector_get_stats(void)
{
    stats.current_size = log_collector_get_size();
    stats.buffer_wrapped = buffer_wrapped;
    return &stats;
}

// ============================================
// Control interface
// ============================================

void log_collector_clear(void)
{
    if (!log_mutex) return;
    
    xSemaphoreTake(log_mutex, portMAX_DELAY);
    
    memset(log_buffer, 0, LOG_BUFFER_SIZE);
    log_write_pos = 0;
    buffer_wrapped = false;
    stats.current_size = 0;
    
    xSemaphoreGive(log_mutex);
    
    ESP_LOGI(TAG, "Log buffer cleared");
}

void log_collector_set_level(log_level_t level)
{
    min_log_level = level;
    const char *level_names[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "VERBOSE"};
    ESP_LOGI(TAG, "Log level set to %s", level_names[level]);
}

void log_collector_set_streaming(bool enable, uint16_t conn_handle, uint16_t attr_handle)
{
    streaming_enabled = enable;
    
    if (enable) {
        // Use printf to avoid triggering the hook
        printf("[LogCollector] Real-time streaming enabled\n");
    } else {
        printf("[LogCollector] Real-time streaming disabled\n");
    }
}
