/**
 * @file flash_storage.c
 * @brief Flash存储工具 - WAV文件复制到Flash FAT分区
 */

#include "flash_storage.h"
#include "sd_card.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "FlashStorage";

#define COPY_BUFFER_SIZE 4096  // 4KB缓冲区

/**
 * @brief 检查Flash分区是否已有WAV文件
 */
bool flash_storage_has_wav_files(void)
{
    DIR *dir = opendir("/flash/audio");
    if (!dir) {
        return false;
    }

    int wav_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        
        if (len < 5) continue;
        
        const char *ext = &name[len - 4];
        if (strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".WAV") == 0) {
            wav_count++;
        }
    }

    closedir(dir);
    
    // 至少需要20个WAV文件才认为Flash已就绪
    bool has_enough = (wav_count >= 20);
    ESP_LOGI(TAG, "Flash audio check: found %d WAV files (need ≥20): %s", 
             wav_count, has_enough ? "OK" : "Insufficient");
    return has_enough;
}

/**
 * @brief 从SD卡复制单个文件到Flash
 */
static esp_err_t copy_file(const char *src_path, const char *dst_path)
{
    FILE *src = fopen(src_path, "rb");
    if (!src) {
        ESP_LOGE(TAG, "Failed to open source: %s", src_path);
        return ESP_FAIL;
    }

    FILE *dst = fopen(dst_path, "wb");
    if (!dst) {
        ESP_LOGE(TAG, "Failed to create dest: %s", dst_path);
        fclose(src);
        return ESP_FAIL;
    }

    uint8_t *buffer = malloc(COPY_BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate copy buffer");
        fclose(src);
        fclose(dst);
        return ESP_FAIL;
    }

    size_t total_copied = 0;
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, COPY_BUFFER_SIZE, src)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, dst);
        if (bytes_written != bytes_read) {
            ESP_LOGE(TAG, "Write error: wrote %d/%d bytes", bytes_written, bytes_read);
            free(buffer);
            fclose(src);
            fclose(dst);
            return ESP_FAIL;
        }
        total_copied += bytes_written;
    }

    free(buffer);
    fclose(src);
    fclose(dst);

    ESP_LOGI(TAG, "✓ Copied: %s -> %s (%d bytes)", src_path, dst_path, total_copied);
    return ESP_OK;
}

/**
 * @brief 从SD卡复制所有WAV文件到Flash分区
 */
esp_err_t flash_storage_copy_wav_files(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Copying WAV files: /sdcard/audio -> /flash/audio");
    ESP_LOGI(TAG, "========================================");

    DIR *dir = opendir("/sdcard/audio");
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open /sdcard/audio directory");
        ESP_LOGI(TAG, "Please ensure SD card is mounted with audio files");
        return ESP_FAIL;
    }

    int copied_count = 0;
    int failed_count = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // 只处理.WAV文件（忽略大小写）
        const char *name = entry->d_name;
        size_t len = strlen(name);
        
        if (len < 5 || len > 240) continue;  // 文件名长度检查，确保路径不会溢出
        
        const char *ext = &name[len - 4];
        if (strcasecmp(ext, ".wav") != 0 && strcasecmp(ext, ".WAV") != 0) {
            continue;
        }

        char src_path[300];
        char dst_path[300];
        snprintf(src_path, sizeof(src_path), "/sdcard/audio/%s", name);
        snprintf(dst_path, sizeof(dst_path), "/flash/audio/%s", name);

        if (copy_file(src_path, dst_path) == ESP_OK) {
            copied_count++;
        } else {
            failed_count++;
        }
    }

    closedir(dir);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Copy complete: %d succeeded, %d failed", copied_count, failed_count);
    ESP_LOGI(TAG, "========================================");

    return (failed_count == 0) ? ESP_OK : ESP_FAIL;
}
