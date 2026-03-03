/*
 * Heart Rate Sensor Driver Implementation
 */

#include "drivers/heart_rate_sensor.h"
#include "esp_log.h"
#include "esp_random.h"

static const char* TAG = "HEART_RATE_DRIVER";

typedef struct {
    bool initialized;
    heart_rate_callback_t callback;
    uint16_t current_bpm;
    bool simulation_mode;
    bool measuring;
} heart_rate_driver_t;

static heart_rate_driver_t g_hr_driver = {0};

esp_err_t heart_rate_driver_init(bool simulation_mode) {
    if (g_hr_driver.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing heart rate driver");
    
    g_hr_driver.simulation_mode = simulation_mode;
    g_hr_driver.current_bpm = 75;
    g_hr_driver.measuring = false;
    g_hr_driver.initialized = true;
    
    return ESP_OK;
}

esp_err_t heart_rate_driver_deinit(void) {
    if (!g_hr_driver.initialized) {
        return ESP_OK;
    }
    
    g_hr_driver.initialized = false;
    g_hr_driver.measuring = false;
    g_hr_driver.callback = NULL;
    
    return ESP_OK;
}

esp_err_t heart_rate_driver_start_measurement(void) {
    if (!g_hr_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_hr_driver.simulation_mode) {
        g_hr_driver.current_bpm = 60 + (esp_random() % 40);
    }
    
    g_hr_driver.measuring = true;
    return ESP_OK;
}

esp_err_t heart_rate_driver_stop_measurement(void) {
    if (!g_hr_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_hr_driver.measuring = false;
    return ESP_OK;
}

uint16_t heart_rate_driver_get_bpm(void) {
    if (!g_hr_driver.initialized) {
        return 0;
    }
    return g_hr_driver.current_bpm;
}

esp_err_t heart_rate_driver_set_callback(heart_rate_callback_t callback) {
    if (!g_hr_driver.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_hr_driver.callback = callback;
    return ESP_OK;
}

esp_err_t heart_rate_driver_update_simulation(void) {
    if (!g_hr_driver.initialized || !g_hr_driver.simulation_mode) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int change = (esp_random() % 10) - 5;
    g_hr_driver.current_bpm += change;
    
    if (g_hr_driver.current_bpm < 60) g_hr_driver.current_bpm = 60;
    if (g_hr_driver.current_bpm > 100) g_hr_driver.current_bpm = 100;
    
    if (g_hr_driver.callback) {
        g_hr_driver.callback(g_hr_driver.current_bpm);
    }
    
    return ESP_OK;
}

bool heart_rate_driver_is_initialized(void) {
    return g_hr_driver.initialized;
}

bool heart_rate_driver_is_measuring(void) {
    return g_hr_driver.initialized && g_hr_driver.measuring;
}
