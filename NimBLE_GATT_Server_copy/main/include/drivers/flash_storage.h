#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief 检查Flash分区是否已有WAV文件
 * 
 * @return true 已有文件 / false 为空
 */
bool flash_storage_has_wav_files(void);

/**
 * @brief 从SD卡复制WAV文件到Flash分区
 * 
 * @return ESP_OK 成功 / ESP_FAIL 失败
 */
esp_err_t flash_storage_copy_wav_files(void);

#endif // FLASH_STORAGE_H
