/**
 * @file nfc_card.h
 * @brief NFC Pet Card Data Structure Definition
 * 
 * Defines the data format for pet identity cards exchanged via NFC.
 * New format: ~140 bytes total
 * - Core Identity: 36 bytes (pet name + UUID)
 * - Social Profile: 70 bytes (personality + catchphrase)
 * - Metadata: 12 bytes (magic, timestamp, CRC)
 */

#ifndef NFC_CARD_H
#define NFC_CARD_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== Field Size Definitions ==================== */

#define NFC_CARD_PET_NAME_LEN       20  ///< Pet name (UTF-8, 20 bytes max)
#define NFC_CARD_UUID_LEN           16  ///< Unique ID (UUID or hash)
#define NFC_CARD_PERSONALITY_LEN    40  ///< Personality traits (5×8 chars)
#define NFC_CARD_CATCHPHRASE_LEN    30  ///< Catchphrase/motto

/* ==================== Magic Number ==================== */

/**
 * Magic number to verify card validity
 * Format: "PET" (0x504554) + version (0xC0 = v1.0)
 */
#define NFC_CARD_MAGIC              0x504554C0

/* ==================== Data Structures ==================== */

/**
 * @brief NFC Pet Card Structure
 * 
 * Complete pet identity card for NFC exchange between two Blinko devices.
 * 
 * Memory Layout:
 *   Offset  | Size | Field
 *   --------|------|------------------
 *   0       | 20   | pet_name
 *   20      | 16   | uuid
 *   36      | 40   | personality
 *   76      | 30   | catchphrase
 *   106     | 4    | magic
 *   110     | 4    | timestamp
 *   114     | 4    | crc32
 *   --------|------|------------------
 *   Total:  | 118  | bytes
 * 
 * Example:
 * @code
 * nfc_card_t my_card = {
 *     .pet_name = "Max",
 *     .uuid = {0x12, 0x34, ...},
 *     .personality = "playful,loyal,shy,brave,cute",
 *     .catchphrase = "Always happy!",
 *     .magic = NFC_CARD_MAGIC,
 *     .timestamp = 1700000000,
 *     .crc32 = 0  // Will be calculated
 * };
 * @endcode
 */
typedef struct __attribute__((packed)) {
    /* === Core Identity (36 bytes) === */
    uint8_t pet_name[NFC_CARD_PET_NAME_LEN];    ///< Pet name (UTF-8, null-terminated)
    uint8_t uuid[NFC_CARD_UUID_LEN];            ///< Unique identifier (16-byte UUID/hash)
    
    /* === Social Profile (70 bytes) === */
    uint8_t personality[NFC_CARD_PERSONALITY_LEN];  ///< Traits: "playful,loyal,shy,..."
    uint8_t catchphrase[NFC_CARD_CATCHPHRASE_LEN];  ///< Short motto/phrase
    
    /* === Metadata (12 bytes) === */
    uint32_t magic;         ///< Validity marker: must be NFC_CARD_MAGIC (0x504554C0)
    uint32_t timestamp;     ///< Last update time (Unix timestamp, seconds since 1970)
    uint32_t crc32;         ///< Data integrity checksum (CRC-32)
} nfc_card_t;

/* ==================== Compile-Time Checks ==================== */

// Ensure structure size is within expected bounds
_Static_assert(sizeof(nfc_card_t) == 118, "NFC card size must be exactly 118 bytes");
_Static_assert(sizeof(nfc_card_t) <= 140, "NFC card size exceeds 140 bytes limit");

/* ==================== Helper Macros ==================== */

/**
 * @brief Initialize an empty NFC card with default values
 */
#define NFC_CARD_INIT() { \
    .pet_name = {0}, \
    .uuid = {0}, \
    .personality = {0}, \
    .catchphrase = {0}, \
    .magic = NFC_CARD_MAGIC, \
    .timestamp = 0, \
    .crc32 = 0 \
}

/**
 * @brief Check if a card has valid magic number
 */
#define NFC_CARD_IS_VALID(card) ((card)->magic == NFC_CARD_MAGIC)

#endif // NFC_CARD_H
