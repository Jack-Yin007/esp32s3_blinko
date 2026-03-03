/**
 * @file aw9535.c
 * @brief AW9535 GPIO扩展器驱动实现
 * 
 * AW9535是一款16位I2C GPIO扩展器，提供16个可配置的GPIO引脚
 * 本驱动实现了完整的GPIO控制、中断处理和I2C通信功能
 * 
 * 硬件连接：
 * - SDA: ESP32-S3 GPIO2
 * - SCL: ESP32-S3 GPIO1
 * - INT: ESP32-S3 GPIO45 (可选)
 */

#include "drivers/aw9535.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "AW9535";

/* ==================== 私有数据结构 ==================== */

/**
 * @brief GPIO中断配置
 */
typedef struct {
    bool enabled;                   // 中断是否启用
    aw9535_int_type_t type;        // 中断类型
    aw9535_isr_t handler;          // 中断回调函数
    void *user_data;               // 用户数据
    aw9535_level_t last_level;     // 上次读取的电平
} aw9535_pin_int_t;

/**
 * @brief AW9535驱动上下文
 */
typedef struct {
    bool initialized;               // 初始化标志
    aw9535_config_t config;        // 配置参数
    SemaphoreHandle_t mutex;       // 互斥锁
    aw9535_pin_int_t pin_ints[AW9535_PIN_MAX];  // 中断配置
    TaskHandle_t int_task_handle;  // 中断处理任务句柄
    uint8_t output_cache_p0;       // 端口0输出缓存
    uint8_t output_cache_p1;       // 端口1输出缓存
} aw9535_context_t;

/* 全局上下文 */
static aw9535_context_t g_aw9535_ctx = {
    .initialized = false,
    .mutex = NULL,
    .int_task_handle = NULL,
};

/* ==================== 私有函数声明 ==================== */

static esp_err_t aw9535_i2c_init(const aw9535_i2c_config_t *i2c_cfg);
static esp_err_t aw9535_i2c_deinit(void);
static esp_err_t aw9535_write_reg(uint8_t reg_addr, uint8_t data);
static esp_err_t aw9535_read_reg(uint8_t reg_addr, uint8_t *data);
static void aw9535_int_task(void *arg);
static void aw9535_isr_handler(void *arg);

/* ==================== I2C通信函数 ==================== */

/**
 * @brief 初始化I2C总线
 */
static esp_err_t aw9535_i2c_init(const aw9535_i2c_config_t *i2c_cfg)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = i2c_cfg->sda_io,
        .scl_io_num = i2c_cfg->scl_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_cfg->freq_hz,
    };

    esp_err_t ret = i2c_param_config(i2c_cfg->i2c_port, &conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C参数配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(i2c_cfg->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C驱动安装失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C初始化成功 (Port:%d, SDA:%d, SCL:%d, Freq:%lu Hz)", 
             i2c_cfg->i2c_port, i2c_cfg->sda_io, i2c_cfg->scl_io, i2c_cfg->freq_hz);
    
    return ESP_OK;
}

/**
 * @brief 反初始化I2C总线
 */
static esp_err_t aw9535_i2c_deinit(void)
{
    return i2c_driver_delete(g_aw9535_ctx.config.i2c_cfg.i2c_port);
}

/**
 * @brief 写AW9535寄存器
 */
static esp_err_t aw9535_write_reg(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_aw9535_ctx.config.i2c_cfg.dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(g_aw9535_ctx.config.i2c_cfg.i2c_port, cmd, 
                                         pdMS_TO_TICKS(AW9535_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief 读AW9535寄存器
 */
static esp_err_t aw9535_read_reg(uint8_t reg_addr, uint8_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // 写寄存器地址
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_aw9535_ctx.config.i2c_cfg.dev_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    
    // 读数据
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (g_aw9535_ctx.config.i2c_cfg.dev_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(g_aw9535_ctx.config.i2c_cfg.i2c_port, cmd, 
                                         pdMS_TO_TICKS(AW9535_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读寄存器0x%02X失败: %s", reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/* ==================== 中断处理 ==================== */

/**
 * @brief ESP32 GPIO中断服务程序
 */
static void IRAM_ATTR aw9535_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (g_aw9535_ctx.int_task_handle != NULL) {
        vTaskNotifyGiveFromISR(g_aw9535_ctx.int_task_handle, &xHigherPriorityTaskWoken);
    }
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief 中断处理任务
 */
static void aw9535_int_task(void *arg)
{
    uint8_t int_status0 = 0, int_status1 = 0;
    uint8_t input_p0 = 0, input_p1 = 0;
    uint8_t last_input_p0 = 0xFF, last_input_p1 = 0xFF;  // 记录上次INPUT值
    uint32_t task_wakeup_count = 0;
    
    ESP_LOGI(TAG, "中断处理任务已启动");
    
    // 读取初始INPUT状态
    aw9535_read_reg(AW9535_REG_INPUT_P0, &last_input_p0);
    aw9535_read_reg(AW9535_REG_INPUT_P1, &last_input_p1);
    
    while (1) {
        // 等待中断通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 读取INT状态寄存器（用于清除中断）
        aw9535_read_reg(AW9535_REG_INT_P0, &int_status0);
        aw9535_read_reg(AW9535_REG_INT_P1, &int_status1);
        
        // 读取当前输入电平
        if (aw9535_read_reg(AW9535_REG_INPUT_P0, &input_p0) != ESP_OK ||
            aw9535_read_reg(AW9535_REG_INPUT_P1, &input_p1) != ESP_OK) {
            ESP_LOGE(TAG, "读取输入电平失败");
            continue;
        }
        
        // 检测P0的变化
        uint8_t changed_p0 = input_p0 ^ last_input_p0;
        
        // 检测P1的变化
        uint8_t changed_p1 = input_p1 ^ last_input_p1;
        
        // 处理端口0中断（基于INPUT变化）
        for (int i = 0; i < 8; i++) {
            if (changed_p0 & (1 << i)) {  // ← 修改：基于变化检测，而非INT寄存器
                aw9535_pin_t pin = (aw9535_pin_t)i;
                aw9535_pin_int_t *pin_int = &g_aw9535_ctx.pin_ints[pin];
                
                if (pin_int->enabled && pin_int->handler != NULL) {
                    aw9535_level_t current_level = (input_p0 & (1 << i)) ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW;
                    
                    // 检查中断类型
                    bool trigger = false;
                    switch (pin_int->type) {
                        case AW9535_INT_RISING:
                            trigger = (pin_int->last_level == AW9535_LEVEL_LOW && current_level == AW9535_LEVEL_HIGH);
                            break;
                        case AW9535_INT_FALLING:
                            trigger = (pin_int->last_level == AW9535_LEVEL_HIGH && current_level == AW9535_LEVEL_LOW);
                            break;
                        case AW9535_INT_BOTH_EDGE:
                            trigger = (pin_int->last_level != current_level);
                            break;
                        default:
                            break;
                    }
                    
                    if (trigger) {
                        pin_int->handler(pin, current_level, pin_int->user_data);
                    }
                    
                    pin_int->last_level = current_level;
                }
            }
        }
        
        // 处理端口1中断（基于INPUT变化）
        for (int i = 0; i < 8; i++) {
            if (changed_p1 & (1 << i)) {  // ← 修改：基于变化检测
                aw9535_pin_t pin = (aw9535_pin_t)(i + 8);
                aw9535_pin_int_t *pin_int = &g_aw9535_ctx.pin_ints[pin];
                
                if (pin_int->enabled && pin_int->handler != NULL) {
                    aw9535_level_t current_level = (input_p1 & (1 << i)) ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW;
                    
                    // 检查中断类型
                    bool trigger = false;
                    switch (pin_int->type) {
                        case AW9535_INT_RISING:
                            trigger = (pin_int->last_level == AW9535_LEVEL_LOW && current_level == AW9535_LEVEL_HIGH);
                            break;
                        case AW9535_INT_FALLING:
                            trigger = (pin_int->last_level == AW9535_LEVEL_HIGH && current_level == AW9535_LEVEL_LOW);
                            break;
                        case AW9535_INT_BOTH_EDGE:
                            trigger = (pin_int->last_level != current_level);
                            break;
                        default:
                            break;
                    }
                    
                    if (trigger) {
                        pin_int->handler(pin, current_level, pin_int->user_data);
                    }
                    
                    pin_int->last_level = current_level;
                }
            }
        }
        
        // 更新last_input以便下次比较
        last_input_p0 = input_p0;
        last_input_p1 = input_p1;
        
        // 清除中断（通过读取输入寄存器自动清除）
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 初始化AW9535驱动
 */
esp_err_t aw9535_init(const aw9535_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_aw9535_ctx.initialized) {
        ESP_LOGW(TAG, "AW9535已经初始化");
        return ESP_OK;
    }
    
    // 创建互斥锁
    g_aw9535_ctx.mutex = xSemaphoreCreateMutex();
    if (g_aw9535_ctx.mutex == NULL) {
        ESP_LOGE(TAG, "创建互斥锁失败");
        return ESP_ERR_NO_MEM;
    }
    
    // 保存配置
    memcpy(&g_aw9535_ctx.config, config, sizeof(aw9535_config_t));
    
    // 初始化I2C
    esp_err_t ret = aw9535_i2c_init(&config->i2c_cfg);
    if (ret != ESP_OK) {
        vSemaphoreDelete(g_aw9535_ctx.mutex);
        g_aw9535_ctx.mutex = NULL;
        return ret;
    }
    
    // 初始化中断配置
    memset(g_aw9535_ctx.pin_ints, 0, sizeof(g_aw9535_ctx.pin_ints));
    
    // 读取当前输出状态
    aw9535_read_reg(AW9535_REG_OUTPUT_P0, &g_aw9535_ctx.output_cache_p0);
    aw9535_read_reg(AW9535_REG_OUTPUT_P1, &g_aw9535_ctx.output_cache_p1);
    
    // 清除任何挂起的中断状态（读取中断状态寄存器会清除它们）
    uint8_t dummy;
    aw9535_read_reg(AW9535_REG_INT_P0, &dummy);
    aw9535_read_reg(AW9535_REG_INT_P1, &dummy);
    ESP_LOGI(TAG, "已清除初始中断状态");
    
    // 如果配置了中断引脚，初始化中断处理
    if (config->int_io >= 0) {
        // 配置中断引脚
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_NEGEDGE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << config->int_io),
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        
        ret = gpio_config(&io_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "配置中断引脚失败");
            aw9535_i2c_deinit();
            vSemaphoreDelete(g_aw9535_ctx.mutex);
            g_aw9535_ctx.mutex = NULL;
            return ret;
        }
        
        // 创建中断处理任务
        BaseType_t task_ret = xTaskCreate(aw9535_int_task, "aw9535_int", 
                                          4096, NULL, 10, &g_aw9535_ctx.int_task_handle);
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "创建中断处理任务失败");
            aw9535_i2c_deinit();
            vSemaphoreDelete(g_aw9535_ctx.mutex);
            g_aw9535_ctx.mutex = NULL;
            return ESP_ERR_NO_MEM;
        }
        
        // 安装GPIO中断服务
        ret = gpio_install_isr_service(0);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "安装GPIO中断服务失败");
            vTaskDelete(g_aw9535_ctx.int_task_handle);
            g_aw9535_ctx.int_task_handle = NULL;
            aw9535_i2c_deinit();
            vSemaphoreDelete(g_aw9535_ctx.mutex);
            g_aw9535_ctx.mutex = NULL;
            return ret;
        }
        
        // 添加中断处理函数
        ret = gpio_isr_handler_add(config->int_io, aw9535_isr_handler, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "添加中断处理函数失败");
            vTaskDelete(g_aw9535_ctx.int_task_handle);
            g_aw9535_ctx.int_task_handle = NULL;
            aw9535_i2c_deinit();
            vSemaphoreDelete(g_aw9535_ctx.mutex);
            g_aw9535_ctx.mutex = NULL;
            return ret;
        }
        
        ESP_LOGI(TAG, "中断功能已启用 (INT GPIO:%d)", config->int_io);
    }
    
    g_aw9535_ctx.initialized = true;
    ESP_LOGI(TAG, "AW9535初始化成功 (I2C地址:0x%02X)", config->i2c_cfg.dev_addr);
    
    return ESP_OK;
}

/**
 * @brief 反初始化AW9535驱动
 */
esp_err_t aw9535_deinit(void)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_OK;
    }
    
    // 停止中断处理
    if (g_aw9535_ctx.int_task_handle != NULL) {
        vTaskDelete(g_aw9535_ctx.int_task_handle);
        g_aw9535_ctx.int_task_handle = NULL;
    }
    
    if (g_aw9535_ctx.config.int_io >= 0) {
        gpio_isr_handler_remove(g_aw9535_ctx.config.int_io);
    }
    
    // 反初始化I2C
    aw9535_i2c_deinit();
    
    // 删除互斥锁
    if (g_aw9535_ctx.mutex != NULL) {
        vSemaphoreDelete(g_aw9535_ctx.mutex);
        g_aw9535_ctx.mutex = NULL;
    }
    
    g_aw9535_ctx.initialized = false;
    ESP_LOGI(TAG, "AW9535已反初始化");
    
    return ESP_OK;
}

/**
 * @brief 复位AW9535设备
 */
esp_err_t aw9535_reset(void)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    // 设置所有引脚为输入模式
    esp_err_t ret = aw9535_write_reg(AW9535_REG_CONFIG_P0, 0xFF);
    ret |= aw9535_write_reg(AW9535_REG_CONFIG_P1, 0xFF);
    
    // 清除输出寄存器
    ret |= aw9535_write_reg(AW9535_REG_OUTPUT_P0, 0x00);
    ret |= aw9535_write_reg(AW9535_REG_OUTPUT_P1, 0x00);
    
    g_aw9535_ctx.output_cache_p0 = 0x00;
    g_aw9535_ctx.output_cache_p1 = 0x00;
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "AW9535复位成功");
    }
    
    return ret;
}

/**
 * @brief 设置GPIO引脚模式
 */
esp_err_t aw9535_set_mode(aw9535_pin_t pin, aw9535_mode_t mode)
{
    if (pin >= AW9535_PIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t reg_addr = (pin < 8) ? AW9535_REG_CONFIG_P0 : AW9535_REG_CONFIG_P1;
    uint8_t bit_pos = pin % 8;
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    uint8_t reg_val = 0;
    esp_err_t ret = aw9535_read_reg(reg_addr, &reg_val);
    
    if (ret == ESP_OK) {
        if (mode == AW9535_MODE_INPUT) {
            reg_val |= (1 << bit_pos);   // 设置为输入
        } else {
            reg_val &= ~(1 << bit_pos);  // 设置为输出
        }
        ret = aw9535_write_reg(reg_addr, reg_val);
    }
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 批量设置端口0模式
 */
esp_err_t aw9535_set_mode_port0(uint8_t mode_mask)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    esp_err_t ret = aw9535_write_reg(AW9535_REG_CONFIG_P0, mode_mask);
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 批量设置端口1模式
 */
esp_err_t aw9535_set_mode_port1(uint8_t mode_mask)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    esp_err_t ret = aw9535_write_reg(AW9535_REG_CONFIG_P1, mode_mask);
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 设置GPIO输出电平
 */
esp_err_t aw9535_set_level(aw9535_pin_t pin, aw9535_level_t level)
{
    if (pin >= AW9535_PIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t bit_pos = pin % 8;
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    esp_err_t ret;
    if (pin < 8) {
        // 端口0
        if (level == AW9535_LEVEL_HIGH) {
            g_aw9535_ctx.output_cache_p0 |= (1 << bit_pos);
        } else {
            g_aw9535_ctx.output_cache_p0 &= ~(1 << bit_pos);
        }
        ret = aw9535_write_reg(AW9535_REG_OUTPUT_P0, g_aw9535_ctx.output_cache_p0);
    } else {
        // 端口1
        if (level == AW9535_LEVEL_HIGH) {
            g_aw9535_ctx.output_cache_p1 |= (1 << bit_pos);
        } else {
            g_aw9535_ctx.output_cache_p1 &= ~(1 << bit_pos);
        }
        ret = aw9535_write_reg(AW9535_REG_OUTPUT_P1, g_aw9535_ctx.output_cache_p1);
    }
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 批量设置端口0输出电平
 */
esp_err_t aw9535_set_level_port0(uint8_t level_mask)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    g_aw9535_ctx.output_cache_p0 = level_mask;
    esp_err_t ret = aw9535_write_reg(AW9535_REG_OUTPUT_P0, level_mask);
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 批量设置端口1输出电平
 */
esp_err_t aw9535_set_level_port1(uint8_t level_mask)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    g_aw9535_ctx.output_cache_p1 = level_mask;
    esp_err_t ret = aw9535_write_reg(AW9535_REG_OUTPUT_P1, level_mask);
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 读取GPIO输入电平
 */
esp_err_t aw9535_get_level(aw9535_pin_t pin, aw9535_level_t *level)
{
    if (pin >= AW9535_PIN_MAX || level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t reg_addr = (pin < 8) ? AW9535_REG_INPUT_P0 : AW9535_REG_INPUT_P1;
    uint8_t bit_pos = pin % 8;
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    uint8_t reg_val = 0;
    esp_err_t ret = aw9535_read_reg(reg_addr, &reg_val);
    
    if (ret == ESP_OK) {
        *level = (reg_val & (1 << bit_pos)) ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW;
    }
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 批量读取端口0输入电平
 */
esp_err_t aw9535_get_level_port0(uint8_t *level_mask)
{
    if (level_mask == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    esp_err_t ret = aw9535_read_reg(AW9535_REG_INPUT_P0, level_mask);
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 批量读取端口1输入电平
 */
esp_err_t aw9535_get_level_port1(uint8_t *level_mask)
{
    if (level_mask == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    esp_err_t ret = aw9535_read_reg(AW9535_REG_INPUT_P1, level_mask);
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 配置GPIO中断
 */
esp_err_t aw9535_set_interrupt(aw9535_pin_t pin, aw9535_int_type_t int_type,
                                aw9535_isr_t isr_handler, void *user_data)
{
    if (pin >= AW9535_PIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_aw9535_ctx.config.int_io < 0) {
        ESP_LOGE(TAG, "未配置中断引脚");
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    aw9535_pin_int_t *pin_int = &g_aw9535_ctx.pin_ints[pin];
    pin_int->type = int_type;
    pin_int->handler = isr_handler;
    pin_int->user_data = user_data;
    pin_int->enabled = (int_type != AW9535_INT_DISABLE);
    
    // 读取当前电平作为初始值（直接读寄存器，避免死锁）
    uint8_t reg_addr = (pin < 8) ? AW9535_REG_INPUT_P0 : AW9535_REG_INPUT_P1;
    uint8_t bit_pos = pin % 8;
    uint8_t reg_val = 0;
    esp_err_t ret = aw9535_read_reg(reg_addr, &reg_val);
    
    if (ret == ESP_OK) {
        aw9535_level_t current_level = (reg_val & (1 << bit_pos)) ? AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW;
        pin_int->last_level = current_level;
        
        // ⚠️ 关键修复：AW9535通过OUTPUT寄存器设置中断触发条件
        // 中断触发条件：INPUT寄存器 != OUTPUT寄存器
        // 为了检测上升沿（LOW→HIGH），OUTPUT寄存器应设为LOW
        // 为了检测下降沿（HIGH→LOW），OUTPUT寄存器应设为HIGH
        uint8_t output_reg_addr = (pin < 8) ? AW9535_REG_OUTPUT_P0 : AW9535_REG_OUTPUT_P1;
        uint8_t output_val = 0;
        aw9535_read_reg(output_reg_addr, &output_val);
        
        // 根据中断类型配置OUTPUT寄存器的期望值
        if (int_type == AW9535_INT_RISING) {
            // 上升沿：期望INPUT从LOW变HIGH，所以OUTPUT设为LOW
            output_val &= ~(1 << bit_pos);  // 清除对应位（设为0=LOW）
        } else if (int_type == AW9535_INT_FALLING) {
            // 下降沿：期望INPUT从HIGH变LOW，所以OUTPUT设为HIGH
            output_val |= (1 << bit_pos);   // 设置对应位（设为1=HIGH）
        } else if (int_type == AW9535_INT_BOTH_EDGE) {
            // 双边沿：设置为与当前输入相反的值
            if (current_level == AW9535_LEVEL_HIGH) {
                output_val &= ~(1 << bit_pos);  // INPUT=HIGH，OUTPUT=LOW
            } else {
                output_val |= (1 << bit_pos);   // INPUT=LOW，OUTPUT=HIGH
            }
        }
        
        // 写入OUTPUT寄存器
        ret = aw9535_write_reg(output_reg_addr, output_val);
        if (ret == ESP_OK) {
            // 更新缓存
            if (pin < 8) {
                g_aw9535_ctx.output_cache_p0 = output_val;
            } else {
                g_aw9535_ctx.output_cache_p1 = output_val;
            }
            ESP_LOGI(TAG, "配置引脚%d中断: 类型=%d, 初始电平=%d, OUTPUT寄存器=0x%02X", 
                     pin, int_type, current_level, output_val);
        } else {
            ESP_LOGE(TAG, "配置引脚%d中断: 写入OUTPUT寄存器失败", pin);
        }
    } else {
        ESP_LOGW(TAG, "配置引脚%d中断: 类型=%d, 读取初始电平失败", pin, int_type);
    }
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 启用GPIO中断
 */
esp_err_t aw9535_enable_interrupt(aw9535_pin_t pin)
{
    if (pin >= AW9535_PIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    g_aw9535_ctx.pin_ints[pin].enabled = true;
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ESP_OK;
}

/**
 * @brief 禁用GPIO中断
 */
esp_err_t aw9535_disable_interrupt(aw9535_pin_t pin)
{
    if (pin >= AW9535_PIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    g_aw9535_ctx.pin_ints[pin].enabled = false;
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ESP_OK;
}

/**
 * @brief 读取中断状态
 */
esp_err_t aw9535_get_interrupt_status(uint8_t *int_status0, uint8_t *int_status1)
{
    if (int_status0 == NULL || int_status1 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    esp_err_t ret = aw9535_read_reg(AW9535_REG_INT_P0, int_status0);
    ret |= aw9535_read_reg(AW9535_REG_INT_P1, int_status1);
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 清除中断状态
 */
esp_err_t aw9535_clear_interrupt(void)
{
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // AW9535通过读取输入寄存器自动清除中断
    uint8_t dummy;
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    esp_err_t ret = aw9535_read_reg(AW9535_REG_INPUT_P0, &dummy);
    ret |= aw9535_read_reg(AW9535_REG_INPUT_P1, &dummy);
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 翻转GPIO输出电平
 */
esp_err_t aw9535_toggle_level(aw9535_pin_t pin)
{
    if (pin >= AW9535_PIN_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_aw9535_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t bit_pos = pin % 8;
    
    xSemaphoreTake(g_aw9535_ctx.mutex, portMAX_DELAY);
    
    esp_err_t ret;
    if (pin < 8) {
        // 端口0
        g_aw9535_ctx.output_cache_p0 ^= (1 << bit_pos);
        ret = aw9535_write_reg(AW9535_REG_OUTPUT_P0, g_aw9535_ctx.output_cache_p0);
    } else {
        // 端口1
        g_aw9535_ctx.output_cache_p1 ^= (1 << bit_pos);
        ret = aw9535_write_reg(AW9535_REG_OUTPUT_P1, g_aw9535_ctx.output_cache_p1);
    }
    
    xSemaphoreGive(g_aw9535_ctx.mutex);
    
    return ret;
}

/**
 * @brief 诊断函数：打印AW9535和中断配置状态
 */
void aw9535_debug_print_status(void)
{
    if (!g_aw9535_ctx.initialized) {
        ESP_LOGW(TAG, "AW9535 not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "AW9535 Diagnostic Information");
    ESP_LOGI(TAG, "========================================");
    
    // 读取所有寄存器
    uint8_t input_p0, input_p1, output_p0, output_p1;
    uint8_t int_p0, int_p1, config_p0, config_p1;
    
    aw9535_read_reg(AW9535_REG_INPUT_P0, &input_p0);
    aw9535_read_reg(AW9535_REG_INPUT_P1, &input_p1);
    aw9535_read_reg(AW9535_REG_OUTPUT_P0, &output_p0);
    aw9535_read_reg(AW9535_REG_OUTPUT_P1, &output_p1);
    aw9535_read_reg(AW9535_REG_INT_P0, &int_p0);
    aw9535_read_reg(AW9535_REG_INT_P1, &int_p1);
    aw9535_read_reg(AW9535_REG_CONFIG_P0, &config_p0);
    aw9535_read_reg(AW9535_REG_CONFIG_P1, &config_p1);
    
    ESP_LOGI(TAG, "Registers:");
    ESP_LOGI(TAG, "  INPUT:  P0=0x%02X, P1=0x%02X", input_p0, input_p1);
    ESP_LOGI(TAG, "  OUTPUT: P0=0x%02X, P1=0x%02X", output_p0, output_p1);
    ESP_LOGI(TAG, "  INT:    P0=0x%02X, P1=0x%02X", int_p0, int_p1);
    ESP_LOGI(TAG, "  CONFIG: P0=0x%02X, P1=0x%02X (1=INPUT, 0=OUTPUT)", config_p0, config_p1);
    
    // P1.7状态
    ESP_LOGI(TAG, "P1.7 (pin 15) Status:");
    ESP_LOGI(TAG, "  Mode: %s", (config_p1 & 0x80) ? "INPUT" : "OUTPUT");
    ESP_LOGI(TAG, "  Level: %s", (input_p1 & 0x80) ? "HIGH" : "LOW");
    ESP_LOGI(TAG, "  INT status: %s", (int_p1 & 0x80) ? "TRIGGERED" : "Clear");
    ESP_LOGI(TAG, "  Callback registered: %s", 
             g_aw9535_ctx.pin_ints[15].handler ? "YES" : "NO");
    ESP_LOGI(TAG, "  Interrupt enabled: %s", 
             g_aw9535_ctx.pin_ints[15].enabled ? "YES" : "NO");
    ESP_LOGI(TAG, "  Last level: %d", g_aw9535_ctx.pin_ints[15].last_level);
    
    // ESP32 GPIO45状态
    if (g_aw9535_ctx.config.int_io >= 0) {
        int gpio_level = gpio_get_level(g_aw9535_ctx.config.int_io);
        ESP_LOGI(TAG, "ESP32 GPIO%d (AW9535 INT pin):", g_aw9535_ctx.config.int_io);
        ESP_LOGI(TAG, "  Current level: %s", gpio_level ? "HIGH" : "LOW");
        ESP_LOGI(TAG, "  Expected: HIGH (idle), LOW (interrupt active)");
    }
    
    ESP_LOGI(TAG, "========================================");
}
