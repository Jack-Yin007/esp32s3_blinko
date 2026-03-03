/**
 * @file nfc_storage.h
 * @brief NFC Card Storage Interface (File System)
 * 
 * Provides persistent storage for:
 *   - My card (本机名片)
 *   - Friend card (朋友名片，只存储最新1条)
 * 
 * Storage uses file system mounted on user data partition.
 * File paths are relative to mount point configured by storage team.
 */

#ifndef NFC_STORAGE_H
#define NFC_STORAGE_H

#include "nfc_card.h"
#include <stdbool.h>

// File system paths (stored in Flash FAT partition)
// Mount point: /flash (Flash FAT partition)
#define NFC_STORAGE_BASE_PATH   "/flash/nfc"     // NFC storage directory
#define NFC_FILE_MY_CARD        "/flash/nfc/my_card.bin"     // 本机名片文件
#define NFC_FILE_FRIEND_CARD    "/flash/nfc/friend.bin" // 朋友名片文件
#define NFC_FILE_FRIEND_FLAG    "/flash/nfc/friend.new"     // 新朋友标志文件

/**
 * @brief Initialize NFC storage (creates directory if needed)
 * 
 * Prerequisites:
 *   - Flash FAT partition must be formatted and mounted
 *   - Mount point: /flash
 * 
 * @return true on success, false if file system not ready
 */
bool nfc_storage_init(void);

/**
 * @brief Save my pet card (本机名片) to file system
 * @param card Pointer to card data
 * @return true on success
 */
bool nfc_storage_save_my_card(const nfc_card_t *card);

/**
 * @brief Load my pet card (本机名片) from file system
 * @param card Pointer to buffer for card data
 * @return true on success, false if not found or corrupted
 */
bool nfc_storage_load_my_card(nfc_card_t *card);

/**
 * @brief Save friend's card (朋友名片) to file system
 * 
 * Overwrites previous friend card (only keeps latest one).
 * Sets "new friend" flag automatically.
 * 
 * @param card Pointer to friend's card data
 * @return true on success
 */
bool nfc_storage_save_friend_card(const nfc_card_t *card);

/**
 * @brief Load friend's card (朋友名片) from file system
 * @param card Pointer to buffer for card data
 * @return true on success, false if not found or corrupted
 */
bool nfc_storage_load_friend_card(nfc_card_t *card);

/**
 * @brief Check if there's a new friend card (未读标志)
 * @return true if friend card exists and hasn't been read by APP
 */
bool nfc_storage_has_new_friend(void);

/**
 * @brief Clear "new friend" flag (APP读取后调用)
 * 
 * Should be called after APP successfully reads friend card.
 */
void nfc_storage_clear_new_friend_flag(void);

/**
 * @brief Delete my card file (for testing/reset)
 * @return true on success
 */
bool nfc_storage_delete_my_card(void);

/**
 * @brief Delete friend card file (for testing/reset)
 * @return true on success
 */
bool nfc_storage_delete_friend_card(void);

#endif // NFC_STORAGE_H
