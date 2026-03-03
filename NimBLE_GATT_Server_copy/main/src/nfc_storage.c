/**
 * @file nfc_storage.c
 * @brief NFC Card Storage Implementation (File System)
 * 
 * Implements persistent storage for NFC pet cards using file system.
 * Assumes user data partition is formatted and mounted by storage team.
 */

#include "nfc_storage.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

static const char *TAG = "NFC_STORAGE";

/**
 * @brief Validate card data (basic check)
 */
static bool validate_card(const nfc_card_t *card) {
    // Check if pet_name is not all zeros
    bool has_name = false;
    for (int i = 0; i < NFC_CARD_PET_NAME_LEN; i++) {
        if (card->pet_name[i] != 0) {
            has_name = true;
            break;
        }
    }
    
    if (!has_name) {
        ESP_LOGW(TAG, "Invalid card: pet_name is empty");
        return false;
    }
    
    return true;
}

/**
 * @brief Check if file system is accessible
 */
static bool is_fs_ready(void) {
    struct stat st;
    // Check if /flash directory exists (Flash FAT partition mount point)
    if (stat("/flash", &st) != 0) {
        ESP_LOGE(TAG, "Flash FAT partition not mounted at /flash");
        return false;
    }
    
    if (!S_ISDIR(st.st_mode)) {
        ESP_LOGE(TAG, "/flash exists but is not a directory");
        return false;
    }
    
    return true;
}

/**
 * @brief Create NFC storage directory if needed
 */
static bool ensure_directory(void) {
    struct stat st;
    
    // Check if NFC directory exists
    if (stat(NFC_STORAGE_BASE_PATH, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true; // Directory already exists
        } else {
            ESP_LOGE(TAG, "%s exists but is not a directory", NFC_STORAGE_BASE_PATH);
            return false;
        }
    }
    
    // Create directory
    if (mkdir(NFC_STORAGE_BASE_PATH, 0755) != 0) {
        ESP_LOGE(TAG, "Failed to create directory: %s", NFC_STORAGE_BASE_PATH);
        return false;
    }
    
    ESP_LOGI(TAG, "Created NFC storage directory: %s", NFC_STORAGE_BASE_PATH);
    return true;
}

/**
 * @brief Save card to file
 */
static bool save_card_to_file(const char *filepath, const nfc_card_t *card) {
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", filepath);
        return false;
    }
    
    size_t written = fwrite(card, 1, sizeof(nfc_card_t), f);
    fclose(f);
    
    if (written != sizeof(nfc_card_t)) {
        ESP_LOGE(TAG, "Failed to write complete card data: %d/%d bytes", 
                 written, sizeof(nfc_card_t));
        return false;
    }
    
    ESP_LOGI(TAG, "Card saved to: %s (%d bytes)", filepath, sizeof(nfc_card_t));
    return true;
}

/**
 * @brief Load card from file
 */
static bool load_card_from_file(const char *filepath, nfc_card_t *card) {
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        ESP_LOGD(TAG, "File not found: %s", filepath);
        return false;
    }
    
    size_t read = fread(card, 1, sizeof(nfc_card_t), f);
    fclose(f);
    
    if (read != sizeof(nfc_card_t)) {
        ESP_LOGW(TAG, "Incomplete card data read: %d/%d bytes", 
                 read, sizeof(nfc_card_t));
        return false;
    }
    
    // Validate card data
    if (!validate_card(card)) {
        ESP_LOGW(TAG, "Card validation failed: %s", filepath);
        return false;
    }
    
    ESP_LOGI(TAG, "Card loaded from: %s", filepath);
    return true;
}

// ==================== Public API ====================

bool nfc_storage_init(void) {
    ESP_LOGI(TAG, "Initializing NFC storage...");
    
    // Check if file system is mounted
    if (!is_fs_ready()) {
        ESP_LOGE(TAG, "File system not ready! Please ensure user data partition is mounted.");
        return false;
    }
    
    // Create NFC directory
    if (!ensure_directory()) {
        return false;
    }
    
    ESP_LOGI(TAG, "NFC storage initialized successfully");
    return true;
}

bool nfc_storage_save_my_card(const nfc_card_t *card) {
    if (!card) {
        ESP_LOGE(TAG, "NULL card pointer");
        return false;
    }
    
    // Directly save card (no metadata processing needed)
    bool success = save_card_to_file(NFC_FILE_MY_CARD, card);
    
    if (success) {
        ESP_LOGI(TAG, "My card saved: name='%.*s'", 
                 NFC_CARD_PET_NAME_LEN, card->pet_name);
    }
    
    return success;
}

bool nfc_storage_load_my_card(nfc_card_t *card) {
    if (!card) {
        ESP_LOGE(TAG, "NULL card pointer");
        return false;
    }
    
    bool success = load_card_from_file(NFC_FILE_MY_CARD, card);
    
    if (success) {
        ESP_LOGI(TAG, "My card loaded: name='%.*s'", 
                 NFC_CARD_PET_NAME_LEN, card->pet_name);
    }
    
    return success;
}

bool nfc_storage_save_friend_card(const nfc_card_t *card) {
    if (!card) {
        ESP_LOGE(TAG, "NULL card pointer");
        return false;
    }
    
    // Directly save card (no metadata processing needed)
    bool success = save_card_to_file(NFC_FILE_FRIEND_CARD, card);
    
    if (success) {
        // Create "new friend" flag file
        FILE *f = fopen(NFC_FILE_FRIEND_FLAG, "w");
        if (f) {
            fprintf(f, "1");
            fclose(f);
        }
        
        ESP_LOGI(TAG, "Friend card saved: name='%.*s'", 
                 NFC_CARD_PET_NAME_LEN, card->pet_name);
    }
    
    return success;
}

bool nfc_storage_load_friend_card(nfc_card_t *card) {
    if (!card) {
        ESP_LOGE(TAG, "NULL card pointer");
        return false;
    }
    
    bool success = load_card_from_file(NFC_FILE_FRIEND_CARD, card);
    
    if (success) {
        ESP_LOGI(TAG, "Friend card loaded: name='%.*s'", 
                 NFC_CARD_PET_NAME_LEN, card->pet_name);
    }
    
    return success;
}

bool nfc_storage_has_new_friend(void) {
    // Check if flag file exists
    struct stat st;
    bool has_flag = (stat(NFC_FILE_FRIEND_FLAG, &st) == 0);
    
    if (has_flag) {
        ESP_LOGD(TAG, "New friend flag detected");
    }
    
    return has_flag;
}

void nfc_storage_clear_new_friend_flag(void) {
    if (unlink(NFC_FILE_FRIEND_FLAG) == 0) {
        ESP_LOGI(TAG, "New friend flag cleared");
    } else {
        ESP_LOGD(TAG, "No friend flag to clear");
    }
}

bool nfc_storage_delete_my_card(void) {
    if (unlink(NFC_FILE_MY_CARD) == 0) {
        ESP_LOGI(TAG, "My card deleted");
        return true;
    }
    ESP_LOGW(TAG, "Failed to delete my card (may not exist)");
    return false;
}

bool nfc_storage_delete_friend_card(void) {
    bool card_deleted = (unlink(NFC_FILE_FRIEND_CARD) == 0);
    bool flag_deleted = (unlink(NFC_FILE_FRIEND_FLAG) == 0);
    
    if (card_deleted || flag_deleted) {
        ESP_LOGI(TAG, "Friend card deleted");
        return true;
    }
    
    ESP_LOGW(TAG, "Failed to delete friend card (may not exist)");
    return false;
}
