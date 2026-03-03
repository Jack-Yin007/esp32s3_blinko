/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "pet_config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nfc_card.h"      // NFC名片数据结构
#include "nfc_storage.h"   // NFC存储API

/**
 * @brief Callback function type for NFC card exchange notification
 * @param card Pointer to the received friend's card data (64 bytes)
 * @param user_data User data passed during callback registration
 */
typedef void (*nfc_card_exchange_callback_t)(const uint8_t card[], void *user_data);

/**
 * NFC/RFID PN532 UART Driver
 * 
 * Hardware Configuration:
 * - UART Port: UART2
 * - TX Pin: GPIO48 (ESP32-S3 -> PN532 RX)
 * - RX Pin: GPIO47 (ESP32-S3 <- PN532 TX)
 * - Baud Rate: 115200
 * - Flow Control: Disabled
 */

#define ECHO_TEST_TXD (48)  // GPIO48 - NFC TX
#define ECHO_TEST_RXD (47)  // GPIO47 - NFC RX
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      2


#define BUF_SIZE (1024)



#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"

// --- Add these definitions ---
#define PN532_FRAME_MAX_LENGTH  (255)
#define PN532_ACK_FRAME_SIZE    (6)
#define PN532_NACK_FRAME_SIZE   (5)
#define PN532_ACK_FRAME         {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00}
#define PN532_NACK_FRAME        {0x00, 0x00, 0xFF, 0xFF, 0x00}
#define PN532_RESPONSE_TIMEOUT_MS        (100)
#define PN532_UART_PORT_NUM  ECHO_UART_PORT_NUM
#define PN532_POLLING_TIMEOUT_MS        (6000)

// 使用统一的NFC名片大小定义（来自 nfc_card.h）
#define NUM_CARD_BYTES  sizeof(nfc_card_t)  // 140字节

//static const char *TAG = "PN532_COMM";
static const char *TAG = "NFC";
#if 0
// --- Pre-defined Commands ---
// Wakeup + SAMConfiguration (Normal Mode) - Used for both reader and emulator init/wakeup
const uint8_t CMD_WAKEUP_AND_CONFIG[] = {
        0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0x03, 0xFD, 0xD4, 0x14, 0x01, 0x17, 0x00
};

// InListPassiveTarget for Type A (Mifare) - Reader Mode Polling
const uint8_t CMD_INLIST_PASSIVE_TARGET[] = {
    0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x4A, 0x02, 0x00, 0xE0, 0x00
};

// InDataExchange - Send data after finding a card (example data)
const uint8_t CMD_INEXCHANGE_DATA[] = {
    0x00, 0x00, 0xFF, 0x15, 0xEB, 0xD4, 0x40, 0x01,
    0xA0, 0x06, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    0x0E, 0x0F, 0xCD, 0x00
};

const uint8_t CMD_SHUTDOWN_RF[] = {0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4, 0x32, 0x01, 0x00, 0xF9, 0x00};
// TgInitAsTarget - Emulate a card (specific configuration from your request)
const uint8_t CMD_TGINIT_AS_TARGET[] = {
    0x00, 0x00, 0xFF, 0x27, 0xD9, 0xD4, 0x8C, 0x04,
    0x08, 0x00, 0x12, 0x34, 0x56, 0x60, 0x01, 0xFE,
    0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xC0, 0xC1,
    0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xFF, 0xFF,
    0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33,
    0x22, 0x11, 0x00, 0x00, 0xFD, 0x00
};

// TgGetData - Wait for data when emulating a card
const uint8_t CMD_TGGET_DATA[] = {
    0x00, 0x00, 0xFF, 0x02, 0xFE, 0xD4, 0x86, 0xA6, 0x00
};

const uint8_t CMD_TGSEND_DATA[] = {0x00, 0x00, 0xff, 0x04, 0xfa, 0xd4, 0x8e, 0x90, 0x00, 0x0e, 0x00};

esp_err_t pn532_send_command(const uint8_t *command, size_t cmd_len)
{
    if (command == NULL || cmd_len == 0) {
        ESP_LOGE(TAG, "Invalid command pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

    // Optional: Add delay before sending if needed for timing
    // vTaskDelay(pdMS_TO_TICKS(1)); 

    int tx_bytes = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)command, cmd_len);
    if (tx_bytes != (int)cmd_len) {
         ESP_LOGE(TAG, "UART write failed! Wrote %d/%d bytes", tx_bytes, (int)cmd_len);
         return ESP_FAIL;
    }
    
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, command, cmd_len, ESP_LOG_DEBUG); // Log sent command at Debug level
    return ESP_OK;
}
esp_err_t pn532_receive_response(uint8_t *response, size_t *resp_len, TickType_t timeout_ticks)
{
    if (response == NULL || resp_len == NULL || *resp_len == 0) {
        ESP_LOGE(TAG, "Invalid response buffer or length pointer");
        return ESP_ERR_INVALID_ARG;
    }
    int len = uart_read_bytes(PN532_UART_PORT_NUM, response, 100, pdMS_TO_TICKS(5000));
    
    if (len > 0) {
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}
// --- Existing or similar functions (pn532_uart_init, pn532_receive_response) ---
// ... (Assume these are implemented as before, with pn532_receive_response being robust) ...

/**
 * @brief Send a command and wait for the standard ACK/NACK response.
 *
 * @param command Pointer to the command buffer.
 * @param cmd_len Length of the command.
 * @param timeout_ms Timeout in milliseconds to wait for the ACK/NACK.
 * @return ESP_OK if ACK received, ESP_ERR_INVALID_RESPONSE if NACK or unexpected,
 *         ESP_ERR_TIMEOUT if no response, ESP_FAIL for other errors.
 */
esp_err_t pn532_send_command_and_wait_ack(const uint8_t *command, size_t cmd_len, TickType_t timeout_ms)
{
    if (command == NULL || cmd_len == 0) {
        ESP_LOGE(TAG, "Invalid command pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Sending command...");
    //ESP_ERROR_CHECK(uart_flush_input(PN532_UART_PORT_NUM)); // Clear input before sending
    ESP_ERROR_CHECK(pn532_send_command(command, cmd_len)); // Use your existing send function

    uint8_t response[PN532_FRAME_MAX_LENGTH];
    size_t response_len = sizeof(response);

    ESP_LOGD(TAG, "Waiting for ACK/NACK...");
    int res = uart_read_bytes(PN532_UART_PORT_NUM, response, PN532_FRAME_MAX_LENGTH, pdMS_TO_TICKS(100));
    if(res > 0) {
        return ESP_OK;
    }
    return ESP_ERR_INVALID_RESPONSE;
}


esp_err_t pn532_send_command_and_wait_ack_final(const uint8_t *command, size_t cmd_len, TickType_t timeout_ms)
{
    if (command == NULL || cmd_len == 0) {
        ESP_LOGE(TAG, "Invalid command pointer or length");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Sending command...");
    //ESP_ERROR_CHECK(uart_flush_input(PN532_UART_PORT_NUM)); // Clear input before sending
    ESP_ERROR_CHECK(pn532_send_command(command, cmd_len)); // Use your existing send function

    uint8_t response[PN532_FRAME_MAX_LENGTH];
    size_t response_len = sizeof(response);

    ESP_LOGD(TAG, "Waiting for ACK/NACK...");
    int res = uart_read_bytes(PN532_UART_PORT_NUM, response, PN532_FRAME_MAX_LENGTH, pdMS_TO_TICKS(3000));
    if(res > 0) {
        for (int i = 0; i < res; i++) {
            ESP_LOGI(TAG, "data: %02X", response[i]);
        }
    }
    return ESP_ERR_INVALID_RESPONSE;
}
// --- Modified Task Functions ---
// You would then modify your reader/emulator tasks to use this new function:

void pn532_reader_mode_task(void)
{
    ESP_LOGI(TAG, "--- Starting READER MODE ---");
    uint8_t response_buffer[PN532_FRAME_MAX_LENGTH];
    size_t response_len;
    esp_err_t ret;

    // 1. Wakeup and configure PN532 - Wait for ACK
    ESP_LOGI(TAG, "Waking up and configuring...");
    ret = pn532_send_command_and_wait_ack(CMD_WAKEUP_AND_CONFIG, sizeof(CMD_WAKEUP_AND_CONFIG), PN532_RESPONSE_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wakeup/configure PN532 for Reader Mode: %s", esp_err_to_name(ret));
        // Handle error (e.g., retry, skip cycle)
        goto end_reader_cycle;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Brief pause after config ACK

    // 2. Send InListPassiveTarget command - Wait for ACK
    ESP_LOGI(TAG, "Polling for cards...");
    ret = pn532_send_command_and_wait_ack(CMD_INLIST_PASSIVE_TARGET, sizeof(CMD_INLIST_PASSIVE_TARGET), PN532_RESPONSE_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send InListPassiveTarget command: %s", esp_err_to_name(ret));
        // Handle error
        goto end_reader_cycle;
    }
    ESP_LOGI(TAG, "Polling start...");
    // 3. Wait for the *actual* response from InListPassiveTarget (card found or timeout)
    response_len = sizeof(response_buffer);
    ret = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, 1, pdMS_TO_TICKS(5000));

    if (ret >0) {
        ESP_LOGI(TAG, "Card FOUND in Reader Mode!");

        ret = pn532_send_command_and_wait_ack_final(CMD_INEXCHANGE_DATA, sizeof(CMD_INEXCHANGE_DATA), PN532_RESPONSE_TIMEOUT_MS);
        ESP_LOGI(TAG, "Sending predefined data...");
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send InDataExchange command: %s", esp_err_to_name(ret));
            // Handle error
             goto end_reader_cycle; // Or continue if you want to try next cycle
        }
        //vTaskDelay(pdMS_TO_TICKS(5000) );

    } 
    //pn532_send_command_and_wait_ack(CMD_SHUTDOWN_RF, sizeof(CMD_SHUTDOWN_RF), PN532_RESPONSE_TIMEOUT_MS);
end_reader_cycle:
    ESP_LOGI(TAG, "--- Ending READER MODE ---\n");
}


void pn532_emulator_mode_task(void)
{
    ESP_LOGI(TAG, "--- Starting EMULATOR MODE ---");
    uint8_t response_buffer[PN532_FRAME_MAX_LENGTH];
    uint8_t response_buffer1[PN532_FRAME_MAX_LENGTH];
    size_t response_len;
    esp_err_t ret;

    // 1. Wakeup and configure PN532 - Wait for ACK
    ESP_LOGI(TAG, "Waking up and configuring...");
    ret = pn532_send_command_and_wait_ack(CMD_WAKEUP_AND_CONFIG, sizeof(CMD_WAKEUP_AND_CONFIG), PN532_RESPONSE_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wakeup/configure PN532 for Emulator Mode: %s", esp_err_to_name(ret));
        // Handle error
        goto end_emulator_cycle;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Brief pause after config ACK

    // 2. Send TgInitAsTarget command - This command's ACK *is* D5 8D, not standard 00 00 FF 00 FF 00
    ESP_LOGI(TAG, "Initializing PN532 as target/emulated card...");
    ret = pn532_send_command_and_wait_ack(CMD_TGINIT_AS_TARGET, sizeof(CMD_TGINIT_AS_TARGET), PN532_RESPONSE_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to wakeup/configure PN532 for Emulator Mode: %s", esp_err_to_name(ret));
        // Handle error
        goto end_emulator_cycle;
    }
    ESP_LOGI(TAG, "Initialized PN532 as target/emulated card...");
    // Now wait for the specific response to TgInitAsTarget, which is D5 8D (ACK) or an error
    response_len = sizeof(response_buffer);
    int read_num = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, 1, pdMS_TO_TICKS(5000)); // Use longer timeout maybe?

    if (read_num > 0) {
          
        ESP_LOGE(TAG, "being detected");
    } else {
        goto end_emulator_cycle;
    }

    pn532_send_command(CMD_TGGET_DATA, sizeof(CMD_TGGET_DATA));

    read_num = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, 45, pdMS_TO_TICKS(3000)); //
    pn532_send_command_and_wait_ack(CMD_TGSEND_DATA, sizeof(CMD_TGSEND_DATA), PN532_RESPONSE_TIMEOUT_MS);

   for (int i = 0; i < read_num; i++) {

        ESP_LOGI(TAG, "data: %02X", response_buffer[i]);

    }
    
end_emulator_cycle:
    ESP_LOGI(TAG, "--- Ending EMULATOR MODE ---\n");
}

static void nfc_uart_init()
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
    
    ESP_LOGI("NFC_UART", "NFC UART initialized: TX=GPIO%d, RX=GPIO%d", ECHO_TEST_TXD, ECHO_TEST_RXD);
}
#endif


const char CMD_IS1[] = {
	0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xFF, 0x03, 0xFD,
	0xD4, 0x14, 0x01,
	0x17, 0x00};
char CMD_IR1[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x16, 0x00, 0xFF, 0x02, 0xFE,
	0x16, 0x00,
	0x16, 0x00};

const char CMD_IS2[] = {
	0x00, 0x00, 0xFF, 0x09, 0xF7,
	0xD4, 0x00, 0x00, 0x6C, 0x69, 0x62, 0x6E, 0x66, 0x63,
	0xBE, 0x00};
char CMD_IR2[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xBC, 0x00, 0xFF, 0x09, 0xF7,
	0xBC, 0x00, 0x00, 0x6C, 0x69, 0x62, 0x6E, 0x66, 0x63,
	0xBC, 0x00};

const char CMD_IS3[] = {
	0x00, 0x00, 0xFF, 0x02, 0xFE,
	0xD4, 0x02,
	0x2A, 0x00};
char CMD_IR3[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xE8, 0x00, 0xFF, 0x06, 0xFA,
	0xE8, 0x00, 0x32, 0x01, 0x06, 0x07,
	0xE8, 0x00};

const char CMD_IS4[] = {
	0x00, 0x00, 0xFF, 0x03, 0xFD,
	0xD4, 0x12, 0x14,
	0x06, 0x00};
char CMD_IR4[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x18, 0x00, 0xFF, 0x02, 0xFE,
	0x18, 0x00,
	0x18, 0x00};

const char CMD_IS5[] = {
	0x00, 0x00, 0xFF, 0x0C, 0xF4,
	0xD4, 0x06, 0x63, 0x02, 0x63, 0x03, 0x63, 0x0D, 0x63, 0x38, 0x63, 0x3D,
	0xB0, 0x00};
char CMD_IR5[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x24, 0x00, 0xFF, 0x07, 0xF9,
	0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x24, 0x00};

const char CMD_IS6[] = {
	0x00, 0x00, 0xFF, 0x08, 0xF8,
	0xD4, 0x08, 0x63, 0x02, 0x80, 0x63, 0x03, 0x80,
	0x59, 0x00};
char CMD_IR6[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x22, 0x00, 0xFF, 0x02, 0xFE,
	0x22, 0x00,
	0x22, 0x00};

const char CMD_IS7[] = {
	0x00, 0x00, 0xFF, 0x04, 0xFC,
	0xD4, 0x32, 0x01, 0x00,
	0xF9, 0x00};
char CMD_IR7[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xF8, 0x00, 0xFF, 0x02, 0xFE,
	0xF8, 0x00,
	0xF8, 0x00};

const char CMD_IS8[] = {
	0x00, 0x00, 0xFF, 0x04, 0xFC,
	0xD4, 0x32, 0x01, 0x01,
	0xF8, 0x00};
char CMD_IR8[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xF8, 0x00, 0xFF, 0x02, 0xFE,
	0xF8, 0x00,
	0xF8, 0x00};

const char CMD_IS9[] = {
	0x00, 0x00, 0xFF, 0x06, 0xFA,
	0xD4, 0x32, 0x05, 0xFF, 0xFF, 0xFF,
	0xF8, 0x00};
char CMD_IR9[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0xF8, 0x00, 0xFF, 0x02, 0xFE,
	0xF8, 0x00,
	0xF8, 0x00};

const char CMD_IS10[] = {
	0x00, 0x00, 0xFF, 0x0E, 0xF2,
	0xD4, 0x06, 0x63, 0x02, 0x63, 0x03, 0x63, 0x05, 0x63, 0x38, 0x63, 0x3C, 0x63, 0x3D,
	0x19, 0x00};
char CMD_IR10[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x24, 0x00, 0xFF, 0x08, 0xF8,
	0x24, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x24, 0x00};

const char CMD_IS11[] = {
	0x00, 0x00, 0xFF, 0x08, 0xF8,
	0xD4, 0x08, 0x63, 0x05, 0x40, 0x63, 0x3C, 0x10,
	0xCD, 0x00};
char CMD_IR11[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x22, 0x00, 0xFF, 0x02, 0xFE,
	0x22, 0x00,
	0x22, 0x00};

const char CMD_IS12[] = {
	0x00, 0x00, 0xFF, 0x0A, 0xF6,
	0xD4, 0x56, 0x00, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0x00, 0x0F,
	0xC7, 0x00};
char CMD_IR12[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};


const char RES_DETECTION[] = {
	0x6C, 0x00, 0xFF, 0x17, 0xE9,
	0x6C, 0x00, 0x00, 0x01, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x03, 0x12, 0x34, 0x56, 0x78,
	0x6C, 0x00};

static char CMD_SEND_DATA[8 + NUM_CARD_BYTES + 2] = {
	0x00, 0x00, 0xFF, (char)(3 + NUM_CARD_BYTES), (char)(-3 - NUM_CARD_BYTES),
	0xD4, 0x40, 0x01,
	0xCB, 0x00};

#if 0
void loop_initiator_once()
{
    int write_size = 0; int res_size = 0;
    uint8_t response_buffer[PN532_FRAME_MAX_LENGTH];
    
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS1, sizeof(CMD_IS1));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR1), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR1))
    {
        ESP_LOGE(TAG, "cmd1 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS2, sizeof(CMD_IS2));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR2), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR2))
    {
        ESP_LOGE(TAG, "cmd2 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS3, sizeof(CMD_IS3));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR3), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR3))
    {
        ESP_LOGE(TAG, "cmd3 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS4, sizeof(CMD_IS4));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR4), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR4))
    {
        ESP_LOGE(TAG, "cmd4 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS5, sizeof(CMD_IS5));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR5), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR5))
    {
        ESP_LOGE(TAG, "cmd5 error");
        return;
    }    

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS6, sizeof(CMD_IS6));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR6), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR6))
    {
        ESP_LOGE(TAG, "cmd6 error");
        return;
    }    

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS7, sizeof(CMD_IS7));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR7), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR7))
    {
        ESP_LOGE(TAG, "cmd7 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS8, sizeof(CMD_IS8));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR8), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR8))
    {
        ESP_LOGE(TAG, "cmd8 error");
        return;
    }
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS9, sizeof(CMD_IS9));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR9), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR9))
    {
        ESP_LOGE(TAG, "cmd9 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS10, sizeof(CMD_IS10));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR10), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR10))
    {
        ESP_LOGE(TAG, "cmd10 error");
        return;
    }
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS11, sizeof(CMD_IS11));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR11), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR11))
    {
        ESP_LOGE(TAG, "cmd11 error");
        return;
    }
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS12, sizeof(CMD_IS12));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR12), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR12))
    {
        ESP_LOGE(TAG, "cmd12 error");
        return;
    }
//    printf("I prepare to detect target\n");
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(RES_DETECTION), pdMS_TO_TICKS(500));
    if(res_size != sizeof(RES_DETECTION))
    {
//        ESP_LOGE(TAG, "DETECTION error");
        return;
    }
//    printf("I detecion found target\n");

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_SEND_DATA, sizeof(CMD_SEND_DATA));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(response_buffer), pdMS_TO_TICKS(1000));

    printf("CMD_SEND_DATA(I) %d bytes:\n", res_size);
    for (int i = 0; i < res_size; i++)
    {
	    printf(" %02X", response_buffer[i]);
	    if (i % 8 == 7) putchar('\n');
    }
    printf("\n");
}
#endif

const char CMD_TS8[] = {
	0x00, 0x00, 0xFF, 0x08, 0xF8,
	0xD4, 0x06, 0x63, 0x05, 0x63, 0x38, 0x63, 0x3D,
	0x83, 0x00};
static char CMD_TR8[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
       	0x22, 0x00, 0xFF, 0x02, 0xFE};

const char CMD_TS9[] = {
	0x00, 0x00, 0xFF, 0x05, 0xFB,
	0xD4, 0x08, 0x63, 0x05, 0x04,
	0xB8, 0x00};
static char CMD_TR9[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x22, 0x00, 0xFF, 0x02, 0xFE,
	0x22, 0x00,
	0x22, 0x00 };

const char CMD_TS10[] = {
	0x00, 0x00, 0xFF, 0x2B, 0xD5,
	0xD4, 0x8C, 0x02,
	0x08, 0x00, 0x12, 0x34, 0x56, 0x40,
	0x01, 0xFE, 0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0x0F, 0xAB,
	0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xFF, 0x00, 0x00,
	0x04, 0x12, 0x34, 0x56, 0x78, 0x00,
	0xD0, 0x00};
static char CMD_TR10[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};


const char CMD_TSGET[] = {
	0x00, 0x00, 0xFF, 0x02, 0xFE,
	0xD4, 0x86,
	0xA6, 0x00};
char RSP_TSGET[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x1A, 0x00, 0xFF, 0x07, 0xF9,
	0x1A, 0x00, 0x00, 0x61, 0x62, 0x63, 0x64,
	0x1A, 0x00};

static char CMD_TSSEND[7 + NUM_CARD_BYTES + 2] = {
	0x00, 0x00, 0xFF, (char)(2 + NUM_CARD_BYTES), (char)(-2 - NUM_CARD_BYTES),
	0xD4, 0x8E,
};
//const char CMD_TSSEND[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD4, 0x8E, 0x64, 0x63, 0x62, 0x61, 0x14, 0x00};
const char RSP_TSSEND[] = {
	0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00,
	0x9C, 0x00, 0xFF, 0x03, 0xFD,
	0x9C, 0x00, 0x00,
	0x9C, 0x00};

#if 0
void loop_tartget_once() {
    int write_size = 0; int res_size,res_size1 = 0;
    uint8_t response_buffer[PN532_FRAME_MAX_LENGTH];
    uint8_t response_buffer1[PN532_FRAME_MAX_LENGTH];
    
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS1, sizeof(CMD_IS1));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR1), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR1))
    {
        ESP_LOGE(TAG, "cmd1 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS2, sizeof(CMD_IS2));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR2), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR2))
    {
        ESP_LOGE(TAG, "cmd2 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS3, sizeof(CMD_IS3));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR3), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR3))
    {
        ESP_LOGE(TAG, "cmd3 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS4, sizeof(CMD_IS4));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR4), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR4))
    {
        ESP_LOGE(TAG, "cmd4 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS5, sizeof(CMD_IS5));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR5), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR5))
    {
        ESP_LOGE(TAG, "cmd5 error");
        return;
    }    

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS6, sizeof(CMD_IS6));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR6), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR6))
    {
        ESP_LOGE(TAG, "cmd6 error");
        return;
    }    

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_IS7, sizeof(CMD_IS7));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_IR7), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_IR7))
    {
        ESP_LOGE(TAG, "cmd7 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_TS8, sizeof(CMD_TS8));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_TR8), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_TR8))
    {
        ESP_LOGE(TAG, "cmdT8 error");
        return;
    }
    
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_TS9, sizeof(CMD_TS9));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_TR9), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_TR9))
    {
        ESP_LOGE(TAG, "cmdT9 error");
        return;
    }

    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_TS10, sizeof(CMD_TS10));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(CMD_TR10), pdMS_TO_TICKS(100));
    if(res_size != sizeof(CMD_TR10))
    {
        ESP_LOGE(TAG, "cmd10 error");
        return;
    }

//    printf("T prepare to be detected\n");
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(RES_DETECTION), pdMS_TO_TICKS(500));
    if(res_size != sizeof(RES_DETECTION))
    {
//        ESP_LOGE(TAG, "DETECTION error");
        return;
    }
//    printf("T been detected\n");


    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_TSGET, sizeof(CMD_TSGET));
    res_size1 = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer1, sizeof response_buffer1, pdMS_TO_TICKS(50));
    write_size = uart_write_bytes(PN532_UART_PORT_NUM, (const char *)CMD_TSSEND, sizeof(CMD_TSSEND));
    res_size = uart_read_bytes(PN532_UART_PORT_NUM, response_buffer, sizeof(RSP_TSSEND), pdMS_TO_TICKS(100));
    if(res_size != sizeof(RSP_TSSEND))
    {
        ESP_LOGE(TAG, "send data error");
        return;
    }

    printf("CMD_TSGET(T) %d bytes:\n", res_size1);
    for (int i = 0; i < res_size1; i++)
    {
	    printf(" %02X", response_buffer1[i]);
	    if (i % 8 == 7) putchar('\n');
    }
    printf("\n");
}

void start_indatachange(void*)
{
//    nfc_uart_init();
    vTaskDelay(pdMS_TO_TICKS(20) * 2);

    //uart_flush_input(PN532_UART_PORT_NUM);

    // Main loop alternating between modes
    while(1) {
    
        loop_tartget_once();
        uint32_t random_n = esp_random() % 10;
        vTaskDelay(pdMS_TO_TICKS(10) * random_n); // Small gap between modes  
        loop_initiator_once(); 
    }
}
#endif

#define MAX_FRIENDS     1

// 使用统一的文件路径（来自 nfc_storage.h）
// 注意：文件系统由存储团队负责，需要先初始化 nfc_storage_init()
#define FILE_MY_CARD    NFC_FILE_MY_CARD      // "/data/nfc/my_card.bin"

static struct nfc_context
{
	int initialized;
	SemaphoreHandle_t mutex_mine;
	SemaphoreHandle_t mutex_hers;
	int card_updated;
	int num_friends;
	uint8_t cards[1 + MAX_FRIENDS][NUM_CARD_BYTES];
	
	// NFC card exchange callback
	nfc_card_exchange_callback_t exchange_callback;
	void *callback_user_data;
} g_nfc_context = {};

static inline struct nfc_context *
get_nfc_context(void)
{
	return &g_nfc_context;
}

#define TICKS_100MS     pdMS_TO_TICKS(100)

#if 1
static void dump_buffer(const uint8_t *buf, int len)
{
	for (int i = 0; i < len; ++i)
	{
		printf(" %02X", buf[i]);
		if (i % 8 == 7) putchar('\n');
	}
	putchar('\n');
}
#endif

static void nfc_initiator_task(void *arg)
{
	static uint32_t error_count = 0;  // 静态错误计数器
	const uart_port_t port = PN532_UART_PORT_NUM;
	uint8_t res[PN532_FRAME_MAX_LENGTH];
	int tx_len, rx_len;

	struct seq
	{
		const char *cmd;
		char *res;
		int cmd_len;
		int res_len;
		TickType_t ticks;
	}
	sequences[] =
	{
		{CMD_IS1, CMD_IR1, sizeof CMD_IS1, sizeof CMD_IR1, TICKS_100MS},
		{CMD_IS2, CMD_IR2, sizeof CMD_IS2, sizeof CMD_IR2, TICKS_100MS},
		{CMD_IS3, CMD_IR3, sizeof CMD_IS3, sizeof CMD_IR3, TICKS_100MS},
		{CMD_IS4, CMD_IR4, sizeof CMD_IS4, sizeof CMD_IR4, TICKS_100MS},
		{CMD_IS5, CMD_IR5, sizeof CMD_IS5, sizeof CMD_IR5, TICKS_100MS},
		{CMD_IS6, CMD_IR6, sizeof CMD_IS6, sizeof CMD_IR6, TICKS_100MS},
		{CMD_IS7, CMD_IR7, sizeof CMD_IS7, sizeof CMD_IR7, TICKS_100MS},
		{CMD_IS8, CMD_IR8, sizeof CMD_IS8, sizeof CMD_IR8, TICKS_100MS},
		{CMD_IS9, CMD_IR9, sizeof CMD_IS9, sizeof CMD_IR9, TICKS_100MS},
		{CMD_IS10, CMD_IR10, sizeof CMD_IS10, sizeof CMD_IR10, TICKS_100MS},
		{CMD_IS11, CMD_IR11, sizeof CMD_IS11, sizeof CMD_IR11, TICKS_100MS},
		{CMD_IS12, CMD_IR12, sizeof CMD_IS12, sizeof CMD_IR12, TICKS_100MS},
	};

	struct seq *p = &sequences[0];
	for (int i = 0; i < sizeof sequences / sizeof sequences[0]; ++i, ++p)
	{
		tx_len = uart_write_bytes(port, p->cmd, p->cmd_len);
		rx_len = uart_read_bytes(port, p->res, p->res_len, p->ticks);
		if (tx_len != p->cmd_len || rx_len != p->res_len)
		{
			// 只在每100次错误时打印一次日志，避免刷屏
			if (++error_count % 100 == 1) {
				ESP_LOGW(TAG, "NFC initiator: No device detected (suppressing further messages)");
			}
			return;
		}
	}
	
	// 重置错误计数器（成功通信）
	if (error_count > 0) {
		ESP_LOGI(TAG, "NFC initiator: Device connected");
		error_count = 0;
	}

	ESP_LOGD(TAG, "initiator detecting target\n");
	rx_len = uart_read_bytes(port, res, sizeof RES_DETECTION, pdMS_TO_TICKS(500));
	if (rx_len != sizeof RES_DETECTION)
	{
		ESP_LOGD(TAG, "DETECTION error");
		return;
	}
	ESP_LOGD(TAG, "initiator found target\n");

	tx_len = uart_write_bytes(port, CMD_SEND_DATA, sizeof CMD_SEND_DATA);
	rx_len = uart_read_bytes(port, res, sizeof res, pdMS_TO_TICKS(1000));
	if (tx_len != sizeof CMD_SEND_DATA) return;

	printf("target card %d bytes:\n", rx_len);
	dump_buffer(res, rx_len);
	if (res[13] != 0) return;

	if ((unsigned)(rx_len - 16) <= NUM_CARD_BYTES)
	{
		struct nfc_context *context = get_nfc_context();

		xSemaphoreTake(context->mutex_hers, portMAX_DELAY);
		memset(&context->cards[1][0], 0, NUM_CARD_BYTES);
		memcpy(&context->cards[1][0], res + 14, rx_len - 16);
		context->num_friends = 1;
		xSemaphoreGive(context->mutex_hers);
		
		// 保存朋友名片到文件系统
		nfc_card_t *friend_card = (nfc_card_t *)&context->cards[1][0];
		if (nfc_storage_save_friend_card(friend_card)) {
			ESP_LOGI(TAG, "✅✅✅ NFC CARD EXCHANGE SUCCESS (Initiator mode) ✅✅✅");
			ESP_LOGI(TAG, "📇 Friend card saved: %.20s", friend_card->pet_name);
			ESP_LOGI(TAG, "💾 Total friends: %d", context->num_friends);
			
			// 触发回调通知（LED、音效、BLE推送等）
			if (context->exchange_callback != NULL) {
				ESP_LOGI(TAG, "🔔 Triggering exchange callback notification...");
				context->exchange_callback(&context->cards[1][0], context->callback_user_data);
			}
		} else {
			ESP_LOGW(TAG, "❌ Failed to save friend card to storage");
		}
	}
}

static void nfc_target_task(void *arg)
{
	static uint32_t error_count = 0;  // 静态错误计数器
	const uart_port_t port = PN532_UART_PORT_NUM;
	uint8_t res[PN532_FRAME_MAX_LENGTH];
	int tx_len, rx_len;

	struct seq
	{
		const char *cmd;
		int cmd_len;
		int res_len;
		TickType_t ticks;
	}
	sequences[] =
	{
		{CMD_IS1, sizeof CMD_IS1, sizeof CMD_IR1, TICKS_100MS},
		{CMD_IS2, sizeof CMD_IS2, sizeof CMD_IR2, TICKS_100MS},
		{CMD_IS3, sizeof CMD_IS3, sizeof CMD_IR3, TICKS_100MS},
		{CMD_IS4, sizeof CMD_IS4, sizeof CMD_IR4, TICKS_100MS},
		{CMD_IS5, sizeof CMD_IS5, sizeof CMD_IR5, TICKS_100MS},
		{CMD_IS6, sizeof CMD_IS6, sizeof CMD_IR6, TICKS_100MS},
		{CMD_IS7, sizeof CMD_IS7, sizeof CMD_IR7, TICKS_100MS},

		{CMD_TS8, sizeof CMD_TS8, sizeof CMD_TR8, TICKS_100MS},
		{CMD_TS9, sizeof CMD_TS9, sizeof CMD_TR9, TICKS_100MS},
		{CMD_TS10, sizeof CMD_TS10, sizeof CMD_TR10, TICKS_100MS},
	};

	struct seq *p = &sequences[0];
	for (int i = 0; i < sizeof sequences / sizeof sequences[0]; ++i, ++p)
	{
		tx_len = uart_write_bytes(port, p->cmd, p->cmd_len);
		rx_len = uart_read_bytes(port, res, p->res_len, p->ticks);
		if (tx_len != p->cmd_len || rx_len != p->res_len)
		{
			// 只在每100次错误时打印一次日志，避免刷屏
			if (++error_count % 100 == 1) {
				ESP_LOGW(TAG, "NFC target: No device detected (suppressing further messages)");
			}
			return;
		}
	}
	
	// 重置错误计数器（成功通信）
	if (error_count > 0) {
		ESP_LOGI(TAG, "NFC target: Device connected");
		error_count = 0;
	}

	ESP_LOGD(TAG, "target detecting initiator\n");
	rx_len = uart_read_bytes(port, res, sizeof RES_DETECTION, pdMS_TO_TICKS(500));
	if (rx_len != sizeof RES_DETECTION)
	{
		ESP_LOGD(TAG, "DETECTION error");
		return;
	}
	ESP_LOGD(TAG, "target found initiator\n");

	tx_len = uart_write_bytes(port, CMD_TSGET, sizeof CMD_TSGET);
	rx_len = uart_read_bytes(port, res, sizeof res, pdMS_TO_TICKS(50));

	uint8_t res1[PN532_FRAME_MAX_LENGTH];
	int tx_len1 = uart_write_bytes(port, CMD_TSSEND, sizeof CMD_TSSEND);
	int rx_len1 = uart_read_bytes(port, res1, sizeof RSP_TSSEND, pdMS_TO_TICKS(100));

	if (tx_len != sizeof CMD_TSGET)
	{
		ESP_LOGE(TAG, "target get data error");
		return;
	}

	if (tx_len1 != sizeof CMD_TSSEND || rx_len1 != sizeof RSP_TSSEND)
	{
		ESP_LOGE(TAG, "target send data error");
		return;
	}

	printf("initiator card %d bytes:\n", rx_len);
	dump_buffer(res, rx_len);

	if ((unsigned)(rx_len - 20) <= NUM_CARD_BYTES)
	{
		struct nfc_context *context = get_nfc_context();

		xSemaphoreTake(context->mutex_hers, portMAX_DELAY);
		memset(&context->cards[1][0], 0, NUM_CARD_BYTES);
		memcpy(&context->cards[1][0], res + 18, rx_len - 20);
		context->num_friends = 1;
		xSemaphoreGive(context->mutex_hers);
		
		// 保存朋友名片到文件系统
		nfc_card_t *friend_card = (nfc_card_t *)&context->cards[1][0];
		if (nfc_storage_save_friend_card(friend_card)) {
			ESP_LOGI(TAG, "✅✅✅ NFC CARD EXCHANGE SUCCESS (Target mode) ✅✅✅");
			ESP_LOGI(TAG, "📇 Friend card saved: %.20s", friend_card->pet_name);
			ESP_LOGI(TAG, "💾 Total friends: %d", context->num_friends);
		} else {
			ESP_LOGW(TAG, "❌ Failed to save friend card to storage");
		}
		
		// Trigger callback notification
		if (context->exchange_callback != NULL)
		{
			ESP_LOGI(TAG, "🔔 Triggering exchange callback notification...");
			context->exchange_callback(&context->cards[1][0], context->callback_user_data);
		}
	}
}

static void nfc_task(void *arg)
{
	struct nfc_context *context = get_nfc_context();
	vTaskDelay(pdMS_TO_TICKS(40));
	
	ESP_LOGI(TAG, "🔄 NFC exchange task started - monitoring for nearby devices...");
	uint32_t cycle_count = 0;

	for (;;)
	{
		cycle_count++;
		xSemaphoreTake(context->mutex_mine, portMAX_DELAY);
		if (context->card_updated)
		{
			uint8_t checksum = 0;
			for (int i = 5; i < 8; ++i)
			{
				checksum += CMD_SEND_DATA[i];
			}
			for (int i = 0; i < NUM_CARD_BYTES; ++i)
			{
				checksum += (CMD_SEND_DATA[8 + i] = context->cards[0][i]);
			}
			CMD_SEND_DATA[8 + NUM_CARD_BYTES] = -checksum;
			//dump_buffer((const uint8_t *)CMD_SEND_DATA, sizeof CMD_SEND_DATA);

			checksum = 0;
			for (int i = 5; i < 7; ++i)
			{
				checksum += CMD_TSSEND[i];
			}
			for (int i = 0; i < NUM_CARD_BYTES; ++i)
			{
				checksum += (CMD_TSSEND[7 + i] = context->cards[0][NUM_CARD_BYTES - 1 - i]);
			}
			CMD_TSSEND[7 + NUM_CARD_BYTES] = -checksum;
			//dump_buffer((const uint8_t *)CMD_TSSEND, sizeof CMD_TSSEND);

			context->card_updated = 0;
		}
		xSemaphoreGive(context->mutex_mine);

		const uart_port_t port = PN532_UART_PORT_NUM;
//		uart_flush(port);
//		uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
		nfc_target_task(arg);
		vTaskDelay(pdMS_TO_TICKS(10) * (esp_random() % 10));
//		uart_flush(port);
//		uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
		nfc_initiator_task(arg);
		
		// 每100个周期打印一次状态（避免刷屏）
		if (cycle_count % 100 == 0) {
			ESP_LOGI(TAG, "📡 NFC monitoring active (cycle: %lu, friends: %d)", 
			         cycle_count, context->num_friends);
		}
	}
}

esp_err_t nfc_set_card(const uint8_t card[])
{
	struct nfc_context *context = get_nfc_context();
	if (context->initialized == 0)
	{
		ESP_LOGE(TAG, "nfc not initialized");
		return ESP_FAIL;
	}

	// 使用存储API保存本地名片
	nfc_card_t *my_card = (nfc_card_t *)card;
	if (!nfc_storage_save_my_card(my_card)) {
		ESP_LOGE(TAG, "Failed to save my card to storage");
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "✅ My card updated: %.20s", my_card->pet_name);

	xSemaphoreTake(context->mutex_mine, portMAX_DELAY);
	memcpy(&context->cards[0][0], card, NUM_CARD_BYTES);
	context->card_updated = 1;
	xSemaphoreGive(context->mutex_mine);

	return ESP_OK;
}

esp_err_t nfc_get_card(uint8_t card[])
{
	struct nfc_context *context = get_nfc_context();
	if (context->num_friends == 0)
	{
		ESP_LOGE(TAG, "no friend yet");
		return ESP_FAIL;
	}

	xSemaphoreTake(context->mutex_hers, portMAX_DELAY);
	memcpy(card, &context->cards[1][0], NUM_CARD_BYTES);
	xSemaphoreGive(context->mutex_hers);
	return ESP_OK;
}

/**
 * @brief Register callback for NFC card exchange events
 * 
 * The callback will be invoked whenever a friend's card is successfully
 * received via NFC exchange (both Initiator and Target modes).
 * 
 * @param callback Function pointer to callback (NULL to disable)
 * @param user_data User data to pass to callback (can be NULL)
 * @return ESP_OK on success, ESP_FAIL if NFC not initialized
 */
esp_err_t nfc_register_exchange_callback(nfc_card_exchange_callback_t callback, void *user_data)
{
	struct nfc_context *context = get_nfc_context();
	if (context->initialized == 0)
	{
		ESP_LOGE(TAG, "nfc not initialized");
		return ESP_FAIL;
	}
	
	xSemaphoreTake(context->mutex_hers, portMAX_DELAY);
	context->exchange_callback = callback;
	context->callback_user_data = user_data;
	xSemaphoreGive(context->mutex_hers);
	
	ESP_LOGI(TAG, "NFC exchange callback registered at %p", callback);
	return ESP_OK;
}

/**
 * @brief Unregister NFC card exchange callback
 * @return ESP_OK on success, ESP_FAIL if NFC not initialized
 */
esp_err_t nfc_unregister_exchange_callback(void)
{
	struct nfc_context *context = get_nfc_context();
	if (context->initialized == 0)
	{
		ESP_LOGE(TAG, "nfc not initialized");
		return ESP_FAIL;
	}
	
	xSemaphoreTake(context->mutex_hers, portMAX_DELAY);
	context->exchange_callback = NULL;
	context->callback_user_data = NULL;
	xSemaphoreGive(context->mutex_hers);
	
	ESP_LOGI(TAG, "NFC exchange callback unregistered");
	return ESP_OK;
}

esp_err_t nfc_init(void)
{
	esp_err_t err = ESP_OK;
	struct nfc_context *context = get_nfc_context();

	if (context->initialized) return err;

#if CONFIG_UART_ISR_IN_IRAM
	intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

	err = uart_driver_install(PN532_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "uart_driver_install() failed %d", err);
		return err;
	}

	/* Configure parameters of an UART driver,
	 * communication pins and install the driver */
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity    = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	err = uart_param_config(PN532_UART_PORT_NUM, &uart_config);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "uart_param_config() failed %d", err);
		return err;
	}

	err = uart_set_pin(PN532_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, -1, -1);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "uart_set_pin() failed %d", err);
		return err;
	}

	ESP_LOGI(TAG, "NFC UART initialized: TX=GPIO%d, RX=GPIO%d", ECHO_TEST_TXD, ECHO_TEST_RXD);

	// 初始化NFC存储（创建目录等）
	if (!nfc_storage_init()) {
		ESP_LOGW(TAG, "NFC storage init failed - file system may not be ready");
	}

	// 从文件系统加载本机名片
	nfc_card_t my_card;
	if (nfc_storage_load_my_card(&my_card)) {
		// 加载成功，复制到context
		memcpy(&context->cards[0][0], &my_card, sizeof(nfc_card_t));
		ESP_LOGI(TAG, "Loaded my card from storage: %.20s", my_card.pet_name);
	} else {
		// 加载失败，使用默认测试数据
		ESP_LOGW(TAG, "No card in storage, using default test data");
		for (int i = 0; i < NUM_CARD_BYTES; ++i) {
			context->cards[0][i] = 0x00 + i;
		}
	}

	context->card_updated = 1;
	context->mutex_mine = xSemaphoreCreateMutex();
	context->mutex_hers = xSemaphoreCreateMutex();
	xTaskCreate(nfc_task, "nfc_task", 4096, NULL, 5, NULL);

	context->initialized = 1;
	
	ESP_LOGI(TAG, "========================================");
	ESP_LOGI(TAG, "✅ NFC SYSTEM INITIALIZED");
	ESP_LOGI(TAG, "📡 Ready to exchange cards");
	ESP_LOGI(TAG, "👥 Bring two devices within 10cm to exchange");
	ESP_LOGI(TAG, "========================================");
	
	return ESP_OK;
}

