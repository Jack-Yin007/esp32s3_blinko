/**
 * @file servo_motor.c
 * @brief 双马达驱动 - 点头马达 + 摇头马达
 * 
 * 硬件连接：
 * - 点头马达: IN1-GPIO46, 
 * - 摇头马达: IN1-GPIO21, 
 * 
 * 
 * 
 * 
 */

#include "drivers/hardware_interface.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Servo_Motor";


#if SUPPORT_270_SERVO_MOTOR

/* ==================== 硬件配置 ==================== */
// 点头马达
#define NOD_MOTOR_IN1       GPIO_NUM_46
#define NOD_LEDC_TIMER      LEDC_TIMER_2
#define NOD_LEDC_CH_IN1     LEDC_CHANNEL_4


// 摇头马达
#define SHAKE_MOTOR_IN1     GPIO_NUM_21
#define SHAKE_LEDC_TIMER    LEDC_TIMER_3
#define SHAKE_LEDC_CH_IN1   LEDC_CHANNEL_6


// PWM配置
#define MOTOR_LEDC_MODE     LEDC_LOW_SPEED_MODE

#define MOTOR_LEDC_DUTY_RES LEDC_TIMER_13_BIT    
#define MOTOR_RESOLUTION  8192
#define MOTOR_PWM_FREQ     50//               

// central postion is 615
#define DELTA_DUTY      8
#define POSTION_DUTY  (615 - DELTA_DUTY)

#define SERVO_MOTOR_NORMAL_ACTION_DELAY     500
#define SERVO_MOTOR_INIT_ACTION_DELAY       100

/* ==================== 全局变量 ==================== */
static bool g_servo_motor_initialized = false;

/* ==================== 驱动层基本控制 ==================== */

/**
 * @brief 初始化马达PWM
 */
esp_err_t hw_servo_motor_init(void)
{
    if (g_servo_motor_initialized) {
        ESP_LOGW(TAG, "Motor already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing motor driver...");

    // 配置点头马达定时器
    ledc_timer_config_t nod_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = NOD_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&nod_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure nod motor timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置摇头马达定时器
    ledc_timer_config_t shake_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = SHAKE_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&shake_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure shake motor timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置点头马达IN1通道
    ledc_channel_config_t nod_ch_in1 = {
        .gpio_num   = NOD_MOTOR_IN1,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = NOD_LEDC_CH_IN1,
        .timer_sel  = NOD_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&nod_ch_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure nod motor IN1: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置摇头马达IN1通道
    ledc_channel_config_t shake_ch_in1 = {
        .gpio_num   = SHAKE_MOTOR_IN1,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = SHAKE_LEDC_CH_IN1,
        .timer_sel  = SHAKE_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&shake_ch_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure shake motor IN1: %s", esp_err_to_name(ret));
        return ret;
    }


    g_servo_motor_initialized = true;
    ESP_LOGI(TAG, "servo Motor driver initialized successfully");
    ESP_LOGI(TAG, "  Nod motor: IN1=GPIO%d, ", NOD_MOTOR_IN1);
    ESP_LOGI(TAG, "  Shake motor: IN1=GPIO%d, ", SHAKE_MOTOR_IN1);

    return ESP_OK;
}

bool hw_servo_motor_is_initialized(void)
{
    return g_servo_motor_initialized;
}


typedef struct
{
    uint8_t angle;
    uint32_t duty;
    
}motor_angle_t;

motor_angle_t motor_angle[] = 
{
    {ANGLE_INIT,        POSTION_DUTY},   
    {ANGLE_10_LEFT,     POSTION_DUTY - 30},   
    {ANGLE_10_RIGHT,    POSTION_DUTY + 30},
    {ANGLE_15_LEFT,     POSTION_DUTY - 45},   
    {ANGLE_15_RIGHT,    POSTION_DUTY + 45},
    {ANGLE_20_LEFT,     POSTION_DUTY - 60},   
    {ANGLE_20_RIGHT,    POSTION_DUTY + 60},
    {ANGLE_30_LEFT,     POSTION_DUTY - 90},   
    {ANGLE_30_RIGHT,    POSTION_DUTY + 90},
    {ANGLE_45_LEFT,     POSTION_DUTY - 135},   
    {ANGLE_45_RIGHT,    POSTION_DUTY + 135},
    {ANGLE_60_LEFT,     POSTION_DUTY - 180},   
    {ANGLE_60_RIGHT,    POSTION_DUTY + 180}

};

#define SEEK_ANGLE_DUTY(a) \
    ({ \
        uint16_t result = 0; \
        for (uint8_t i = 0; i < sizeof(motor_angle)/sizeof(motor_angle[0]); i++) { \
            if (motor_angle[i].angle == (a)) { \
                result = motor_angle[i].duty; \
                break; \
            } \
        } \
        result; \
    })


void start_the_shake(uint8_t type)
{
    if (type == 0)
    return;
    if (type % 3 == 0 ) {
        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -2 ));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -1 ));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    else {
        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s set duty %d ", __func__, SEEK_ANGLE_DUTY(type));
}

void stop_the_shake(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s", __func__);
}

void start_the_node(uint8_t type)
{
    if (type == 0)
    return;
    if (type % 3 == 0 ) {
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -2 ));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -1 ));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    else {
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s set duty %d ", __func__, SEEK_ANGLE_DUTY(type));

}

void stop_the_node(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s", __func__);
}

void start_shake_and_node(uint8_t shake_type, uint8_t node_type)
{
    if (shake_type == ANGLE_INIT)
        start_the_node(node_type);
    else if (node_type == ANGLE_INIT)
        start_the_shake(shake_type);
    else {
        if (shake_type % 3 == 0 && node_type % 3 != 0)
        {
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (node_type % 3 == 0 && shake_type % 3 != 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (node_type % 3 == 0 && shake_type % 3 == 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -2));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -1));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
           vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (node_type % 3 != 0 && shake_type % 3 != 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }

        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    }
    
    ESP_LOGI(TAG, "%s set duty shake(%d %d) node(%d %d) ", __func__, shake_type, SEEK_ANGLE_DUTY(shake_type), node_type, SEEK_ANGLE_DUTY(node_type) );
}


void stop_shake_and_node()
{   
    ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
    ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
}



#elif SUPPORT_90_SERVO_MOTOR
//Also add extra two ear motor


/* ==================== 硬件配置 ==================== */
// 点头马达
#define NOD_MOTOR_IN1               GPIO_NUM_46
#define NOD_LEDC_TIMER              LEDC_TIMER_2
#define NOD_LEDC_CH_IN1             LEDC_CHANNEL_4


// 摇头马达
#define SHAKE_MOTOR_IN1             GPIO_NUM_21
#define SHAKE_LEDC_TIMER            LEDC_TIMER_3
#define SHAKE_LEDC_CH_IN1           LEDC_CHANNEL_6


// 左耳马达
#define EAR_MOTOR_LEFT              GPIO_NUM_3
#define EAR_LEFT_LEDC_TIMER         LEDC_TIMER_0
#define EAR_LEFT_LEDC_CH_IN1        LEDC_CHANNEL_5


// 右耳马达
#define EAR_MOTOR_RIGHT             GPIO_NUM_14
#define EAR_RIGHT_LEDC_TIMER        LEDC_TIMER_1
#define EAR_RIGHT_LEDC_CH_IN1       LEDC_CHANNEL_7





// PWM配置
#define MOTOR_LEDC_MODE     LEDC_LOW_SPEED_MODE

#define MOTOR_LEDC_DUTY_RES LEDC_TIMER_13_BIT    
#define MOTOR_RESOLUTION  8192
#define MOTOR_PWM_FREQ     50//               

// central postion is 615
#define DELTA_DUTY      8
#define POSTION_DUTY  (615 - DELTA_DUTY)

#define SERVO_MOTOR_NORMAL_ACTION_DELAY     500
#define SERVO_MOTOR_INIT_ACTION_DELAY       100

/* ==================== 全局变量 ==================== */
static bool g_servo_motor_initialized = false;

/* ==================== 驱动层基本控制 ==================== */

/**
 * @brief 初始化马达PWM
 */
esp_err_t hw_servo_motor_init(void)
{
    if (g_servo_motor_initialized) {
        ESP_LOGW(TAG, "Motor already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing motor driver...");

    // 配置点头马达定时器
    ledc_timer_config_t nod_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = NOD_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&nod_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure nod motor timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置摇头马达定时器
    ledc_timer_config_t shake_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = SHAKE_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&shake_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure shake motor timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置左耳马达定时器
    ledc_timer_config_t ear_left_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = EAR_LEFT_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ear_left_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ear left motor timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置右耳马达定时器
    ledc_timer_config_t ear_right_timer = {
        .speed_mode       = MOTOR_LEDC_MODE,
        .timer_num        = EAR_RIGHT_LEDC_TIMER,
        .duty_resolution  = MOTOR_LEDC_DUTY_RES,
        .freq_hz          = MOTOR_PWM_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ear_right_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ear right motor timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置点头马达IN1通道
    ledc_channel_config_t nod_ch_in1 = {
        .gpio_num   = NOD_MOTOR_IN1,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = NOD_LEDC_CH_IN1,
        .timer_sel  = NOD_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&nod_ch_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure nod motor IN1: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置摇头马达IN1通道
    ledc_channel_config_t shake_ch_in1 = {
        .gpio_num   = SHAKE_MOTOR_IN1,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = SHAKE_LEDC_CH_IN1,
        .timer_sel  = SHAKE_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&shake_ch_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure shake motor IN1: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置左耳马达IN1通道
    ledc_channel_config_t ear_left_ch_in1 = {
        .gpio_num   = EAR_MOTOR_LEFT,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = EAR_LEFT_LEDC_CH_IN1,
        .timer_sel  = EAR_LEFT_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&ear_left_ch_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ear left motor IN1: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置右耳马达IN1通道
    ledc_channel_config_t ear_right_ch_in1 = {
        .gpio_num   = EAR_MOTOR_RIGHT,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = EAR_RIGHT_LEDC_CH_IN1,
        .timer_sel  = EAR_RIGHT_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&ear_right_ch_in1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ear right motor IN1: %s", esp_err_to_name(ret));
        return ret;
    }



    g_servo_motor_initialized = true;
    ESP_LOGI(TAG, "servo Motor driver initialized successfully");
    ESP_LOGI(TAG, "  Nod motor: IN1=GPIO%d, ", NOD_MOTOR_IN1);
    ESP_LOGI(TAG, "  Shake motor: IN1=GPIO%d, ", SHAKE_MOTOR_IN1);
    ESP_LOGI(TAG, "  ear left motor: IN1=GPIO%d, ", EAR_MOTOR_LEFT);
    ESP_LOGI(TAG, "  ear right motor: IN1=GPIO%d, ", EAR_MOTOR_RIGHT);

    return ESP_OK;
}

bool hw_servo_motor_is_initialized(void)
{
    return g_servo_motor_initialized;
}


typedef struct
{
    uint8_t angle;
    uint32_t duty;
    
}motor_angle_t;

//1° 11.11
motor_angle_t motor_angle[] = 
{
    {ANGLE_INIT,        POSTION_DUTY},   
    {ANGLE_2_LEFT,     POSTION_DUTY - 9},   
    {ANGLE_2_RIGHT,    POSTION_DUTY + 9},
    {ANGLE_5_LEFT,     POSTION_DUTY - 23},   
    {ANGLE_5_RIGHT,    POSTION_DUTY + 23},
    {ANGLE_10_LEFT,     POSTION_DUTY - 46},   
    {ANGLE_10_RIGHT,    POSTION_DUTY + 46},
    {ANGLE_15_LEFT,     POSTION_DUTY - 68},   
    {ANGLE_15_RIGHT,    POSTION_DUTY + 68},
    {ANGLE_20_LEFT,     POSTION_DUTY - 91},   
    {ANGLE_20_RIGHT,    POSTION_DUTY + 91},
    {ANGLE_25_LEFT,     POSTION_DUTY - 114},   
    {ANGLE_25_RIGHT,    POSTION_DUTY + 114},

};

#define SEEK_ANGLE_DUTY(a) \
    ({ \
        uint16_t result = 0; \
        for (uint8_t i = 0; i < sizeof(motor_angle)/sizeof(motor_angle[0]); i++) { \
            if (motor_angle[i].angle == (a)) { \
                result = motor_angle[i].duty; \
                break; \
            } \
        } \
        result; \
    })


void start_the_shake(uint8_t type)
{
    if (type == 0)
    return;
    if (type % 3 == 0 ) {
        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -2 ));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -1 ));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    else {
        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s set duty %d ", __func__, SEEK_ANGLE_DUTY(type));
}

void stop_the_shake(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s", __func__);
}

void start_the_node(uint8_t type)
{
    if (type == 0)
    return;
    if (type % 3 == 0 ) {
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -2 ));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -1 ));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    else {
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s set duty %d ", __func__, SEEK_ANGLE_DUTY(type));

}

void stop_the_node(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s", __func__);
}

void start_shake_and_node(uint8_t shake_type, uint8_t node_type)
{
    if (shake_type == ANGLE_INIT)
        start_the_node(node_type);
    else if (node_type == ANGLE_INIT)
        start_the_shake(shake_type);
    else {
        if (shake_type % 3 == 0 && node_type % 3 != 0)
        {
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (node_type % 3 == 0 && shake_type % 3 != 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (node_type % 3 == 0 && shake_type % 3 == 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -2));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type -1));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
           vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (node_type % 3 != 0 && shake_type % 3 != 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(node_type ));
            ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(shake_type));
            ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }

        ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
        ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
        ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
        ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    }
    
    ESP_LOGI(TAG, "%s set duty shake(%d %d) node(%d %d) ", __func__, shake_type, SEEK_ANGLE_DUTY(shake_type), node_type, SEEK_ANGLE_DUTY(node_type) );
}


void stop_shake_and_node()
{   
    ledc_set_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, NOD_LEDC_CH_IN1);
    ledc_set_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, SHAKE_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
}


//the ear motor control
void start_the_ear_left(uint8_t type)
{
    if (type == 0)
    return;
    if (type % 3 == 0 ) {
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -2 ));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -1 ));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    else {
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s set duty %d ", __func__, SEEK_ANGLE_DUTY(type));
}

void stop_the_ear_left(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s", __func__);
}

void start_the_ear_right(uint8_t type)
{
    if (type == 0)
    return;
    if (type % 3 == 0 ) {
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -2 ));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type -1 ));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    else {
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(type));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
    }
    ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s set duty %d ", __func__, SEEK_ANGLE_DUTY(type));

}

void stop_the_ear_right(void)
{
    ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    ESP_LOGI(TAG, "%s", __func__);
}

void start_ear_left_and_right(uint8_t left_type, uint8_t right_type)
{
    if (left_type == ANGLE_INIT)
        start_the_ear_right(right_type);
    else if (right_type == ANGLE_INIT)
        start_the_ear_left(left_type);
    else {
        if (left_type % 3 == 0 && right_type % 3 != 0)
        {
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(left_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(right_type));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(left_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (right_type % 3 == 0 && left_type % 3 != 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(right_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(left_type));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(right_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (right_type % 3 == 0 && left_type % 3 == 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(right_type -2 ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(left_type -2));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(right_type -1 ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(left_type -1));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
           vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }
        else if (right_type % 3 != 0 && left_type % 3 != 0) {
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(right_type ));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
            ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(left_type));
            ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
            vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_NORMAL_ACTION_DELAY));
        }

        ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
        ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
        ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
        vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
    }
    
    ESP_LOGI(TAG, "%s set duty shake(%d %d) node(%d %d) ", __func__, left_type, SEEK_ANGLE_DUTY(left_type), right_type, SEEK_ANGLE_DUTY(right_type) );
}


void stop_ear_left_and_right()
{   
    ledc_set_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, EAR_RIGHT_LEDC_CH_IN1);
    ledc_set_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1, SEEK_ANGLE_DUTY(ANGLE_INIT));
    ledc_update_duty(MOTOR_LEDC_MODE, EAR_LEFT_LEDC_CH_IN1);
    vTaskDelay(pdMS_TO_TICKS(SERVO_MOTOR_INIT_ACTION_DELAY));
}

#endif /*SUPPORT_270_SERVO_MOTOR SUPPORT_90_SERVO_MOTOR*/