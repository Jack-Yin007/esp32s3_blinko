#include "app/pet_config.h"
#include "esp_log.h"

static const char* TAG = "PET_CONFIG";

esp_err_t pet_config_init(void)
{
    ESP_LOGI(TAG, "Pet configuration system initialized");
    return ESP_OK;
}