/**
 * @file voice_recognition.c
 * @brief Voice Recognition GPIO Interrupt Driver Implementation
 * 
 * Handles voice wakeup detection through GPIO44 hardware interrupt.
 * When voice keyword "Blinko" is detected, GPIO44 goes LOW (active low).
 * 
 * Hardware: GPIO44 (U0RXD) connected to voice module output
 * Mode: Hardware interrupt (falling edge trigger)
 * Debounce: 500ms software debounce
 * 
 * Advantages over polling:
 * - Instant response (<1ms vs 0-100ms)
 * - No missed triggers (polling can miss <100ms pulses)
 * - Lower CPU usage (no continuous polling)
 * - Lower power consumption
 */

#include "drivers/voice_recognition.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app/trigger_detector.h"

#define VOICE_GPIO_NUM          GPIO_NUM_7   // Voice module output pin (active low) - 硬件变更
#define VOICE_DEBOUNCE_MS       500          // 500ms debounce time

static const char *TAG = "VOICE";
static bool g_voice_initialized = false;
static TaskHandle_t g_voice_task_handle = NULL;
static uint32_t g_last_trigger_time = 0;

static void voice_sensor_evt_task(void* param);
static void IRAM_ATTR voice_gpio_isr_handler(void* arg);

esp_err_t hw_aduio_sensor_init(void)
{
    if (g_voice_initialized) {
        ESP_LOGW(TAG, "Voice sensor already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing voice sensor (GPIO7 interrupt mode)...");
    
    // Configure GPIO7 as input with falling edge interrupt
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << VOICE_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  
        .intr_type = GPIO_INTR_NEGEDGE  // Trigger on falling edge (HIGH->LOW)
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO%d: %s", 
                 VOICE_GPIO_NUM, esp_err_to_name(ret));
        return ret;
    }
    
    // Install GPIO ISR service
    ret = gpio_install_isr_service(0);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "GPIO ISR service already installed");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Create voice handling task
    BaseType_t task_ret = xTaskCreate(
        voice_sensor_evt_task,
        "voice_task",
        3072,
        NULL,
        5,  // Same priority as NFC
        &g_voice_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create voice task");
        return ESP_FAIL;
    }
    
    // Add ISR handler
    ret = gpio_isr_handler_add(VOICE_GPIO_NUM, voice_gpio_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add GPIO ISR handler: %s", esp_err_to_name(ret));
        vTaskDelete(g_voice_task_handle);
        g_voice_task_handle = NULL;
        return ret;
    }
    
    // Enable interrupt
    ret = gpio_intr_enable(VOICE_GPIO_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable GPIO interrupt: %s", esp_err_to_name(ret));
        gpio_isr_handler_remove(VOICE_GPIO_NUM);
        vTaskDelete(g_voice_task_handle);
        g_voice_task_handle = NULL;
        return ret;
    }
    
    g_voice_initialized = true;

    int level = gpio_get_level(VOICE_GPIO_NUM);
    ESP_LOGI(TAG, "✅ Voice sensor initialized (interrupt mode):");
    ESP_LOGI(TAG, "   - GPIO%d: Voice output (LOW=keyword detected)", VOICE_GPIO_NUM);
    ESP_LOGI(TAG, "   - Current level: %d (should be 1 with pull-up)", level);
    ESP_LOGI(TAG, "   - Trigger: Falling edge (HIGH→LOW)");
    ESP_LOGI(TAG, "   - Debounce: %d ms", VOICE_DEBOUNCE_MS);
    ESP_LOGI(TAG, "   - ISR: Installed and enabled");
    
    return ESP_OK;
}

bool hw_voice_sensor_is_detected(void)
{
    if (!g_voice_initialized) {
        ESP_LOGW(TAG, "Voice sensor not initialized");
        return false;
    }
    
    int level = gpio_get_level(VOICE_GPIO_NUM);
    return (level == 0);  // Active low: 0 = keyword detected
}

/**
 * @brief GPIO interrupt handler (runs in IRAM)
 * 
 * Triggered when voice module pulls GPIO44 LOW (keyword detected).
 * Sends notification to voice task for processing.
 */
static void IRAM_ATTR voice_gpio_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Notify task to handle voice trigger
    if (g_voice_task_handle != NULL) {
        xTaskNotifyFromISR(g_voice_task_handle, 1, eSetBits, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Voice trigger handling task
 * 
 * Processes voice recognition triggers with debouncing and reports to trigger detector.
 */
static void voice_sensor_evt_task(void* param)
{
    uint32_t notification_value;
    
    ESP_LOGI(TAG, "🎤 Voice trigger task started");

    while (1) {
        // Wait for interrupt notification
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notification_value, portMAX_DELAY) == pdTRUE) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            
            // Debounce check
            if (now - g_last_trigger_time < VOICE_DEBOUNCE_MS) {
                ESP_LOGD(TAG, "Voice trigger ignored (debounce: %lu ms)", now - g_last_trigger_time);
                continue;
            }
            
            g_last_trigger_time = now;
            
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "🎤 Voice Recognition Triggered!");
            ESP_LOGI(TAG, "   Time: %lu ms", now);
            ESP_LOGI(TAG, "   GPIO%d: LOW detected", VOICE_GPIO_NUM);
            ESP_LOGI(TAG, "========================================");
            
            // Report to trigger detector
            trigger_detector_manual_trigger(TRIGGER_TYPE_VOICE, 100);
        }
    }
}
