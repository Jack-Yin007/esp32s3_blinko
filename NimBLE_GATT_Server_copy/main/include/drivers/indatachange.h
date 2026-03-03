#ifndef IN_DATA_CHANGE_H
#define IN_DATA_CHANGE_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_CARD_BYTES  64

/**
 * @brief Callback function type for NFC card exchange notification
 * 
 * This callback is invoked when a friend's business card is successfully
 * received via NFC exchange.
 * 
 * @param card Pointer to the received card data (64 bytes)
 * @param user_data User-defined data passed during callback registration
 * 
 * @note This callback is called from the NFC task context with mutex held.
 *       Keep processing minimal and avoid blocking operations.
 */
typedef void (*nfc_card_exchange_callback_t)(const uint8_t card[], void *user_data);

/**
 * @fn nfc_init
 * @brief initalize PN532, start nfc task
 * @return ESP_OK if success
 */
esp_err_t nfc_init(void);

/**
 * @fn nfc_set_card
 * @brief update my name card
 * @return ESP_OK if success
 * @notes the size of name card is fixed to 64 bytes
 */
esp_err_t nfc_set_card(const uint8_t card[]);

/**
 * @fn nfc_get_card
 * @brief get the name card of friend
 * @return ESP_OK if success
 */
esp_err_t nfc_get_card(uint8_t card[]);

/**
 * @brief Register callback for NFC card exchange events
 * 
 * The callback will be invoked whenever a friend's card is successfully
 * received via NFC exchange (both Initiator and Target modes).
 * 
 * @param callback Function pointer to callback (NULL to disable)
 * @param user_data User data to pass to callback (can be NULL)
 * @return ESP_OK on success, ESP_FAIL if NFC not initialized
 * 
 * @example
 * void on_card_received(const uint8_t card[], void *user_data) {
 *     ESP_LOGI("APP", "Friend card received!");
 *     // Process card data...
 * }
 * 
 * nfc_register_exchange_callback(on_card_received, NULL);
 */
esp_err_t nfc_register_exchange_callback(nfc_card_exchange_callback_t callback, void *user_data);

/**
 * @brief Unregister NFC card exchange callback
 * 
 * @return ESP_OK on success, ESP_FAIL if NFC not initialized
 */
esp_err_t nfc_unregister_exchange_callback(void);

#ifdef __cplusplus
}
#endif

#endif

