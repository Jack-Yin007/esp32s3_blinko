/* SD card and FAT filesystem example.
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

// This example uses SDMMC peripheral to communicate with SD card.

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "pet_config.h"
#include "driver/sdmmc_host.h"
#if SOC_SDMMC_IO_POWER_EXTERNAL
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#endif
#include "sd_card.h"
#include "wear_levelling.h"
#define EXAMPLE_MAX_CHAR_SIZE    1024

static const char *TAG = "example";
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

#define EXAMPLE_IS_UHS1    (CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_SDR50 || CONFIG_EXAMPLE_SDMMC_SPEED_UHS_I_DDR50)


esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

/**
 * 列出SD卡目录内容
 */
esp_err_t sd_card_list_dir(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "📁 Listing directory: %s", path);
    ESP_LOGI(TAG, "==================================================");

    struct dirent *entry;
    int file_count = 0;
    int dir_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // 构造完整路径
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // 获取文件信息
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                ESP_LOGI(TAG, "  [DIR]  %s", entry->d_name);
                dir_count++;
            } else {
                ESP_LOGI(TAG, "  [FILE] %s (%ld bytes)", entry->d_name, st.st_size);
                file_count++;
            }
        } else {
            ESP_LOGI(TAG, "  [?] %s (stat failed)", entry->d_name);
        }
    }

    closedir(dir);
    
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "Total: %d files, %d directories", file_count, dir_count);

    return ESP_OK;
}

/**
 * @brief 初始化Flash FAT分区（用于音频和NFC文件存储）
 * 
 * 分区名称: flash (定义在partitions.csv)
 * 挂载点: /flash
 * 子目录:
 *   - /flash/audio  - WAV音频文件
 *   - /flash/nfc    - NFC相关文件
 * 
 * @return ESP_OK 成功 / ESP_FAIL 失败
 */
esp_err_t flash_fat_init(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing Flash FAT Partition");
    ESP_LOGI(TAG, "========================================");

    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 10,
        .format_if_mount_failed = true,  // 首次使用自动格式化
        .allocation_unit_size = 4096
    };

    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(FLASH_MOUNT_POINT, "flash", 
                                                       &mount_config, &s_wl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount Flash FAT partition (0x%x)", ret);
        ESP_LOGE(TAG, "Check partitions.csv has 'flash' partition defined");
        return ret;
    }

    ESP_LOGI(TAG, "✅ Flash FAT mounted at %s", FLASH_MOUNT_POINT);

    // 创建子目录结构
    struct stat st;
    if (stat("/flash/audio", &st) != 0) {
        ESP_LOGI(TAG, "Creating /flash/audio directory...");
        if (mkdir("/flash/audio", 0775) != 0) {
            ESP_LOGW(TAG, "Failed to create /flash/audio (may already exist)");
        }
    }

    if (stat("/flash/nfc", &st) != 0) {
        ESP_LOGI(TAG, "Creating /flash/nfc directory...");
        if (mkdir("/flash/nfc", 0775) != 0) {
            ESP_LOGW(TAG, "Failed to create /flash/nfc (may already exist)");
        }
    }

    ESP_LOGI(TAG, "Flash FAT structure ready:");
    ESP_LOGI(TAG, "  /flash/audio - Audio files");
    ESP_LOGI(TAG, "  /flash/nfc   - NFC files");
    ESP_LOGI(TAG, "========================================");

    return ESP_OK;
}

esp_err_t  sd_card_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing SD Card (SDMMC 1-line mode)");
    ESP_LOGI(TAG, "========================================");

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,               // 增加到10，支持多个WAV同时打开
        .allocation_unit_size = 8 * 1024  // 8KB，减少内存使用
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 40MHz for SDMMC)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = 10000;  // 降低到10MHz，提高稳定性（防止sdmmc_read_blocks失败）
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    slot_config.width = 1;

    // On chips where the GPIOs used for SD card can be configured, set them in
    // the slot_config structure:
    slot_config.clk = PIN_SDMMC_CLK;
    slot_config.cmd = PIN_SDMMC_CMD;
    slot_config.d0 =  PIN_SDMMC_DAT0;

    ESP_LOGI(TAG, "Hardware Configuration:");
    ESP_LOGI(TAG, "  Mode: SDMMC 1-line");
    ESP_LOGI(TAG, "  CLK  : GPIO%d", slot_config.clk);
    ESP_LOGI(TAG, "  CMD  : GPIO%d", slot_config.cmd);
    ESP_LOGI(TAG, "  DAT0 : GPIO%d", slot_config.d0);
    ESP_LOGI(TAG, "  Freq : %d kHz (reduced for stability)", host.max_freq_khz);
    ESP_LOGI(TAG, "========================================");


    // Enable internal pullups on enabled pins. The internal pullups
    // are insufficient however, please make sure 10k external pullups are
    // connected on the bus. This is for debug / example purpose only.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "Attempting to mount filesystem...");
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "========================================");
        ESP_LOGE(TAG, "SD Card Mount Failed!");
        ESP_LOGE(TAG, "========================================");
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Error: Failed to mount filesystem");
            ESP_LOGE(TAG, "Possible causes:");
            ESP_LOGE(TAG, "  - SD card not formatted as FAT/FAT32");
            ESP_LOGE(TAG, "  - SD card partition table corrupted");
            ESP_LOGE(TAG, "Solution: Format SD card as FAT32 on PC");
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Error: SD card communication timeout (0x%x)", ret);
            ESP_LOGE(TAG, "Possible causes:");
            ESP_LOGE(TAG, "  - SD card not inserted");
            ESP_LOGE(TAG, "  - Poor contact in SD card slot");
            ESP_LOGE(TAG, "  - Wrong GPIO pins (CLK=%d, CMD=%d, DAT0=%d)", 
                     slot_config.clk, slot_config.cmd, slot_config.d0);
            ESP_LOGE(TAG, "  - Missing pull-up resistors (10kΩ recommended)");
            ESP_LOGE(TAG, "Solution:");
            ESP_LOGE(TAG, "  1. Check SD card is fully inserted");
            ESP_LOGE(TAG, "  2. Verify pin connections");
            ESP_LOGE(TAG, "  3. Add 10kΩ pull-up resistors to CLK/CMD/DAT0");
        } else {
            ESP_LOGE(TAG, "Error: Card init failed with code 0x%x (%s)", 
                     ret, esp_err_to_name(ret));
        }
        ESP_LOGE(TAG, "========================================");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "✅ SD Card Mounted Successfully!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Card Information:");
    ESP_LOGI(TAG, "  Name: %s", card->cid.name);
    ESP_LOGI(TAG, "  Type: %s", (card->ocr & (1 << 30)) ? "SDHC/SDXC" : "SDSC");
    ESP_LOGI(TAG, "  Speed: %s", (card->csd.tr_speed > 25000000) ? "High Speed" : "Default Speed");
    ESP_LOGI(TAG, "  Capacity: %llu MB", ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    ESP_LOGI(TAG, "  Sector size: %d bytes", card->csd.sector_size);
    ESP_LOGI(TAG, "========================================");

    // 列出根目录
    sd_card_list_dir(MOUNT_POINT);
    
    // 列出audio目录（如果存在）
    char audio_path[64];
    snprintf(audio_path, sizeof(audio_path), "%s/audio", MOUNT_POINT);
    sd_card_list_dir(audio_path);

    return ESP_OK;


}
