/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "heart_rate.h"
#include "led.h"
#include "audio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "esp_task_wdt.h"

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

/* Private functions */
/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void) {
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void) {
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

static void heart_rate_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "heart rate task has been started!");

    /* Loop forever */
    while (1) {
        /* Update heart rate value every 1 second */
        update_heart_rate();
        //ESP_LOGI(TAG, "heart rate updated to %d", get_heart_rate());

        /* Send heart rate indication if enabled */
        send_heart_rate_indication();

        /* Sleep */
        vTaskDelay(HEART_RATE_TASK_PERIOD);
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}

#define DISTANCE_GPIO GPIO_NUM_4
#define TOUCH_GPIO GPIO_NUM_5
#define NO_GPIO GPIO_NUM_6
// #define YES_GPIO GPIO_NUM_7  // 已移除：GPIO7 现用于天问语音模块中断

#define EVENT_BIT_DISTANCE  (1<<0)
#define EVENT_BIT_TOUCH  (1<<1)
#define EVENT_BIT_NO  (1<<2)
// #define EVENT_BIT_YES  (1<<3)  // 已移除

static EventGroupHandle_t gpio_event=NULL;

static void IRAM_ATTR gpio_isr_handler(void *pvparam)
{
	BaseType_t  pxHigherPriorityTaskWoken = EVENT_BIT_TOUCH; 
    uint32_t gpio_num = (uint32_t) pvparam;
    uint32_t gpio_bit =  0;
    if (gpio_num == TOUCH_GPIO) {
        gpio_bit = EVENT_BIT_TOUCH;
    }else if (gpio_num == NO_GPIO) {
        gpio_bit = EVENT_BIT_NO;
    }/* YES_GPIO 已移除 - GPIO7 现用于语音模块
    else if (gpio_num == YES_GPIO) {
        gpio_bit = EVENT_BIT_YES;
    }*/
    else if (gpio_num == DISTANCE_GPIO) {
        gpio_bit = EVENT_BIT_DISTANCE;
    }
	xEventGroupSetBitsFromISR(gpio_event, gpio_bit, &pxHigherPriorityTaskWoken);
}


typedef enum{
    MIN_PWM_RATE=0,
    LOW_PWM_RATE=1024,      //25%
    MID_PWM_RATE=4096,      //50%
    HIGH_PWM_RATE=8192,     //75%
    MAX_PWM_RATE=8192       //MAX PWM_RATE = 2 ** 13 = 8192
}PWM_RATE_T;
 




static void yes_no_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, 
        .freq_hz = 1000,                      
        .speed_mode = LEDC_LOW_SPEED_MODE,    
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK             
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,        
        .duty       = 0,                       
        .gpio_num   = 1,                       
        .speed_mode = LEDC_LOW_SPEED_MODE,    
        .hpoint     = 0,                       
        .timer_sel  = LEDC_TIMER_0             
    }; 

    ledc_channel_config_t ledc_channel1 = {
        .channel    = LEDC_CHANNEL_1,        
        .duty       = 0,                       
        .gpio_num   = 2,                       
        .speed_mode = LEDC_LOW_SPEED_MODE,    
        .hpoint     = 0,                       
        .timer_sel  = LEDC_TIMER_0             
    }; 
    ledc_channel_config_t ledc_channel2 = {
        .channel    = LEDC_CHANNEL_2,        
        .duty       = 0,                       
        .gpio_num   = 42,                       
        .speed_mode = LEDC_LOW_SPEED_MODE,    
        .hpoint     = 0,                       
        .timer_sel  = LEDC_TIMER_0             
    }; 
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel1));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel2));
}

void PwmDev_Node(PWM_RATE_T rate)
{

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, rate));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}

void PwmDev_left()
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, MID_PWM_RATE));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
}

void PwmDev_right()
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, MID_PWM_RATE));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2));
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
}

int node_count = 0; 
void stop_nod()
{
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);

}

void event_task(void *pvparam)
{
	ESP_LOGI(TAG, "GPIO event task started");
	
	// Register this task with the watchdog timer
	esp_err_t wdt_ret = esp_task_wdt_add(NULL);
	if (wdt_ret != ESP_OK) {
		ESP_LOGW(TAG, "Failed to register GPIO event task with watchdog: %s", esp_err_to_name(wdt_ret));
	}
	
	while(1){
        // Use timeout instead of portMAX_DELAY to prevent watchdog issues
        EventBits_t notify_bits = xEventGroupWaitBits(gpio_event, 
                                                      EVENT_BIT_DISTANCE | EVENT_BIT_TOUCH | EVENT_BIT_NO | EVENT_BIT_YES, 
                                                      pdTRUE, pdFALSE, pdMS_TO_TICKS(1000));
        
        // Reset watchdog periodically
        esp_task_wdt_reset();
        
        if(notify_bits == 0) {
            // Timeout occurred, just continue
            continue;
        }
	
        if(notify_bits & EVENT_BIT_TOUCH ){
            ESP_LOGD(TAG, "EVENT_BIT_TOUCH ");
            continue;  
        }
        if(notify_bits & EVENT_BIT_NO ){
            ESP_LOGD(TAG, "EVENT_BIT_NO ");
            continue;  
        }
        if(notify_bits & EVENT_BIT_YES ){
            ESP_LOGD(TAG, "EVENT_BIT_YES ");
            node_count++;
            if (node_count == 6) {
                stop_nod();
                node_count = 0;
            }
            continue;  
        }
        
        if(notify_bits & EVENT_BIT_DISTANCE ){
            ESP_LOGD(TAG, "EVENT_BIT_DISTANCE %d", gpio_get_level(DISTANCE_GPIO));
        }
        
        // Give other tasks a chance to run
        vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void gpio_init(void)
{


	gpio_config_t key_config1 = {
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask =  (1ULL <<  NO_GPIO) | (1ULL << TOUCH_GPIO),  // 移除 YES_GPIO
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_POSEDGE
	};
	gpio_config_t key_config2= {
		.mode = GPIO_MODE_INPUT,
		.pin_bit_mask = (1ULL << DISTANCE_GPIO),
		.pull_up_en = GPIO_PULLUP_ENABLE,
		.pull_down_en = GPIO_PULLDOWN_ENABLE,
		.intr_type = GPIO_INTR_ANYEDGE
	};
	gpio_config(&key_config1);
	gpio_config(&key_config2);

	/*创建事件组，用于发出中断事件*/
	gpio_event = xEventGroupCreate();

	/*任务创建 - 增加堆栈大小防止溢出*/
	xTaskCreatePinnedToCore(event_task, "event_task" ,3072, NULL, 1, NULL, 1);

	/*安装中断服务，此函数进行中断寄存器初始化，并注册中断*/
	gpio_install_isr_service(0);
    // gpio_isr_handler_add(YES_GPIO, gpio_isr_handler, (void*) YES_GPIO);  // 已移除：GPIO7 用于语音模块
    gpio_isr_handler_add(NO_GPIO, gpio_isr_handler, (void*) NO_GPIO);
    gpio_isr_handler_add(TOUCH_GPIO, gpio_isr_handler, (void*) TOUCH_GPIO);
    gpio_isr_handler_add(DISTANCE_GPIO, gpio_isr_handler, (void*) DISTANCE_GPIO);

}
#define EX_UART_NUM UART_NUM_2
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;

void uart_init(){
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart0_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, 6, 7, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    //Set uart pattern detect function.
    uart_enable_pattern_det_baud_intr(EX_UART_NUM, '+', PATTERN_CHR_NUM, 9, 0, 0);
    //Reset the pattern queue length to record at most 20 pattern positions.
    uart_pattern_queue_reset(EX_UART_NUM, 20);
}

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    size_t buffered_size;
    uint8_t* dtmp = (uint8_t*) malloc(RD_BUF_SIZE);
    assert(dtmp);
    
    ESP_LOGI(TAG, "UART event task started");
    
    for (;;) {
        //Waiting for UART event with timeout to prevent watchdog issues
        if (xQueueReceive(uart0_queue, (void *)&event, pdMS_TO_TICKS(1000))) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGD(TAG, "uart[%d] event:", EX_UART_NUM);
            switch (event.type) {
            //Event of UART receiving data
            case UART_DATA:
                if (event.size > 0 && event.size < RD_BUF_SIZE) {
                    uart_read_bytes(EX_UART_NUM, dtmp, event.size, pdMS_TO_TICKS(100));
                    if (event.size >= 2 && dtmp[0] == 0 && dtmp[1] == 1) {
                        PwmDev_Node(MID_PWM_RATE);
                    } else if (event.size >= 2 && dtmp[0] == 0 && dtmp[1] == 3) {
                        PwmDev_left();
                        PwmDev_right();
                        PwmDev_left();
                    }
                }
                break;
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "hw fifo overflow");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart0_queue);
                break;
            //Event of UART ring buffer full
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "ring buffer full");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart0_queue);
                break;
            //Event of UART RX break detected
            case UART_BREAK:
                ESP_LOGD(TAG, "uart rx break");
                // Don't do anything that might block here
                break;
            //Event of UART parity check error
            case UART_PARITY_ERR:
                ESP_LOGW(TAG, "uart parity error");
                break;
            //Event of UART frame error
            case UART_FRAME_ERR:
                ESP_LOGW(TAG, "uart frame error");
                break;
            //UART_PATTERN_DET
            case UART_PATTERN_DET:
                uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
                int pos = uart_pattern_pop_pos(EX_UART_NUM);
                ESP_LOGD(TAG, "[UART PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);
                if (pos == -1) {
                    uart_flush_input(EX_UART_NUM);
                } else if (pos < RD_BUF_SIZE) {
                    uart_read_bytes(EX_UART_NUM, dtmp, pos, pdMS_TO_TICKS(100));
                    uint8_t pat[PATTERN_CHR_NUM + 1];
                    memset(pat, 0, sizeof(pat));
                    uart_read_bytes(EX_UART_NUM, pat, PATTERN_CHR_NUM, pdMS_TO_TICKS(100));
                    ESP_LOGD(TAG, "read data: %s", dtmp);
                    ESP_LOGD(TAG, "read pat : %s", pat);
                }
                break;
            //Others
            default:
                ESP_LOGD(TAG, "uart event type: %d", event.type);
                break;
            }
        }
        
        // Give other tasks a chance to run
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}


void app_main(void) {
    /* Local variables */
    int rc;
    esp_err_t ret;

    /* LED initialization */
    led_init();
    

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* GATT server initialization */
    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    /* Audio GATT service initialization */
    rc = audio_gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize Audio GATT service, error code: %d", rc);
        return;
    }

    /* Audio hardware initialization */
    ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize audio hardware, error code: %d", ret);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    xTaskCreate(heart_rate_task, "Heart Rate", 3*1024, NULL, 4, NULL);

    uart_init();
    //Create a task to handler UART event from ISR (increased stack size to prevent overflow)
    xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 1, NULL);
    
    gpio_init();
    yes_no_init();
    //nfc_init();
    
    /* Create an event loop task to handle ongoing operations - matching gatt_server pattern */
    /* IMPORTANT: Create event_task AFTER gpio_init() to ensure gpio_event is initialized */
    xTaskCreate(event_task, "Event Task", 4*1024, NULL, 3, NULL);
    
    // Reset watchdog to prevent immediate restart
    esp_task_wdt_reset();
    ESP_LOGI(TAG, "Main initialization completed successfully");
    
    return;
}
