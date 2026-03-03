/**
 * @file motor.c
 * @brief 双马达驱动 - 点头马达 + 摇头马达
 * 
 * 硬件连接：
 * - 点头马达: IN1-GPIO46, IN2-GPIO3
 * - 摇头马达: IN1-GPIO21, IN2-GPIO14
 * 
 * 控制方式：双PWM控制
 * - IN1=HIGH, IN2=LOW  -> 正转
 * - IN1=LOW,  IN2=HIGH -> 反转
 * - IN1=LOW,  IN2=LOW  -> 停止/刹车
 * - IN1=HIGH, IN2=HIGH -> 不推荐（短路风险）
 * 
 * 点头马达：连续旋转，机械结构控制反复点头
 * 摇头马达：需要时间控制正反转实现摇头动作
 */

#include "drivers/hardware_interface.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Motor";

/* ==================== 硬件配置 ==================== */
// 点头马达
#define NOD_MOTOR_IN1       GPIO_NUM_46
#define NOD_MOTOR_IN2       GPIO_NUM_3
#define NOD_LEDC_TIMER      LEDC_TIMER_2
#define NOD_LEDC_CH_IN1     LEDC_CHANNEL_4
#define NOD_LEDC_CH_IN2     LEDC_CHANNEL_5

// 摇头马达
#define SHAKE_MOTOR_IN1     GPIO_NUM_21
#define SHAKE_MOTOR_IN2     GPIO_NUM_14
#define SHAKE_LEDC_TIMER    LEDC_TIMER_3
#define SHAKE_LEDC_CH_IN1   LEDC_CHANNEL_6
#define SHAKE_LEDC_CH_IN2   LEDC_CHANNEL_7

// PWM配置
#define MOTOR_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_DUTY_RES LEDC_TIMER_8_BIT    // 8位分辨率 (0-255)
#define MOTOR_PWM_FREQ      1000                 // 1kHz

/* ==================== 全局变量 ==================== */
static bool g_motor_initialized = false;

/* ==================== 驱动层基本控制 ==================== */

/**
 * @brief 初始化马达PWM
 */
esp_err_t hw_motor_init(void)
{
    if (g_motor_initialized) {
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

    // 配置点头马达IN2通道
    ledc_channel_config_t nod_ch_in2 = {
        .gpio_num   = NOD_MOTOR_IN2,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = NOD_LEDC_CH_IN2,
        .timer_sel  = NOD_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&nod_ch_in2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure nod motor IN2: %s", esp_err_to_name(ret));
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

    // 配置摇头马达IN2通道
    ledc_channel_config_t shake_ch_in2 = {
        .gpio_num   = SHAKE_MOTOR_IN2,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel    = SHAKE_LEDC_CH_IN2,
        .timer_sel  = SHAKE_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE
    };
    ret = ledc_channel_config(&shake_ch_in2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure shake motor IN2: %s", esp_err_to_name(ret));
        return ret;
    }

    g_motor_initialized = true;
    ESP_LOGI(TAG, "Motor driver initialized successfully");
    ESP_LOGI(TAG, "  Nod motor: IN1=GPIO%d, IN2=GPIO%d", NOD_MOTOR_IN1, NOD_MOTOR_IN2);
    ESP_LOGI(TAG, "  Shake motor: IN1=GPIO%d, IN2=GPIO%d", SHAKE_MOTOR_IN1, SHAKE_MOTOR_IN2);

    return ESP_OK;
}

/**
 * @brief 设置马达方向和速度（基本原语）
 * @param motor 马达ID
 * @param direction 方向
 * @param speed 速度 (0-100)
 */
esp_err_t hw_motor_set(motor_id_t motor, motor_direction_t direction, uint8_t speed)
{
    if (!g_motor_initialized) {
        ESP_LOGW(TAG, "Motor not initialized");
        return ESP_FAIL;
    }

    // 限制速度范围
    if (speed > 100) {
        speed = 100;
    }
    uint32_t duty = (speed * 255) / 100;

    ledc_channel_t ch_in1, ch_in2;
    const char *motor_name;
    
    // 选择通道
    if (motor == MOTOR_NOD) {
        ch_in1 = NOD_LEDC_CH_IN1;
        ch_in2 = NOD_LEDC_CH_IN2;
        motor_name = "Nod";
    } else if (motor == MOTOR_SHAKE) {
        ch_in1 = SHAKE_LEDC_CH_IN1;
        ch_in2 = SHAKE_LEDC_CH_IN2;
        motor_name = "Shake";
    } else {
        ESP_LOGE(TAG, "Invalid motor ID: %d", motor);
        return ESP_ERR_INVALID_ARG;
    }

    // 根据方向设置PWM
    switch (direction) {
        case MOTOR_DIR_FORWARD:  // 正转
            ESP_LOGD(TAG, "%s motor: FORWARD at %d%% (duty=%lu)", motor_name, speed, duty);
            ledc_set_duty(MOTOR_LEDC_MODE, ch_in1, duty);
            ledc_set_duty(MOTOR_LEDC_MODE, ch_in2, 0);
            break;

        case MOTOR_DIR_BACKWARD: // 反转
            ESP_LOGD(TAG, "%s motor: BACKWARD at %d%% (duty=%lu)", motor_name, speed, duty);
            ledc_set_duty(MOTOR_LEDC_MODE, ch_in1, 0);
            ledc_set_duty(MOTOR_LEDC_MODE, ch_in2, duty);
            break;

        case MOTOR_DIR_STOP:     // 停止
        case MOTOR_DIR_BRAKE:    // 刹车
            ESP_LOGD(TAG, "%s motor: STOP", motor_name);
            ledc_set_duty(MOTOR_LEDC_MODE, ch_in1, 0);
            ledc_set_duty(MOTOR_LEDC_MODE, ch_in2, 0);
            break;

        default:
            ESP_LOGE(TAG, "Invalid direction: %d", direction);
            return ESP_ERR_INVALID_ARG;
    }

    ledc_update_duty(MOTOR_LEDC_MODE, ch_in1);
    ledc_update_duty(MOTOR_LEDC_MODE, ch_in2);

    return ESP_OK;
}

/**
 * @brief 停止指定马达
 */
esp_err_t hw_motor_stop(motor_id_t motor)
{
    return hw_motor_set(motor, MOTOR_DIR_STOP, 0);
}

/**
 * @brief 停止所有马达
 */
esp_err_t hw_motor_stop_all(void)
{
    ESP_LOGI(TAG, "Stopping all motors");
    hw_motor_stop(MOTOR_NOD);
    hw_motor_stop(MOTOR_SHAKE);
    return ESP_OK;
}

/**
 * @brief 获取马达初始化状态
 */
bool hw_motor_is_initialized(void)
{
    return g_motor_initialized;
}
