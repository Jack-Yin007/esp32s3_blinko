/**
 * @file nfc_storage.h
 * @brief NFC Card Storage Interface (NVS Flash)
 * 
 * Provides persistent storage for pet identity cards in ESP32 NVS Flash.
 * 
 * Storage Keys:
 *   - my_card:     本机名片 (My pet's card)
 *   - friend_card: 朋友名片 (Friend's card, only stores latest one)
 *   - friend_new:  新朋友标志位 (Flag indicating new friend data available)
 * 
 * Usage Flow:
 *   1. nfc_storage_init() - Initialize NVS
 *   2. nfc_storage_save_my_card() - Save my card from APP
 *   3. [NFC Touch] - Exchange cards
 *   4. nfc_storage_save_friend_card() - Save received friend card
 *   5. nfc_storage_load_friend_card() - APP reads friend card
 */

#ifndef NFC_STORAGE_H
#define NFC_STORAGE_H

#include "nfc_card.h"
#include <stdbool.h>

/* ==================== NVS Configuration ==================== */

#define NFC_NVS_NAMESPACE       "blinko_nfc"    ///< NVS partition namespace
#define NFC_NVS_KEY_MY_CARD     "my_card"       ///< Key for my pet card
#define NFC_NVS_KEY_FRIEND_CARD "friend_card"   ///< Key for friend pet card
#define NFC_NVS_KEY_FRIEND_FLAG "friend_new"    ///< Key for new friend flag (uint8_t)

/* ==================== Initialization ==================== */

/**
 * @brief Initialize NFC storage subsystem
 * 
 * Must be called before any other nfc_storage_* functions.
 * Initializes NVS flash and creates namespace if needed.
 * 
 * @return true on success, false on failure
 * 
 * @note Thread-safe after initialization
 */
bool nfc_storage_init(void);

/* ==================== My Card Operations ==================== */

/**
 * @brief Save my pet card to Flash
 * 
 * Called when APP writes user information via BLE.
 * Automatically calculates CRC32 and sets timestamp.
 * 
 * @param card Pointer to card data (crc32 will be updated)
 * @return true on success, false on failure
 * 
 * @note Input card->crc32 will be recalculated
 * @note Input card->timestamp will be updated to current time
 */
bool nfc_storage_save_my_card(nfc_card_t *card);

/**
 * @brief Load my pet card from Flash
 * 
 * Used when sending card via NFC to another device.
 * Verifies magic number and CRC32 checksum.
 * 
 * @param card Pointer to buffer for card data
 * @return true on success, false if no card stored or corrupted
 */
bool nfc_storage_load_my_card(nfc_card_t *card);

/**
 * @brief Check if my card exists in Flash
 * 
 * @return true if my card is stored and valid
 */
bool nfc_storage_has_my_card(void);

/* ==================== Friend Card Operations ==================== */

/**
 * @brief Save friend's pet card to Flash
 * 
 * Called when receiving card via NFC from another device.
 * Overwrites previous friend card (only stores 1 friend).
 * Sets "new friend" flag for APP notification.
 * 
 * @param card Pointer to friend's card data (must have valid CRC)
 * @return true on success, false on failure
 * 
 * @note Verifies card->magic and card->crc32 before saving
 * @note Automatically sets friend_new flag to 1
 */
bool nfc_storage_save_friend_card(const nfc_card_t *card);

/**
 * @brief Load friend's pet card from Flash
 * 
 * Called when APP reads friend information via BLE.
 * Does NOT clear "new friend" flag (use nfc_storage_clear_new_friend_flag).
 * 
 * @param card Pointer to buffer for card data
 * @return true on success, false if no friend card stored or corrupted
 */
bool nfc_storage_load_friend_card(nfc_card_t *card);

/**
 * @brief Check if friend card exists in Flash
 * 
 * @return true if friend card is stored and valid
 */
bool nfc_storage_has_friend_card(void);

/* ==================== New Friend Flag Operations ==================== */

/**
 * @brief Check if there's a new friend card waiting to be read
 * 
 * Used to determine if APP should be notified about new friend.
 * 
 * @return true if friend_new flag is set
 */
bool nfc_storage_has_new_friend(void);

/**
 * @brief Clear the "new friend" flag
 * 
 * Called after APP successfully reads friend card.
 * 
 * @return true on success, false on failure
 */
bool nfc_storage_clear_new_friend_flag(void);

/* ==================== Utility Functions ==================== */

/**
 * @brief Erase all NFC storage data
 * 
 * Deletes my card, friend card, and new friend flag.
 * Used for factory reset or testing.
 * 
 * @return true on success, false on failure
 */
bool nfc_storage_erase_all(void);

/**
 * @brief Calculate CRC32 checksum for card data
 * 
 * Calculates CRC over entire card structure except crc32 field itself.
 * 
 * @param card Pointer to card data
 * @return CRC32 checksum value
 * 
 * @note Uses ESP32 hardware CRC accelerator
 */
uint32_t nfc_storage_calculate_crc(const nfc_card_t *card);

/**
 * @brief Verify card data integrity
 * 
 * Checks magic number and CRC32 checksum.
 * 
 * @param card Pointer to card data
 * @return true if card is valid, false otherwise
 */
bool nfc_storage_verify_card(const nfc_card_t *card);

#endif // NFC_STORAGE_H
