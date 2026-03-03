/**
 * @file motor_test.h
 * @brief 马达驱动测试接口
 */

#ifndef MOTOR_TEST_H
#define MOTOR_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动马达测试
 * @note 在主程序初始化完成后调用此函数启动测试任务
 * 
 * 使用方法：
 * 1. 在 pet_main.c 的 app_main() 最后添加：
 *    #ifdef CONFIG_MOTOR_TEST_ENABLE
 *    motor_test_start();
 *    #endif
 * 
 * 2. 或直接在初始化完成后调用（临时测试）：
 *    motor_test_start();
 */
void motor_test_start(void);

#ifdef __cplusplus
}
#endif

#endif // MOTOR_TEST_H
