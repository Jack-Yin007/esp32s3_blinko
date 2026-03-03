# AW9535 GPIO扩展器驱动文档

## 📋 目录
- [概述](#概述)
- [硬件连接](#硬件连接)
- [接线图](#接线图)
- [快速开始](#快速开始)
- [API参考](#api参考)
- [使用示例](#使用示例)
- [注意事项](#注意事项)
- [故障排查](#故障排查)

---

## 概述

AW9535是一款16位I2C接口的GPIO扩展器芯片，适用于GPIO引脚不足的场景。

### 主要特性
- ✅ **16个GPIO引脚**：分为端口0（P0.0-P0.7）和端口1（P1.0-P1.7）
- ✅ **I2C接口控制**：标准I2C通信，支持400kHz快速模式
- ✅ **灵活配置**：每个引脚可独立配置为输入或输出
- ✅ **中断支持**：支持上升沿、下降沿、双边沿中断
- ✅ **驱动能力**：输出驱动电流可达8mA
- ✅ **低功耗**：待机电流小于1µA

### 技术参数
| 参数 | 规格 |
|-----|------|
| 工作电压 | 1.65V ~ 5.5V |
| I2C地址 | 0x20 ~ 0x27（可配置），默认0x24 |
| I2C速率 | 标准模式100kHz，快速模式400kHz |
| GPIO数量 | 16个（2个8位端口） |
| 输出电流 | 每引脚最大8mA |
| 工作温度 | -40°C ~ +85°C |

---

## 硬件连接

### 引脚定义

#### AW9535芯片引脚
```
AW9535 引脚说明：
┌─────────────────────────┐
│  1  P0.0   GPIO端口0.0   │
│  2  P0.1   GPIO端口0.1   │
│  3  P0.2   GPIO端口0.2   │
│  4  P0.3   GPIO端口0.3   │
│  5  P0.4   GPIO端口0.4   │
│  6  P0.5   GPIO端口0.5   │
│  7  P0.6   GPIO端口0.6   │
│  8  P0.7   GPIO端口0.7   │
│  9  GND    地线          │
│ 10  P1.0   GPIO端口1.0   │
│ 11  P1.1   GPIO端口1.1   │
│ 12  P1.2   GPIO端口1.2   │
│ 13  P1.3   GPIO端口1.3   │
│ 14  P1.4   GPIO端口1.4   │
│ 15  P1.5   GPIO端口1.5   │
│ 16  P1.6   GPIO端口1.6   │
│ 17  P1.7   GPIO端口1.7   │
│ 18  INT    中断输出      │
│ 19  SCL    I2C时钟       │
│ 20  SDA    I2C数据       │
│ 21  ADDR   地址选择      │
│ 22  VDD    电源正极      │
│ 23  RST    复位（高电平）│
│ 24  GND    地线          │
└─────────────────────────┘
```

### ESP32-S3连接

| AW9535引脚 | ESP32-S3引脚 | 说明 |
|-----------|-------------|------|
| VDD | 3.3V | 电源（3.3V） |
| GND | GND | 地线 |
| SDA | GPIO2 | I2C数据线 |
| SCL | GPIO1 | I2C时钟线 |
| INT | GPIO45 | 中断信号（可选） |
| RST | 3.3V | 复位引脚（接高电平） |
| ADDR | GND | 地址选择（接地=0x24） |

---

## 接线图

### 基本连接（无中断）
```
ESP32-S3                    AW9535 GPIO扩展器
┌──────────┐               ┌──────────────┐
│          │               │              │
│  GPIO2   ├───────────────┤ SDA          │
│  (SDA)   │               │              │
│          │               │              │
│  GPIO1   ├───────────────┤ SCL          │
│  (SCL)   │               │              │
│          │               │              │
│  GPIO45  │               │              │
│          │               │              │
│   3.3V   ├───────────────┤ VDD          │
│          │               │              │
│   GND    ├───────────────┤ GND          │
│          │               │              │
└──────────┘               │ RST ──┐      │
                           │       │      │
                      3.3V ├───────┘      │
                           │              │
                           │ ADDR ──┐     │
                           │        │     │
                       GND ├────────┘     │
                           │              │
                           └──────────────┘
```

### 完整连接（含中断）
```
ESP32-S3                    AW9535 GPIO扩展器
┌──────────┐               ┌──────────────┐
│          │               │              │
│  GPIO2   ├───────────────┤ SDA          │
│  (SDA)   │               │              │
│          │               │              │
│  GPIO1   ├───────────────┤ SCL          │
│  (SCL)   │               │              │
│          │               │              │
│  GPIO45  ├───────────────┤ INT          │
│  (INT)   │               │              │
│          │               │              │
│   3.3V   ├───────────────┤ VDD          │
│          │               │              │
│   GND    ├───────────────┤ GND          │
│          │               │              │
└──────────┘               │ RST ──┐      │
                           │       │      │
                      3.3V ├───────┘      │
                           │              │
                           │ ADDR ──┐     │
                           │        │     │
                       GND ├────────┘     │
                           │              │
                           └──────────────┘
```

### I2C地址配置
通过ADDR引脚配置I2C地址：

| ADDR连接 | I2C地址 |
|---------|---------|
| GND | 0x24（默认） |
| VDD | 0x25 |
| SCL | 0x26 |
| SDA | 0x27 |

---

## 快速开始

### 1. 初始化驱动

```c
#include "drivers/aw9535.h"

// 配置参数
aw9535_config_t config = {
    .i2c_cfg = {
        .i2c_port = I2C_NUM_0,              // I2C端口0
        .dev_addr = AW9535_I2C_ADDR_DEFAULT, // 0x24
        .sda_io = GPIO_NUM_2,                // SDA引脚
        .scl_io = GPIO_NUM_1,                // SCL引脚
        .freq_hz = 400000,                   // 400kHz
    },
    .int_io = GPIO_NUM_45,                   // 中断引脚（可选）
};

// 初始化
esp_err_t ret = aw9535_init(&config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "初始化失败");
}
```

### 2. GPIO输出控制

```c
// 配置为输出模式
aw9535_set_mode(AW9535_PIN_P0_0, AW9535_MODE_OUTPUT);

// 设置高电平
aw9535_set_level(AW9535_PIN_P0_0, AW9535_LEVEL_HIGH);

// 设置低电平
aw9535_set_level(AW9535_PIN_P0_0, AW9535_LEVEL_LOW);

// 电平翻转
aw9535_toggle_level(AW9535_PIN_P0_0);
```

### 3. GPIO输入读取

```c
// 配置为输入模式
aw9535_set_mode(AW9535_PIN_P1_0, AW9535_MODE_INPUT);

// 读取电平
aw9535_level_t level;
aw9535_get_level(AW9535_PIN_P1_0, &level);

if (level == AW9535_LEVEL_HIGH) {
    ESP_LOGI(TAG, "按钮按下");
}
```

### 4. 批量操作

```c
// 配置端口0所有引脚为输出
aw9535_set_mode_port0(0x00);  // 0=输出，1=输入

// 批量设置输出电平
aw9535_set_level_port0(0xFF);  // 全部高电平

// 批量读取输入
uint8_t input_value;
aw9535_get_level_port0(&input_value);
```

---

## API参考

### 初始化函数

#### `aw9535_init()`
```c
esp_err_t aw9535_init(const aw9535_config_t *config);
```
**功能**：初始化AW9535驱动  
**参数**：
- `config`：配置结构指针
**返回**：ESP_OK成功，其他为错误码

#### `aw9535_deinit()`
```c
esp_err_t aw9535_deinit(void);
```
**功能**：反初始化驱动，释放资源

#### `aw9535_reset()`
```c
esp_err_t aw9535_reset(void);
```
**功能**：软件复位设备到默认状态

### GPIO模式配置

#### `aw9535_set_mode()`
```c
esp_err_t aw9535_set_mode(aw9535_pin_t pin, aw9535_mode_t mode);
```
**功能**：设置单个引脚模式  
**参数**：
- `pin`：引脚编号（0-15）
- `mode`：模式（AW9535_MODE_INPUT / AW9535_MODE_OUTPUT）

#### `aw9535_set_mode_port0()` / `aw9535_set_mode_port1()`
```c
esp_err_t aw9535_set_mode_port0(uint8_t mode_mask);
esp_err_t aw9535_set_mode_port1(uint8_t mode_mask);
```
**功能**：批量设置端口模式  
**参数**：
- `mode_mask`：模式掩码（bit=1为输入，bit=0为输出）

### GPIO电平控制

#### `aw9535_set_level()`
```c
esp_err_t aw9535_set_level(aw9535_pin_t pin, aw9535_level_t level);
```
**功能**：设置输出电平  
**参数**：
- `pin`：引脚编号
- `level`：电平（AW9535_LEVEL_HIGH / AW9535_LEVEL_LOW）

#### `aw9535_get_level()`
```c
esp_err_t aw9535_get_level(aw9535_pin_t pin, aw9535_level_t *level);
```
**功能**：读取输入电平  
**参数**：
- `pin`：引脚编号
- `level`：返回的电平值指针

#### `aw9535_toggle_level()`
```c
esp_err_t aw9535_toggle_level(aw9535_pin_t pin);
```
**功能**：翻转输出电平

### 中断配置

#### `aw9535_set_interrupt()`
```c
esp_err_t aw9535_set_interrupt(aw9535_pin_t pin, 
                                aw9535_int_type_t int_type,
                                aw9535_isr_t isr_handler, 
                                void *user_data);
```
**功能**：配置引脚中断  
**参数**：
- `pin`：引脚编号
- `int_type`：中断类型（RISING/FALLING/BOTH_EDGE/DISABLE）
- `isr_handler`：中断回调函数
- `user_data`：用户数据指针

#### `aw9535_enable_interrupt()` / `aw9535_disable_interrupt()`
```c
esp_err_t aw9535_enable_interrupt(aw9535_pin_t pin);
esp_err_t aw9535_disable_interrupt(aw9535_pin_t pin);
```
**功能**：启用/禁用引脚中断

---

## 使用示例

### 示例1：LED控制

```c
void example_led_control(void)
{
    // 初始化驱动
    aw9535_config_t config = {
        .i2c_cfg = {
            .i2c_port = I2C_NUM_0,
            .dev_addr = 0x24,
            .sda_io = GPIO_NUM_2,
            .scl_io = GPIO_NUM_1,
            .freq_hz = 400000,
        },
        .int_io = GPIO_NUM_45,  // 中断引脚
    };
    aw9535_init(&config);
    
    // 配置P0.0为输出（连接LED）
    aw9535_set_mode(AW9535_PIN_P0_0, AW9535_MODE_OUTPUT);
    
    // LED闪烁
    for (int i = 0; i < 10; i++) {
        aw9535_set_level(AW9535_PIN_P0_0, AW9535_LEVEL_HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
        
        aw9535_set_level(AW9535_PIN_P0_0, AW9535_LEVEL_LOW);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 示例2：按钮检测

```c
void example_button_read(void)
{
    // 配置P1.0为输入（连接按钮）
    aw9535_set_mode(AW9535_PIN_P1_0, AW9535_MODE_INPUT);
    
    // 轮询读取按钮状态
    while (1) {
        aw9535_level_t level;
        aw9535_get_level(AW9535_PIN_P1_0, &level);
        
        if (level == AW9535_LEVEL_LOW) {
            ESP_LOGI(TAG, "按钮被按下");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 示例3：中断方式检测按钮

```c
// 中断回调函数
void button_isr_handler(aw9535_pin_t pin, aw9535_level_t level, void *user_data)
{
    ESP_LOGI(TAG, "按钮中断触发: Pin=%d, Level=%d", pin, level);
}

void example_button_interrupt(void)
{
    // 配置P1.0为输入
    aw9535_set_mode(AW9535_PIN_P1_0, AW9535_MODE_INPUT);
    
    // 配置下降沿中断
    aw9535_set_interrupt(AW9535_PIN_P1_0, 
                         AW9535_INT_FALLING,
                         button_isr_handler, 
                         NULL);
    
    // 启用中断
    aw9535_enable_interrupt(AW9535_PIN_P1_0);
}
```

### 示例4：流水灯

```c
void example_running_led(void)
{
    // 配置端口0所有引脚为输出
    aw9535_set_mode_port0(0x00);
    
    // 流水灯效果
    while (1) {
        for (int i = 0; i < 8; i++) {
            uint8_t pattern = (1 << i);
            aw9535_set_level_port0(pattern);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
```

### 示例5：多功能应用

```c
void example_mixed_application(void)
{
    // LED输出（P0.0-P0.2）
    aw9535_set_mode(AW9535_PIN_P0_0, AW9535_MODE_OUTPUT);  // 红色LED
    aw9535_set_mode(AW9535_PIN_P0_1, AW9535_MODE_OUTPUT);  // 绿色LED
    aw9535_set_mode(AW9535_PIN_P0_2, AW9535_MODE_OUTPUT);  // 蓝色LED
    
    // 按钮输入（P1.0-P1.2）
    aw9535_set_mode(AW9535_PIN_P1_0, AW9535_MODE_INPUT);   // 按钮1
    aw9535_set_mode(AW9535_PIN_P1_1, AW9535_MODE_INPUT);   // 按钮2
    aw9535_set_mode(AW9535_PIN_P1_2, AW9535_MODE_INPUT);   // 按钮3
    
    // 根据按钮控制LED
    while (1) {
        aw9535_level_t btn1, btn2, btn3;
        
        aw9535_get_level(AW9535_PIN_P1_0, &btn1);
        aw9535_get_level(AW9535_PIN_P1_1, &btn2);
        aw9535_get_level(AW9535_PIN_P1_2, &btn3);
        
        // 按钮按下（低电平）时点亮对应LED
        aw9535_set_level(AW9535_PIN_P0_0, btn1 == AW9535_LEVEL_LOW ? 
                         AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
        aw9535_set_level(AW9535_PIN_P0_1, btn2 == AW9535_LEVEL_LOW ? 
                         AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
        aw9535_set_level(AW9535_PIN_P0_2, btn3 == AW9535_LEVEL_LOW ? 
                         AW9535_LEVEL_HIGH : AW9535_LEVEL_LOW);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```