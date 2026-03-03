/**
 * @file action_executor.c  
 * @brief 动作执行器 - 实现Blinko动作表中的复杂马达动作
 * 
 * 基于Excel表格DEFGHI列的马达动作需求：
 * - D列：点头方向 (正转/反转/停止)
 * - E列：点头速度 (0-100)
 * - F列：点头持续时间(ms)
 * - G列：摇头方向 (左摇/右摇/停止)
 * - H列：摇头速度 (0-100)
 * - I列：摇头持续时间(ms)
 * 
 * 设计思路：
 * - 两个马达可以同时运行
 * - 各自独立的运行时长
 * - 任务负责时序控制和停止
 */

#include "app/action_executor.h"
#include "drivers/hardware_interface.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "ActionExec";

/* ==================== 动作配置结构 ==================== */
// typedef struct {
//     // 点头参数
//     motor_direction_t nod_dir;
//     uint8_t nod_speed;
//     uint16_t nod_duration_ms;
//     uint8_t nod_execute_num;   
//     uint16_t nod_interval_ms;
//     uint8_t nod_priority;
//     // 摇头参数
//     motor_direction_t shake_dir;
//     uint8_t shake_speed;
//     uint16_t shake_duration_ms;
//     uint8_t shake_execute_num;   
//     uint16_t shake_interval_ms;
//     uint8_t shake_priority;
// } motor_action_config_t;

/* ==================== 全局变量 ==================== */
static TaskHandle_t g_action_task = NULL;
static QueueHandle_t g_action_queue = NULL;
static bool g_executor_initialized = false;
static bool g_action_active = false;
static motor_action_config_t *g_ear_action = NULL;
static TaskHandle_t g_ear_action_task = NULL;
/* ==================== 私有函数 ==================== */

/**
 * @brief 执行单个马达动作
 */
__attribute__((unused)) static void execute_motor_action(const motor_action_config_t *action)
{
    if (action == NULL) {
        return;
    }

    g_action_active = true;

    ESP_LOGI(TAG, "Executing motor action:");
    ESP_LOGI(TAG, "  Nod: dir=%d, speed=%d%%, duration=%dms",
             action->nod_dir, action->nod_speed, action->nod_duration_ms);
    ESP_LOGI(TAG, "  Shake: dir=%d, speed=%d%%, duration=%dms",
             action->shake_dir, action->shake_speed, action->shake_duration_ms);

    // 同时启动两个马达
    hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
    hw_motor_set(MOTOR_SHAKE, action->shake_dir, action->shake_speed);

    // 计算两个马达的最大运行时间
    uint16_t nod_duration = action->nod_duration_ms;
    uint16_t shake_duration = action->shake_duration_ms;
    uint16_t max_duration = (nod_duration > shake_duration) ? nod_duration : shake_duration;

    // 使用分段延时，分别停止各个马达
    if (nod_duration > 0 && shake_duration > 0) {
        if (nod_duration < shake_duration) {
            // 点头马达先停止
            vTaskDelay(pdMS_TO_TICKS(nod_duration));
            hw_motor_stop(MOTOR_NOD);
            vTaskDelay(pdMS_TO_TICKS(shake_duration - nod_duration));
            hw_motor_stop(MOTOR_SHAKE);
        } else if (shake_duration < nod_duration) {
            // 摇头马达先停止
            vTaskDelay(pdMS_TO_TICKS(shake_duration));
            hw_motor_stop(MOTOR_SHAKE);
            vTaskDelay(pdMS_TO_TICKS(nod_duration - shake_duration));
            hw_motor_stop(MOTOR_NOD);
        } else {
            // 同时停止
            vTaskDelay(pdMS_TO_TICKS(max_duration));
            hw_motor_stop_all();
        }
    } else if (nod_duration > 0) {
        // 仅点头马达运行
        vTaskDelay(pdMS_TO_TICKS(nod_duration));
        hw_motor_stop(MOTOR_NOD);
    } else if (shake_duration > 0) {
        // 仅摇头马达运行
        vTaskDelay(pdMS_TO_TICKS(shake_duration));
        hw_motor_stop(MOTOR_SHAKE);
    }

    // 确保全部停止
    hw_motor_stop_all();
    
    ESP_LOGI(TAG, "Motor action completed");
    g_action_active = false;
}

void motor_time_control (uint16_t nod_duration, uint16_t shake_duration, uint32_t execute_duration)
{
    if (nod_duration <= shake_duration) {
    vTaskDelay(pdMS_TO_TICKS(nod_duration));
    hw_motor_stop(MOTOR_NOD);
    vTaskDelay(pdMS_TO_TICKS(shake_duration - nod_duration));
    }
    else
    vTaskDelay(pdMS_TO_TICKS(shake_duration));

}
//基位置在最右边   
//      45
//      30
//      15
 
esp_err_t motor_control(const motor_action_config_t *action)
{

    uint32_t total_duration = 0;
    uint16_t node_duration = action->nod_duration_ms;
    uint8_t speed = action->shake_speed;
    uint16_t unit_time = ((speed == 100)  ? QUICK_UNIT_ANGLE_TIME : (speed == 70 ? MID_UNIT_ANGLE_TIME : SLOW_UNIT_ANGLE_TIME));
    uint8_t angle = 0;
    uint16_t full_rotation_time = ((MAX_ANGLE-angle)/UNIT_ANGLE ) * unit_time;
    angle = action->shake_duration_ms / unit_time;
    uint16_t real_rotation_time = (angle/UNIT_ANGLE ) * unit_time;
    uint16_t shake_duration = full_rotation_time*2;  //if people change the location, we can correct the location to rightmost 
    uint8_t dir = ((action->shake_dir == MOTOR_DIR_TOGGLE) ? MOTOR_DIR_FORWARD : action->shake_dir);
    uint8_t execute_num = action->shake_execute_num;
    bool running_at_same_time = false;

    if (action->nod_priority > action->shake_priority) {
    
        if (action->nod_duration_ms > 0) {   //only delay time had set, we start the motor
            hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
            vTaskDelay(pdMS_TO_TICKS(action->nod_duration_ms));
            hw_motor_stop(MOTOR_NOD);
        }
    }
    if (action->nod_priority == action->shake_priority)
    {
        if (action->nod_duration_ms > 0) { 
        running_at_same_time = true;
        hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
        }
    }

    if (action->shake_duration_ms) {  //only delay time had set, we start the motor

        hw_motor_set(MOTOR_SHAKE, dir, speed);

        if (angle == 15) { // for adction_d3
            shake_duration = 175;
        }
        total_duration += shake_duration;
        motor_time_control (node_duration, shake_duration,  total_duration);
        hw_motor_stop(MOTOR_SHAKE);
        if (action->shake_dir == MOTOR_DIR_FORWARD || action->shake_dir == MOTOR_DIR_BACKWARD) {
            hw_motor_set(MOTOR_SHAKE, DIRECTION_CHANGE(dir), speed);
            shake_duration = full_rotation_time ; //back to original position
            if (running_at_same_time )
            {
                //shake_duration += 60;
            }

            else if (angle != MAX_ANGLE)
            {
                //shake_duration -= HIGH_DELTA_TIME;
            }

            else if (unit_time != QUICK_UNIT_ANGLE_TIME)
            {
                if (action->shake_dir == MOTOR_DIR_FORWARD)
                shake_duration += 0 ;
                else
                shake_duration -= MID_DELTA_TIME;
            }


            //113025
            if (unit_time == QUICK_UNIT_ANGLE_TIME)
            {
                if (action->shake_dir == MOTOR_DIR_FORWARD)
                shake_duration -= 80 ;
                else
                shake_duration += 0; // for s4
            }

            if (angle == 15) { // for adction_d3
                shake_duration = 175;
            }

            total_duration += shake_duration;
            node_duration -= shake_duration;
            motor_time_control (node_duration, shake_duration,  total_duration);
            hw_motor_stop(MOTOR_SHAKE);
            for (uint8_t i = 1; i < (execute_num);)
            {
                shake_duration = real_rotation_time;
                hw_motor_set(MOTOR_SHAKE, dir, speed);
                total_duration += shake_duration;
                node_duration -= shake_duration;
                motor_time_control (node_duration, shake_duration,  total_duration);
                hw_motor_stop(MOTOR_SHAKE);
                hw_motor_set(MOTOR_SHAKE, DIRECTION_CHANGE(dir), speed);
                total_duration += shake_duration;
                node_duration -= shake_duration;
                motor_time_control (node_duration, shake_duration,  total_duration);
                hw_motor_stop(MOTOR_SHAKE);
                i++;
            }
            
        }
        else if (action->shake_dir == MOTOR_DIR_TOGGLE) //我们总是先从右开始
        {
            shake_duration = full_rotation_time ; 
            shake_duration += real_rotation_time; 
            hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_BACKWARD, speed);
            total_duration += shake_duration;
            node_duration -= shake_duration;
            motor_time_control (node_duration, shake_duration,  total_duration);
            hw_motor_stop(MOTOR_SHAKE);
            shake_duration = real_rotation_time;
            hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_FORWARD, speed);
            total_duration += shake_duration;
            node_duration -= shake_duration;
            if (angle == MAX_ANGLE && unit_time != QUICK_UNIT_ANGLE_TIME) {
            shake_duration -= HIGH_DELTA_TIME;
            }
            if (execute_num <= 1 ) {
                if (unit_time == QUICK_UNIT_ANGLE_TIME)
                //shake_duration -= 40;  s3
                shake_duration -= 90;

                if (unit_time == MID_UNIT_ANGLE_TIME)
                shake_duration += 120;
            }
            
            motor_time_control (node_duration, shake_duration,  total_duration);
            hw_motor_stop(MOTOR_SHAKE);
    REPEAT: 
            if (execute_num > 1) //兼容老的写法
            { 
                hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_FORWARD, speed);
                total_duration += shake_duration;
                node_duration -= shake_duration;
                motor_time_control (node_duration, shake_duration,  total_duration);
                hw_motor_stop(MOTOR_SHAKE);
                shake_duration = real_rotation_time *2 ; 
                hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_BACKWARD, speed);
                total_duration += shake_duration;
                node_duration -= shake_duration;
                motor_time_control (node_duration, shake_duration,  total_duration);
                hw_motor_stop(MOTOR_SHAKE);
                shake_duration = real_rotation_time;
                node_duration -= shake_duration;
                hw_motor_set(MOTOR_SHAKE, MOTOR_DIR_FORWARD, speed);
                total_duration += shake_duration;
                node_duration -= shake_duration;
                shake_duration -= HIGH_DELTA_TIME;
                motor_time_control (node_duration, shake_duration,  total_duration);
                hw_motor_stop(MOTOR_SHAKE);
                execute_num--;
                goto REPEAT;
            }

        }
    }

    if ((node_duration > total_duration) && (action->nod_priority == action->shake_priority))
    {
        if (action->nod_duration_ms) {
            vTaskDelay(pdMS_TO_TICKS(node_duration - total_duration));
            hw_motor_stop(MOTOR_NOD);
        }
    }
    if (action->nod_priority < action->shake_priority)
    {
        if (action->nod_duration_ms) {
            hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
            vTaskDelay(pdMS_TO_TICKS(action->nod_duration_ms));
            hw_motor_stop(MOTOR_NOD);
        }
    }

    return ESP_OK;
} 



esp_err_t servo_motor_control(const motor_action_config_t *action)
{

    uint8_t i = 1, j = 1;
    if (action->nod_priority > action->shake_priority)
    {
        start_the_node(action->nod_dir);
        for(; i < action->nod_execute_num; i++) {
            start_the_node(action->nod_dir);
        }
        start_the_shake(action->shake_dir);
        for(; j < action->shake_execute_num; j++) {
            start_the_shake(action->shake_dir);
        }
    }
    else if (action->shake_priority > action->nod_priority) {
        start_the_shake(action->shake_dir);
        for(; j < action->shake_execute_num; j++) {
            start_the_shake(action->shake_dir);
        }
        start_the_node(action->nod_dir);
        for(; i < action->nod_execute_num; i++) {
            start_the_node(action->nod_dir);
        }
    }
    else if (action->shake_priority == action->nod_priority) {
        start_shake_and_node(action->shake_dir, action->nod_dir);
        for(; i < action->nod_execute_num; i++) {
            start_shake_and_node(action->shake_dir, action->nod_dir);
        }
    }
    return ESP_OK;
}


#if SUPPORT_90_SERVO_MOTOR
esp_err_t ear_servo_motor_control(const motor_action_config_t *action)
{

    uint8_t i = 1, j = 1;
    if (action->ear_right_priority > action->ear_left_priority)
    {
        start_the_ear_right(action->ear_right_dir);
        for(; i < action->ear_right_execute_num; i++) {
            start_the_ear_right(action->ear_right_dir);
        }
        start_the_ear_left(action->ear_left_dir);
        for(; j < action->ear_left_execute_num; j++) {
            start_the_ear_left(action->ear_left_dir);
        }
    }
    else if (action->ear_left_priority > action->ear_right_priority) {
        start_the_ear_left(action->ear_left_dir);
        for(; j < action->ear_left_execute_num; j++) {
            start_the_ear_left(action->ear_left_dir);
        }
        start_the_ear_right(action->ear_right_dir);
        for(; i < action->ear_right_execute_num; i++) {
            start_the_ear_right(action->ear_right_dir);
        }
    }
    else if (action->ear_left_priority == action->ear_right_priority) {
        start_ear_left_and_right(action->ear_left_dir, action->ear_right_dir);
        for(; i < action->ear_right_execute_num; i++) {
            start_ear_left_and_right(action->ear_left_dir, action->ear_right_dir);
        }
    }
    return ESP_OK;
}
#endif /*SUPPORT_90_SERVO_MOTOR*/
static void execute_motor_action_cfg(const motor_action_config_t *action)
{
    if (action == NULL) {
        return;
    }

    g_action_active = true;

    ESP_LOGI(TAG, "Executing motor action:");
    ESP_LOGI(TAG, "  Nod: dir=%d, speed=%d%%, duration=%dms repeat=%d, prority=%d",
             action->nod_dir, action->nod_speed, action->nod_duration_ms, action->nod_execute_num, action->nod_priority);
    ESP_LOGI(TAG, "  Shake: dir=%d, speed=%d%%, duration=%dms repeat=%d, prority=%d",
             action->shake_dir, action->shake_speed, action->shake_duration_ms, action->shake_execute_num, action->shake_priority);

    ESP_LOGI(TAG, "  ear left: dir=%d, repeat=%d, prority=%d, ear right: dir=%d, repeat=%d, prority=%d",
             action->ear_left_dir, action->ear_left_execute_num, action->ear_left_priority, action->ear_right_dir, action->ear_right_execute_num, action->ear_right_priority);


//     if (action->nod_priority > action->shake_priority)
//     {
//         hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
//         vTaskDelay(pdMS_TO_TICKS(action->nod_duration_ms));
//         hw_motor_stop(MOTOR_NOD);

//         motor_control(action);
//     }
//     else if (action->shake_priority > action->nod_priority)
//     {
//         motor_control(action);
//         hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
//         vTaskDelay(pdMS_TO_TICKS(action->nod_duration_ms));
//         hw_motor_stop(MOTOR_NOD);
//     }
//     else {

//         if (action ->shake_duration_ms == 0)
//         {
//             hw_motor_set(MOTOR_NOD, action->nod_dir, action->nod_speed);
//             vTaskDelay(pdMS_TO_TICKS(action->nod_duration_ms));
//         }
//         else
//         motor_control(action);
//   }

    servo_motor_control(action);

    // motor_control(action);
    // // 确保全部停止
    // hw_motor_stop_all();
    
    ESP_LOGI(TAG, "Motor action completed");
    g_action_active = false;
}


/**
 * @brief 动作执行任务
 */
static void action_executor_task(void *arg)
{
    motor_action_config_t action;

    while (1) {
        // 从队列接收动作
        if (xQueueReceive(g_action_queue, &action, portMAX_DELAY) == pdTRUE && !action_executor_is_active()) {
            #if SUPPORT_90_SERVO_MOTOR
            g_ear_action = &action; // pass to another task to work
            #endif/*SUPPORT_90_SERVO_MOTOR*/
            execute_motor_action_cfg(&action);
        }
    }
}

static void action_ear_executor_task(void *arg)
{
    motor_action_config_t action;

    while (1) {

        if (g_ear_action) {
            #if SUPPORT_90_SERVO_MOTOR
            ear_servo_motor_control(g_ear_action);
            #endif/*SUPPORT_90_SERVO_MOTOR*/
            g_ear_action = NULL;
            
        }

        vTaskDelay(pdMS_TO_TICKS(10));

    }
}

/* ==================== 公共API ==================== */

/**
 * @brief 初始化动作执行器
 */
esp_err_t action_executor_init(void)
{
    if (g_executor_initialized) {
        ESP_LOGW(TAG, "Action executor already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing action executor...");

    // 创建动作队列
    g_action_queue = xQueueCreate(5, sizeof(motor_action_config_t));
    if (g_action_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create action queue");
        return ESP_FAIL;
    }

    // 创建动作马达执行任务
    BaseType_t ret = xTaskCreate(
        action_executor_task,
        "action_exec",
        4096,
        NULL,
        4,
        &g_action_task
    );

    #if SUPPORT_90_SERVO_MOTOR
    // 创建耳朵马达执行任务
    BaseType_t ret1 = xTaskCreate(
        action_ear_executor_task,
        "action_ear_exec",
        4096,
        NULL,
        4,
        &g_ear_action_task
    );
    #endif/*SUPPORT_90_SERVO_MOTOR*/

    if (ret != pdPASS 
        #if SUPPORT_90_SERVO_MOTOR
        || ret1 != pdPASS
        #endif/*SUPPORT_90_SERVO_MOTOR*/
    ) {
        ESP_LOGE(TAG, "Failed to action executor task");
        vQueueDelete(g_action_queue);
        return ESP_FAIL;
    }

    g_executor_initialized = true;
    ESP_LOGI(TAG, "Action executor initialized successfully");

    return ESP_OK;
}

/**
 * @brief 执行马达动作（应用层接口）
 * @param nod_dir 点头方向
 * @param nod_speed 点头速度 (0-100)
 * @param nod_duration 点头持续时间(ms)
 * @param shake_dir 摇头方向
 * @param shake_speed 摇头速度 (0-100)
 * @param shake_duration 摇头持续时间(ms)
 */
esp_err_t action_executor_run_motor_action(
    motor_direction_t nod_dir, uint8_t nod_speed, uint16_t nod_duration,
    motor_direction_t shake_dir, uint8_t shake_speed, uint16_t shake_duration)
{
    if (!g_executor_initialized) {
        ESP_LOGW(TAG, "Action executor not initialized");
        return ESP_FAIL;
    }

    motor_action_config_t action = {
        .nod_dir = nod_dir,
        .nod_speed = nod_speed,
        .nod_duration_ms = nod_duration,
        .shake_dir = shake_dir,
        .shake_speed = shake_speed,
        .shake_duration_ms = shake_duration
    };

    // 发送到队列
    if (xQueueSend(g_action_queue, &action, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Action queue full, action dropped");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Motor action queued");
    return ESP_OK;
}

extern  uint32_t test;

esp_err_t load_action_to_motor(
    motor_direction_t nod_dir, uint8_t nod_speed, uint16_t nod_duration, uint8_t nod_execute_num , uint16_t nod_interval_ms, uint8_t nod_priority,  

    motor_direction_t shake_dir, uint8_t shake_speed, uint16_t shake_duration,  uint8_t shake_execute_num , uint16_t shake_interval_ms, uint8_t shake_priority,
    motor_direction_t ear_left_dir,  uint8_t ear_left_priority, uint8_t ear_left_execute_num,
    motor_direction_t ear_right_dir, uint8_t ear_right_priority, uint8_t ear_right_execute_num )
{
    if (!g_executor_initialized) {
        ESP_LOGW(TAG, "Action executor not initialized");
        return ESP_FAIL;
    }



    motor_action_config_t action = {
        .nod_dir                =   nod_dir,
        .nod_speed              =   nod_speed,
        .nod_duration_ms        =   nod_duration,
        .nod_execute_num        =   nod_execute_num,
        .nod_interval_ms        =   nod_interval_ms,
        .nod_priority           =   nod_priority,
        .shake_dir              =   shake_dir,
        .shake_speed            =   shake_speed,
        .shake_duration_ms      =   shake_duration,
        .shake_execute_num      =   shake_execute_num,
        .shake_interval_ms      =   shake_interval_ms,
        .shake_priority         =   shake_priority,
        .ear_left_dir           =   ear_left_dir,
        .ear_left_priority      =   ear_left_priority,
        .ear_left_execute_num   =   ear_left_execute_num,
        .ear_right_dir          =   ear_right_dir,
        .ear_right_priority     =   ear_right_priority,
        .ear_right_execute_num  =   ear_right_execute_num,
    };

    // 发送到队列
    if (xQueueSend(g_action_queue, &action, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Action queue full, action dropped");
        return ESP_FAIL;
    }
    test++;
    ESP_LOGI(TAG, "Motor action queued");
    return ESP_OK;
}
/**
 * @brief 从动作表执行动作组合
 * @param combo_id 动作组合ID
 */
// esp_err_t action_executor_execute_combo(uint8_t combo_id)
// {
//     if (!g_executor_initialized) {
//         ESP_LOGW(TAG, "Action executor not initialized");
//         return ESP_FAIL;
//     }

//     const action_combo_t *combo = action_table_get_combo_by_id(combo_id);
//     if (combo == NULL) {
//         ESP_LOGW(TAG, "Invalid combo ID: 0x%02X", combo_id);
//         return ESP_ERR_INVALID_ARG;
//     }

//     ESP_LOGI(TAG, "Executing combo: %s (0x%02X)", 
//              combo->name ? combo->name : "Unknown", combo_id);

//     // 从动作表解析并执行
//     return action_executor_run_motor_action(
//         (motor_direction_t)combo->nod_direction,
//         combo->nod_speed,
//         combo->nod_duration,
//         (motor_direction_t)combo->shake_direction,
//         combo->shake_speed,
//         combo->shake_duration
//     );
// }


esp_err_t action_executor_execute_combo(uint8_t combo_id)
{
    if (!g_executor_initialized) {
        ESP_LOGW(TAG, "Action executor not initialized");
        return ESP_FAIL;
    }

    const action_combo_t *combo = action_table_get_combo_by_id(combo_id);
    if (combo == NULL) {
        ESP_LOGW(TAG, "Invalid combo ID: 0x%02X", combo_id);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Executing combo: %s (0x%02X)", 
             combo->name ? combo->name : "Unknown", combo_id);



    return load_action_to_motor(
        (motor_direction_t)combo->nod_direction,
        combo->nod_speed,
        combo->nod_duration,
        combo->nod_execute_num,
        combo->nod_interval_ms,
        combo->nod_priority,
        (motor_direction_t)combo->shake_direction,
        combo->shake_speed,
        combo->shake_duration,
        combo->shake_execute_num,
        combo->shake_interval_ms,
        combo->shake_priority,
        combo->ear_left_direction,
        combo->ear_left_priority,
        combo->ear_left_repeat_num,
        combo->ear_right_direction,
        combo->ear_right_priority,
        combo->ear_right_repeat_num
    );
}
/**
 * @brief 停止当前动作
 */
esp_err_t action_executor_stop(void)
{
    if (!g_executor_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping current action");
    
    // 清空队列
    xQueueReset(g_action_queue);
    
    // 停止所有马达
    //hw_motor_stop_all();
    stop_shake_and_node();

    g_action_active = false;

    return ESP_OK;
}

/**
 * @brief 检查是否有动作正在执行
 */
bool action_executor_is_active(void)
{
    return g_action_active;
}

/**
 * @brief 获取队列中待执行的动作数量
 */
uint32_t action_executor_get_queue_count(void)
{
    if (!g_executor_initialized || g_action_queue == NULL) {
        return 0;
    }
    return uxQueueMessagesWaiting(g_action_queue);
}
