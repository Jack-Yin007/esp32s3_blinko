#ifndef SD_CARD_H
#define SD_CARD_H
#include "esp_err.h"
#define MOUNT_POINT "/sdcard"
#define FLASH_MOUNT_POINT "/flash"
esp_err_t flash_fat_init(void);
esp_err_t sd_card_init(void);
esp_err_t sd_card_list_dir(const char *path);
#endif
