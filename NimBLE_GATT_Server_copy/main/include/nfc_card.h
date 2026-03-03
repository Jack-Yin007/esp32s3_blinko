/**
 * @file nfc_card.h
 * @brief NFC Pet Card Data Structure Definition
 * 
 * Defines the data format for pet identity cards exchanged via NFC.
 * Total size: 140 bytes (fixed)
 * 
 * Structure:
 *   - Core Identity: 70 bytes (pet_name + uuid + reserved padding)
 *   - Social Profile: 70 bytes (personality + catchphrase)
 */

#ifndef NFC_CARD_H
#define NFC_CARD_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>  // for memset, strncpy

// Field size definitions
#define NFC_CARD_PET_NAME_LEN       20  // Pet name (UTF-8, 20 bytes max)
#define NFC_CARD_UUID_LEN           16  // Unique ID (UUID/hash)
#define NFC_CARD_CORE_RESERVED_LEN  34  // Reserved space in Core Identity (padding to 70 bytes)
#define NFC_CARD_PERSONALITY_LEN    40  // Personality traits (5×8 chars: "playful,loyal,shy")
#define NFC_CARD_CATCHPHRASE_LEN    30  // Catchphrase/motto

// Section sizes
#define NFC_CARD_CORE_IDENTITY_SIZE 70  // Core Identity total size (fixed)
#define NFC_CARD_SOCIAL_PROFILE_SIZE 70 // Social Profile total size

/**
 * @brief NFC Pet Card Structure
 * 
 * Layout breakdown:
 *   Core Identity (70 bytes - FIXED):
 *     - Pet Name: 20 bytes (UTF-8 encoded name)
 *     - Unique ID: 16 bytes (UUID or hash identifier)
 *     - Reserved: 34 bytes (padding, filled with zeros)
 * 
 *   Social Profile (70 bytes):
 *     - Personality: 40 bytes (comma-separated traits, e.g., "playful,loyal,shy,curious,brave")
 *     - Catchphrase: 30 bytes (short motto or signature phrase)
 * 
 * Total: 140 bytes (70 + 70)
 */
typedef struct __attribute__((packed)) {
    // === Core Identity (70 bytes - FIXED) ===
    uint8_t pet_name[NFC_CARD_PET_NAME_LEN];        // Pet name (UTF-8) - 20 bytes
    uint8_t uuid[NFC_CARD_UUID_LEN];                // Unique identifier - 16 bytes
    uint8_t core_reserved[NFC_CARD_CORE_RESERVED_LEN]; // Reserved (zeros) - 34 bytes
    
    // === Social Profile (70 bytes) ===
    uint8_t personality[NFC_CARD_PERSONALITY_LEN];  // Traits: "playful,loyal,shy,curious,brave" - 40 bytes
    uint8_t catchphrase[NFC_CARD_CATCHPHRASE_LEN];  // Short motto - 30 bytes
} nfc_card_t;

// Compile-time size verification
_Static_assert(sizeof(nfc_card_t) == 140, "NFC card size must be exactly 140 bytes (70+70)");
_Static_assert(NFC_CARD_PET_NAME_LEN + NFC_CARD_UUID_LEN + NFC_CARD_CORE_RESERVED_LEN == NFC_CARD_CORE_IDENTITY_SIZE, 
               "Core Identity must be exactly 70 bytes");

/**
 * @brief Helper: Initialize card with zeros
 */
static inline void nfc_card_init(nfc_card_t *card) {
    if (card) {
        memset(card, 0, sizeof(nfc_card_t));
        // Core Identity reserved area is already zeroed by memset
    }
}

/**
 * @brief Helper: Set pet name (with null termination)
 */
static inline void nfc_card_set_name(nfc_card_t *card, const char *name) {
    if (card && name) {
        strncpy((char*)card->pet_name, name, NFC_CARD_PET_NAME_LEN - 1);
        card->pet_name[NFC_CARD_PET_NAME_LEN - 1] = '\0';
    }
}

/**
 * @brief Helper: Set personality traits
 */
static inline void nfc_card_set_personality(nfc_card_t *card, const char *traits) {
    if (card && traits) {
        strncpy((char*)card->personality, traits, NFC_CARD_PERSONALITY_LEN - 1);
        card->personality[NFC_CARD_PERSONALITY_LEN - 1] = '\0';
    }
}

/**
 * @brief Helper: Set catchphrase
 */
static inline void nfc_card_set_catchphrase(nfc_card_t *card, const char *phrase) {
    if (card && phrase) {
        strncpy((char*)card->catchphrase, phrase, NFC_CARD_CATCHPHRASE_LEN - 1);
        card->catchphrase[NFC_CARD_CATCHPHRASE_LEN - 1] = '\0';
    }
}

#endif // NFC_CARD_H
