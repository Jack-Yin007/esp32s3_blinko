/*
 * Hardware Driver Interface - 硬件驱动接口
 * 
 * 本文件定义了动作执行所需的硬件驱动接口
 * 函数声明由应用层定义，具体实现由驱动开发同事完成
 * 
 * 开发流程：
 * 1. 在 main/src/drivers/ 目录下创建对应的驱动文件
 * 2. 实现本头文件中声明的所有函数
 * 3. 在 CMakeLists.txt 中添加新驱动文件
 * 4. 在 main/include/drivers/ 创建对应的头文件
 */

#ifndef HARDWARE_INTERFACE_H
#define HARDWARE_INTERFACE_H

#include <stdint.h>
#include "esp_err.h"
#include "app/action_table.h"

#ifdef __cplusplus
extern "C" {
#endif


//#define SUPPORT_270_SERVO_MOTOR 1

#define SUPPORT_90_SERVO_MOTOR 1

/* ==================== LED驱动接口 ==================== */

/**
 * @brief 初始化LED驱动
 * @return ESP_OK 成功
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/led_enhanced.c
 * - 使用AW9535 GPIO扩展器控制RGB LED (P1.4/P1.5/P1.6)
 * - 支持8种LED模式（纯色、闪烁、交替等）
 */
esp_err_t hw_led_init(void);

/**
 * @brief 执行LED模式
 * @param pattern LED模式ID (参见 led_pattern_id_t)
 * @return ESP_OK 成功，ESP_FAIL 失败
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/led_driver.c (已存在，需增强)
 * - 需要支持的模式：纯色、呼吸、渐变、闪烁、波浪等
 * - 可以使用 WS2812/SK6812 RGB LED灯带
 */
esp_err_t hw_led_set_pattern(led_pattern_id_t pattern);

/**
 * @brief 设置LED颜色（用于纯色模式）
 * @param r 红色分量 (0-255)
 * @param g 绿色分量 (0-255)
 * @param b 蓝色分量 (0-255)
 * @return ESP_OK 成功
 */
esp_err_t hw_led_set_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief 停止LED效果
 * @return ESP_OK 成功
 */
esp_err_t hw_led_stop(void);

/* ==================== 震动马达驱动接口 ==================== */

/**
 * @brief 执行震动模式
 * @param pattern 震动模式ID (参见 vibration_pattern_id_t)
 * @return ESP_OK 成功，ESP_FAIL 失败
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/vibrator.c
 * - 使用GPIO控制震动马达
 * - 支持3种震动模式：震动一下、双震、高频轻震
 * - GPIO: GPIO_NUM_9 (MOTO3_RSTN)
 */

/**
 * @brief 初始化震动马达
 * @return ESP_OK 成功
 */
esp_err_t hw_vibrator_init(void);

/**
 * @brief 设置震动模式
 * @param pattern 震动模式ID
 * @return ESP_OK 成功
 */
esp_err_t hw_vibrator_set_pattern(vibration_pattern_id_t pattern);

/**
 * @brief 设置震动强度
 * @param intensity 强度百分比 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t hw_vibrator_set_intensity(uint8_t intensity);

/**
 * @brief 停止震动
 * @return ESP_OK 成功
 */
esp_err_t hw_vibrator_stop(void);

/* ==================== 音频播放驱动接口 ==================== */

/**
 * @brief 播放音效
 * @param effect_id 音效ID (参见 sound_effect_id_t)
 * @return ESP_OK 成功，ESP_FAIL 失败
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/audio_player.c (✅ 已完成)
 * - 使用MAX98357A I2S DAC输出音频
 * - 支持9种预定义音效（基于音调序列）
 * - 增益控制：4档 (+3/+6/+9/+12 dB)
 * - 关断模式：SD_MODE控制省电
 */
esp_err_t hw_audio_play_wav(sound_effect_id_t wav_id);

/**
 * @brief 初始化音频播放器
 * @return ESP_OK 成功
 */
esp_err_t hw_audio_player_init(void);

/**
 * @brief 停止音频播放
 * @return ESP_OK 成功
 */
esp_err_t hw_audio_stop(void);

/**
 * @brief 设置音量
 * @param volume 音量百分比 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t hw_audio_set_volume(uint8_t volume);

/**
 * @brief 检查是否正在播放
 * @return true 正在播放，false 空闲
 */
bool hw_audio_is_playing(void);

/**
 * @brief 测试所有音效
 * @return ESP_OK 成功
 * 
 * 依次播放所有9种音效，用于验证音频系统
 */
esp_err_t hw_audio_test_all_effects(void);

/* ==================== 触摸传感器驱动接口 (TTP223) ==================== */

/**
 * @brief 初始化触摸传感器
 * @return ESP_OK 成功
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/touch_sensor.c (✅ 已完成)
 * - 头文件：main/include/drivers/touch_ttp223.h
 * - 使用TTP223数字触摸传感器，双通道
 * - GPIO4: 额头触摸 (TOUCH_INTN_1)
 * - GPIO5: 后背触摸 (TOUCH_INTN_2)
 * - 高电平=触摸，低电平=未触摸
 * - ✅ 支持GPIO中断模式（上升沿/下降沿）
 * - ✅ 支持事件回调机制
 * - ✅ 60ms防抖处理
 */
esp_err_t hw_touch_sensor_init(void);

/**
 * @brief 检测额头触摸状态
 * @return true 已触摸，false 未触摸
 * 
 * 读取GPIO4状态（高电平=触摸）
 */
bool hw_touch_sensor_is_forehead_touched(void);

/**
 * @brief 检测后背触摸状态
 * @return true 已触摸，false 未触摸
 * 
 * 读取GPIO5状态（高电平=触摸）
 */
bool hw_touch_sensor_is_back_touched(void);

/**
 * @brief 检测任意触摸状态
 * @return true 任意一个通道被触摸，false 都未触摸
 */
bool hw_touch_sensor_is_any_touched(void);

/* ==================== 马达驱动接口 ==================== */

/**
 * @brief 马达ID定义
 */
typedef enum {
    MOTOR_NOD = 0,      // 点头马达
    MOTOR_SHAKE = 1     // 摇头马达
} motor_id_t;

/**
 * @brief 马达方向定义
 */
typedef enum {
    MOTOR_DIR_STOP = 0,         // 停止
    MOTOR_DIR_FORWARD = 1,      // 正转   // 点头马达，我们只有一个方向，结构实现方向变化
    MOTOR_DIR_BACKWARD = 2,     // 反转
    MOTOR_DIR_BRAKE = 3,        // 刹车 
    MOTOR_DIR_TOGGLE = 4,        // 每次先往右转，再往左转 

} motor_direction_t;

#define DIRECTION_CHANGE(a)  ((a) = ((a) == MOTOR_DIR_FORWARD) ? MOTOR_DIR_BACKWARD : MOTOR_DIR_FORWARD)      

/**
 * @brief 初始化马达驱动
 * @return ESP_OK 成功
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/motor.c
 * - 点头马达: IN1-GPIO46, IN2-GPIO3
 * - 摇头马达: IN1-GPIO21, IN2-GPIO14
 * - 双PWM控制方式
 * - 点头马达：连续旋转，机械结构控制反复点头
 * - 摇头马达：时间控制正反转实现摇头
 */
esp_err_t hw_motor_init(void);

/**
 * @brief 设置马达方向和速度
 * @param motor 马达ID (MOTOR_NOD 或 MOTOR_SHAKE)
 * @param direction 方向 (正转/反转/停止)
 * @param speed 速度 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t hw_motor_set(motor_id_t motor, motor_direction_t direction, uint8_t speed);

/**
 * @brief 停止指定马达
 * @param motor 马达ID
 * @return ESP_OK 成功
 */
esp_err_t hw_motor_stop(motor_id_t motor);

/**
 * @brief 停止所有马达
 * @return ESP_OK 成功
 */
esp_err_t hw_motor_stop_all(void);

/**
 * @brief 获取马达初始化状态
 * @return true 已初始化，false 未初始化
 */
bool hw_motor_is_initialized(void);


typedef enum {
    ANGLE_INIT,
    #if SUPPORT_270_SERVO_MOTOR
    ANGLE_10_LEFT,
    ANGLE_10_RIGHT,
    ANGLE_10_BOTH,
    ANGLE_15_LEFT,
    ANGLE_15_RIGHT,
    ANGLE_15_BOTH,
    ANGLE_20_LEFT,
    ANGLE_20_RIGHT,
    ANGLE_20_BOTH,
    ANGLE_30_LEFT,
    ANGLE_30_RIGHT,
    ANGLE_30_BOTH,
    ANGLE_45_LEFT,
    ANGLE_45_RIGHT,
    ANGLE_45_BOTH,
    ANGLE_60_LEFT,
    ANGLE_60_RIGHT,
    ANGLE_60_BOTH,
    #elif SUPPORT_90_SERVO_MOTOR
    ANGLE_2_LEFT,
    ANGLE_2_RIGHT,
    ANGLE_2_BOTH,
    ANGLE_5_LEFT,
    ANGLE_5_RIGHT,
    ANGLE_5_BOTH,
    ANGLE_10_LEFT,
    ANGLE_10_RIGHT,
    ANGLE_10_BOTH,
    ANGLE_15_LEFT,
    ANGLE_15_RIGHT,
    ANGLE_15_BOTH,
    ANGLE_20_LEFT,
    ANGLE_20_RIGHT,
    ANGLE_20_BOTH,
    ANGLE_25_LEFT,
    ANGLE_25_RIGHT,
    ANGLE_25_BOTH,
    #endif /*SUPPORT_270_SERVO_MOTOR SUPPORT_90_SERVO_MOTOR*/
    ANGLE_NUM_MAX,
}angle_type_e;


esp_err_t hw_servo_motor_init(void);
bool hw_servo_motor_is_initialized(void);
void start_the_shake(uint8_t type);
void stop_the_shake(void);
void start_the_node(uint8_t type);
void stop_the_node(void);
void start_shake_and_node(uint8_t shake_type, uint8_t node_type);
void stop_shake_and_node();

#if SUPPORT_90_SERVO_MOTOR
void start_the_ear_left(uint8_t type);
void stop_the_ear_left(void);
void start_the_ear_right(uint8_t type);
void stop_the_ear_right(void);
void start_ear_left_and_right(uint8_t left_type, uint8_t right_type);
void stop_ear_left_and_right();
#endif/*SUPPORT_90_SERVO_MOTOR*/

/* ==================== PIR接近传感器驱动接口 ==================== */

/**
 * @brief 初始化PIR人体感应传感器
 * @return ESP_OK 成功
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/pir_sensor.c (已实现)
 * - GPIO6: PIR传感器输出引脚
 * - 高电平=检测到人体，低电平=未检测到
 * - 感应距离：3-7米，角度：约120度
 * - 需要30-60秒预热时间
 */
esp_err_t hw_pir_sensor_init(void);

/**
 * @brief 检测人体移动
 * @return true 检测到人体，false 未检测到
 * 
 * 读取GPIO6状态（高电平=检测到人体）
 */
bool hw_pir_sensor_is_detected(void);

/**
 * @brief 获取PIR传感器原始GPIO电平
 * @return 0=低电平（未检测到），1=高电平（检测到）
 */
int hw_pir_sensor_get_raw_level(void);

/* ==================== 麦克风驱动接口 ==================== */

/**
 * @brief 初始化麦克风
 * @return ESP_OK 成功
 * 
 * 驱动开发说明：
 * - 实现文件：main/src/drivers/microphone.c (✅ 已完成)
 * - 使用INMP441数字麦克风（I2S接口）
 * - 支持8kHz/16bit采样率（BLE音频传输）
 * - 实时声音检测和RMS音量计算
 * - 噪声基线自动校准
 */
esp_err_t hw_microphone_init(void);

/**
 * @brief 反初始化麦克风（释放资源）
 * @return ESP_OK 成功
 */
esp_err_t hw_microphone_deinit(void);

/**
 * @brief 启用BLE音频流传输
 * @param callback 音频数据回调函数（接收PCM数据）
 * @return ESP_OK 成功
 */
esp_err_t hw_microphone_enable_streaming(void (*callback)(const uint8_t *data, uint16_t len));

/**
 * @brief 禁用BLE音频流传输
 * @return ESP_OK 成功
 */
esp_err_t hw_microphone_disable_streaming(void);

/**
 * @brief 获取当前声音强度（用于语音触发检测）
 * @return 声音强度 (0-100)
 */
uint8_t hw_microphone_get_sound_level(void);

/**
 * @brief 检测是否有声音
 * @return true 检测到声音，false 无声音
 */
bool hw_microphone_is_sound_detected(void);

/**
 * @brief 设置声音检测阈值
 * @param threshold 阈值 (0-100)
 * @return ESP_OK 成功
 */
esp_err_t hw_microphone_set_threshold(uint8_t threshold);

/**
 * @brief 重新校准噪声基线
 * @return ESP_OK 成功
 */
esp_err_t hw_microphone_calibrate(void);

/**
 * @brief 获取噪声基线
 * @return 噪声基线值
 */
uint32_t hw_microphone_get_noise_floor(void);

/* ==================== 电源管理接口 ==================== */

/**
 * @brief 获取电池电量
 * @return 电量百分比 (0-100)
 * 
 * 驱动开发说明：
 * - 使用ADC读取电池电压
 * - 通过分压电路监测电池电压
 * - 转换为电量百分比
 */
uint8_t hw_power_get_battery_level(void);

/**
 * @brief 检测充电状态
 * @return true 正在充电，false 未充电
 */
bool hw_power_is_charging(void);

/* ==================== 环境传感器接口（可选） ==================== */

/**
 * @brief 读取温度
 * @return 温度值（摄氏度），-127表示失败
 * 
 * 驱动开发说明：
 * - 可选实现，使用DHT22、SHT30等传感器
 * - I2C或单总线接口
 */
int8_t hw_sensor_read_temperature(void);

/**
 * @brief 读取湿度
 * @return 湿度百分比 (0-100)，255表示失败
 */
uint8_t hw_sensor_read_humidity(void);

/**
 * @brief 读取光照强度
 * @return 光照值 (0-100)，255表示失败
 */
uint8_t hw_sensor_read_light(void);

#ifdef __cplusplus
}
#endif

#endif // HARDWARE_INTERFACE_H
