/**
 * @file action_table.c
 * @brief Blinko动作表 - 自动生成
 * 
 * 此文件由 generate_action_table_from_txt.py 自动生成
 * 数据来源: Blinko动作组合.txt
 * 
 * 请勿手动修改此文件！
 */

#include "app/action_table.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "app/action_executor.h"
/* ==================== S区间动作 (亢奋) ==================== */

// static const action_combo_t action_s1 = {

//     .combo_id = 0x01,
//     .name = "被摸的时候连续点头",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 1,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 2000,  //1个半周750ms
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "被摸的时候连续点头"
// };

// static const action_combo_t action_s2 = { 
//     .combo_id = 0x02,
//     .name = "连续转头",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 2,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME, 

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 

//     .shake_execute_num =2,
//     .shake_priority = 1,
//     .duration_ms = 0,
//     .description = "连续转头"
// };

// static const action_combo_t action_s3 = {
//     .combo_id = 0x03,
//     .name = "转头后点头",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 3,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .nod_priority = 1,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration =  45*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .shake_priority = 2,
//     .duration_ms = 0,
//     .description = "转头后点头"
// };

// static const action_combo_t action_s4 = {
//     .combo_id = 0x04,
//     .name = "转头抬头",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 4,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 2000,
//     .nod_priority = 1,
//     .shake_direction = 1,
//     // .shake_speed = 100,
//     // .shake_duration =  45*QUICK_UNIT_ANGLE_TIME,
//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .shake_priority = 1,
//     .duration_ms = 0,
//     .description = "转头抬头"
// };

// static const action_combo_t action_s5 = {
//     .combo_id = 0x05,
//     .name = "低头后转头",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 1,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .nod_priority = 2,
//     .shake_direction = 1,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,
//     //.shake_duration = 30*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .shake_priority = 1,
//     .duration_ms = 0,
//     .description = "低头后转头"
// };

// static const action_combo_t action_s6 = {
//     .combo_id = 0x06,
//     .name = "左右快速转头+抖动",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 2,
//     .nod_direction = 1,
//     .nod_speed = 10,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // //.shake_duration = 30*QUICK_UNIT_ANGLE_TIME,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 

//     .shake_execute_num =3,
//     .duration_ms = 0,
//     .description = "左右快速转头+抖动"
// };

// static const action_combo_t action_s7 = {
//     .combo_id = 0x07,
//     .name = "转向一边",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 3,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 0,
//     .shake_direction = 1,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,
//     //.shake_duration = 30*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转向一边"
// };

// static const action_combo_t action_s8 = {
//     .combo_id = 0x08,
//     .name = "快速转头",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 4,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 

//     .duration_ms = 0,
//     .description = "快速转头"
// };

// static const action_combo_t action_s9 = {
//     .combo_id = 0x09,
//     .name = "只发声",
//     .led_pattern = 4,
//     .vib_pattern = 3,
//     .sound_effect = 5,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "只发声"
// };

/* ==================== A区间动作 (积极) ==================== */

// static const action_combo_t action_a1 = {
//     .combo_id = 0x0A,
//     .name = "转头点头",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 6,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转头点头"
// };

// static const action_combo_t action_a2 = {
//     .combo_id = 0x0B,
//     .name = "转头",
//     .led_pattern = 7,
//     .vib_pattern = 1,
//     .sound_effect = 7,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // //.shake_duration = 30*QUICK_UNIT_ANGLE_TIME,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 


//     .duration_ms = 0,
//     .description = "转头"
// };

// static const action_combo_t action_a3 = {
//     .combo_id = 0x0C,
//     .name = "被摸的时候连续微点头",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 8,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 2000,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "被摸的时候连续微点头"
// };

// static const action_combo_t action_a4 = {
//     .combo_id = 0x0D,
//     .name = "转向一边",
//     .led_pattern = 7,
//     .vib_pattern = 1,
//     .sound_effect = 9,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 2,
//     // .shake_speed = 100,
//     // .shake_duration = 30*QUICK_UNIT_ANGLE_TIME,


//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转向一边"
// };

// static const action_combo_t action_a5 = {
//     .combo_id = 0x0E,
//     .name = "点头转向一边",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 6,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 2000,
//     .shake_direction = 1,
//     // .shake_speed = 100,
//     // .shake_duration = 30*QUICK_UNIT_ANGLE_TIME,

//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "点头转向一边"
// };

// static const action_combo_t action_a6 = {
//     .combo_id = 0x0F,
//     .name = "抬头",
//     .led_pattern = 7,
//     .vib_pattern = 1,
//     .sound_effect = 10,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "抬头"
// };

// static const action_combo_t action_a7 = {
//     .combo_id = 0x10,
//     .name = "点头转向一边",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 7,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 2000,
//     .shake_direction = 1,
//     // .shake_speed = 100,
//     // .shake_duration = 30*QUICK_UNIT_ANGLE_TIME,


//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 

//     .duration_ms = 0,
//     .description = "点头转向一边"
// };

// static const action_combo_t action_a8 = {
//     .combo_id = 0x11,
//     .name = "只发声",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 8,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "只发声"
// };

// static const action_combo_t action_a9 = {
//     .combo_id = 0x12,
//     .name = "抬头震动",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 9,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "抬头震动"
// };

// /* ==================== B区间动作 (中性) ==================== */

// static const action_combo_t action_b1 = {
//     .combo_id = 0x13,
//     .name = "转头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 11,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,


//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转头"
// };

// static const action_combo_t action_b2 = {
//     .combo_id = 0x14,
//     .name = "点头",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 12,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "点头"
// };

// static const action_combo_t action_b3 = {
//     .combo_id = 0x15,
//     .name = "抬头震动",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 13,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "抬头震动"
// };

// static const action_combo_t action_b4 = {
//     .combo_id = 0x16,
//     .name = "抬头转向一边",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 11,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 1,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,


//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "抬头转向一边"
// };

// static const action_combo_t action_b5 = {
//     .combo_id = 0x17,
//     .name = "转头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 14,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
// //     .shake_speed = 100,
// //    // .shake_duration = 25*QUICK_UNIT_ANGLE_TIME,
// //     .shake_duration = 30*QUICK_UNIT_ANGLE_TIME,


//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转头"
// };

// static const action_combo_t action_b6 = {
//     .combo_id = 0x18,
//     .name = "点头",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 15,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 1500,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "点头"
// };

// static const action_combo_t action_b7 = {
//     .combo_id = 0x19,
//     .name = "转头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 11,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration = 30*QUICK_UNIT_ANGLE_TIME,


//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转头"
// };

// static const action_combo_t action_b8 = {
//     .combo_id = 0x1A,
//     .name = "转头点头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 13,
//     .nod_direction = 1,
//     .nod_speed = 100,
//     .nod_duration = 2000,
//     .shake_direction = 4,
//     // .shake_speed = 100,
//     // .shake_duration = 45*QUICK_UNIT_ANGLE_TIME,



//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME, 
//     .duration_ms = 0,
//     .description = "转头点头"
// };

// static const action_combo_t action_b9 = {
//     .combo_id = 0x1B,
//     .name = "只发声",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 12,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "只发声"
// };

/* ==================== C区间动作 (平静) ==================== */

// static const action_combo_t action_c1 = {
//     .combo_id = 0x1C,
//     .name = "点头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 16,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "点头"
// };

// static const action_combo_t action_c2 = {
//     .combo_id = 0x1D,
//     .name = "慢转头一次",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 17,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 1,
//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME,
//     .duration_ms = 0,
//     .description = "慢转头一次"
// };

// static const action_combo_t action_c3 = {
//     .combo_id = 0x1E,
//     .name = "抬头转头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 19,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 3000,
//     .shake_direction = 4,
//     .shake_speed = 70,
//     .shake_duration = 30*MID_UNIT_ANGLE_TIME,
//     .duration_ms = 0,
//     .description = "抬头转头"
// };

// static const action_combo_t action_c4 = {
//     .combo_id = 0x1F,
//     .name = "摇一下头保持姿态",
//     .led_pattern = 7,
//     .vib_pattern = 1,
//     .sound_effect = 17,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 4,
//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME,
//     .duration_ms = 0,
//     .description = "摇一下头保持姿态"
// };

// static const action_combo_t action_c5 = {
//     .combo_id = 0x20,
//     .name = "点头后低头",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 16,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "点头后低头"
// };

// static const action_combo_t action_c6 = {
//     .combo_id = 0x21,
//     .name = "只发声+震动",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 18,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "只发声+震动"
// };

// static const action_combo_t action_c7 = {
//     .combo_id = 0x22,
//     .name = "转头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 16,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 1,
//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME,
//     .duration_ms = 0,
//     .description = "转头"
// };

// static const action_combo_t action_c8 = {
//     .combo_id = 0x23,
//     .name = "转头点头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 20,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2500,
//     .shake_direction = 4,
//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME,
//     .duration_ms = 0,
//     .description = "转头点头"
// };

// static const action_combo_t action_c9 = {
//     .combo_id = 0x24,
//     .name = "抬头",
//     .led_pattern = 4,
//     .vib_pattern = 2,
//     .sound_effect = 18,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "抬头"
// };

// /* ==================== D区间动作 (消极) ==================== */

// static const action_combo_t action_d1 = {
//     .combo_id = 0x25,
//     .name = "只发声",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 20,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "只发声"
// };

// static const action_combo_t action_d2 = {
//     .combo_id = 0x26,
//     .name = "被碰后缓慢点头",
//     .led_pattern = 7,
//     .vib_pattern = 2,
//     .sound_effect = 19,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .shake_direction = 1,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "被碰后缓慢点头"
// };

// static const action_combo_t action_d3 = {
//     .combo_id = 0x27,
//     .name = "微抖一下+低头",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 21,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .nod_priority = 1,
//     .shake_priority = 2,
//     .shake_direction = 4,
//     .shake_speed = 70,
//     .shake_duration = 45*MID_UNIT_ANGLE_TIME,
//     .duration_ms = 0,
//     .description = "微抖一下+低头"
// };

// static const action_combo_t action_d4 = {
//     .combo_id = 0x28,
//     .name = "只发声",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 22,
//     .nod_direction = 0,
//     .nod_speed = 0,
//     .nod_duration = 0,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "只发声"
// };

// static const action_combo_t action_d5 = {
//     .combo_id = 0x29,
//     .name = "缓慢点头",
//     .led_pattern = 4,
//     .vib_pattern = 1,
//     .sound_effect = 19,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "缓慢点头"
// };

// static const action_combo_t action_d6 = {
//     .combo_id = 0x2A,
//     .name = "缓慢抬头",
//     .led_pattern = 7,
//     .vib_pattern = 1,
//     .sound_effect = 22,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 2000,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "缓慢抬头"
// };

// static const action_combo_t action_d7 = {
//     .combo_id = 0x2B,
//     .name = "低头静止",
//     .led_pattern = 7,
//     .vib_pattern = 0,
//     .sound_effect = 0,
//     .nod_direction = 1,
//     .nod_speed = 50,
//     .nod_duration = 500,
//     .shake_direction = 0,
//     .shake_speed = 0,
//     .shake_duration = 0,
//     .duration_ms = 0,
//     .description = "低头静止"
// };


//-------------------------------------------------
//servo motor table 
//-------------------------------------------------
#if SUPPORT_270_SERVO_MOTOR

static const action_combo_t action_s1 = {

    .combo_id = 0x01,
    .name = "被摸的时候连续点头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 1,
    .nod_direction = ANGLE_30_BOTH,
    .nod_execute_num = 2,
    .description = "被摸的时候连续点头"
};

static const action_combo_t action_s2 = { 
    .combo_id = 0x02,
    .name = "连续转头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 2,
    .shake_direction = ANGLE_45_BOTH,
    .shake_execute_num =2,
    .shake_priority = 1,
    .duration_ms = 0,
    .description = "连续转头"
};

static const action_combo_t action_s3 = {
    .combo_id = 0x03,
    .name = "转头后点头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 3,
    .nod_direction = ANGLE_30_BOTH,
    .nod_priority = 1,
    .shake_direction = ANGLE_45_BOTH,
    .shake_priority = 2,
    .duration_ms = 0,
    .description = "转头后点头"
};

static const action_combo_t action_s4 = {
    .combo_id = 0x04,
    .name = "转头抬头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 4,
    .nod_direction = ANGLE_30_RIGHT,
    .nod_priority = 1,
    .shake_direction = ANGLE_45_RIGHT,
    .shake_priority = 1,
    .duration_ms = 0,
    .description = "转头抬头"
};

static const action_combo_t action_s5 = {
    .combo_id = 0x05,
    .name = "低头后转头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 1,
    .nod_direction = ANGLE_20_LEFT,
    .nod_priority = 2,
    .shake_direction = ANGLE_45_RIGHT,
    .shake_priority = 1,
    .duration_ms = 0,
    .description = "低头后转头"
};

static const action_combo_t action_s6 = {
    .combo_id = 0x06,
    .name = "左右快速转头+抖动",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 2,
    .shake_direction = ANGLE_30_BOTH,
    .shake_execute_num =3,
    .duration_ms = 0,
    .description = "左右快速转头+抖动"
};

static const action_combo_t action_s7 = {
    .combo_id = 0x07,
    .name = "转向一边",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 3,
    .shake_direction = ANGLE_45_RIGHT,
    .duration_ms = 0,
    .description = "转向一边"
};

static const action_combo_t action_s8 = {
    .combo_id = 0x08,
    .name = "快速转头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 4,
    .shake_direction = ANGLE_45_BOTH,
    .duration_ms = 0,
    .description = "快速转头"
};

static const action_combo_t action_s9 = {
    .combo_id = 0x09,
    .name = "只发声",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 5,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = 0,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .description = "只发声"
};

/* ==================== A区间动作 (积极) ==================== */

static const action_combo_t action_a1 = {
    .combo_id = 0x0A,
    .name = "转头点头",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 6,
    .nod_direction = ANGLE_30_BOTH,
    .shake_direction = ANGLE_30_BOTH,
    .description = "转头点头"
};

static const action_combo_t action_a2 = {
    .combo_id = 0x0B,
    .name = "转头",
    .led_pattern = 7,
    .vib_pattern = 1,
    .sound_effect = 7,
    .shake_direction = ANGLE_30_BOTH,
    .duration_ms = 0,
    .description = "转头"
};

static const action_combo_t action_a3 = {
    .combo_id = 0x0C,
    .name = "被摸的时候连续微点头",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 8,
    .nod_direction = ANGLE_20_BOTH,
    .nod_execute_num = 2,
    .duration_ms = 0,
    .description = "被摸的时候连续微点头"
};

static const action_combo_t action_a4 = {
    .combo_id = 0x0D,
    .name = "转向一边",
    .led_pattern = 7,
    .vib_pattern = 1,
    .sound_effect = 9,
    .shake_direction = ANGLE_30_LEFT,
    .duration_ms = 0,
    .description = "转向一边"
};

static const action_combo_t action_a5 = {
    .combo_id = 0x0E,
    .name = "点头转向一边",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 6,
    .nod_direction = ANGLE_20_BOTH,
    .shake_direction = ANGLE_30_RIGHT,
    .duration_ms = 0,
    .description = "点头转向一边"
};

static const action_combo_t action_a6 = {
    .combo_id = 0x0F,
    .name = "抬头",
    .led_pattern = 7,
    .vib_pattern = 1,
    .sound_effect = 10,
    .nod_direction = ANGLE_30_RIGHT,
    .duration_ms = 0,
    .description = "抬头"
};

static const action_combo_t action_a7 = {
    .combo_id = 0x10,
    .name = "点头转向一边",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 7,
    .nod_direction = ANGLE_20_BOTH,
    .shake_direction = ANGLE_30_RIGHT,
    .duration_ms = 0,
    .description = "点头转向一边"
};

static const action_combo_t action_a8 = {
    .combo_id = 0x11,
    .name = "只发声",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 8,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = 0,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .description = "只发声"
};

static const action_combo_t action_a9 = {
    .combo_id = 0x12,
    .name = "抬头震动",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 9,
    .nod_direction = ANGLE_30_RIGHT,
    .duration_ms = 0,
    .description = "抬头震动"
};

/* ==================== B区间动作 (中性) ==================== */

static const action_combo_t action_b1 = {
    .combo_id = 0x13,
    .name = "转头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 11,
    .shake_direction = ANGLE_45_BOTH,
    .duration_ms = 0,
    .description = "转头"
};

static const action_combo_t action_b2 = {
    .combo_id = 0x14,
    .name = "点头",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 12,
    .nod_direction = ANGLE_20_BOTH,
    .duration_ms = 0,
    .description = "点头"
};

static const action_combo_t action_b3 = {
    .combo_id = 0x15,
    .name = "抬头震动",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 13,
    .nod_direction = ANGLE_30_RIGHT,
    .duration_ms = 0,
    .description = "抬头震动"
};

static const action_combo_t action_b4 = {
    .combo_id = 0x16,
    .name = "抬头转向一边",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 11,
    .nod_direction = ANGLE_30_RIGHT,
    .shake_direction = ANGLE_45_RIGHT,
    .duration_ms = 0,
    .description = "抬头转向一边"
};

static const action_combo_t action_b5 = {
    .combo_id = 0x17,
    .name = "转头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 14,
    .shake_direction = ANGLE_30_BOTH,
    .duration_ms = 0,
    .description = "转头"
};

static const action_combo_t action_b6 = {
    .combo_id = 0x18,
    .name = "点头",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 15,
    .nod_direction = ANGLE_20_BOTH,
    .duration_ms = 0,
    .description = "点头"
};

static const action_combo_t action_b7 = {
    .combo_id = 0x19,
    .name = "转头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 14,
    .shake_direction = ANGLE_30_BOTH,
    .duration_ms = 0,
    .description = "转头"
};

static const action_combo_t action_b8 = {
    .combo_id = 0x1A,
    .name = "转头点头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 13,
    .nod_direction = ANGLE_20_BOTH,
    .shake_direction = ANGLE_60_BOTH,
    .duration_ms = 0,
    .description = "转头点头"
};

static const action_combo_t action_b9 = {
    .combo_id = 0x1B,
    .name = "只发声",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 12,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = 0,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .description = "只发声"
};

/* ==================== C区间动作 (平静) ==================== */

static const action_combo_t action_c1 = {
    .combo_id = 0x1C,
    .name = "点头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 16,
    .nod_direction = ANGLE_20_BOTH,
    .duration_ms = 0,
    .description = "点头"
};

static const action_combo_t action_c2 = {
    .combo_id = 0x1D,
    .name = "慢转头一次",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 17,
    .shake_direction = ANGLE_60_RIGHT,
    .duration_ms = 0,
    .description = "慢转头一次"
};

static const action_combo_t action_c3 = {
    .combo_id = 0x1E,
    .name = "抬头转头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 19,
    .nod_direction = ANGLE_15_RIGHT,
    .shake_direction = ANGLE_15_BOTH,
    .duration_ms = 0,
    .description = "抬头转头"
};

static const action_combo_t action_c4 = {
    .combo_id = 0x1F,
    .name = "摇一下头保持姿态",
    .led_pattern = 7,
    .vib_pattern = 1,
    .sound_effect = 17,
    .shake_direction = ANGLE_10_BOTH,
    .duration_ms = 0,
    .description = "摇一下头保持姿态"
};

static const action_combo_t action_c5 = {
    .combo_id = 0x20,
    .name = "点头后低头",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 16,
    .nod_direction = ANGLE_20_BOTH,
    .duration_ms = 0,
    .description = "点头后低头"
};

static const action_combo_t action_c6 = {
    .combo_id = 0x21,
    .name = "只发声+震动",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 18,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = 0,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .description = "只发声+震动"
};

static const action_combo_t action_c7 = {
    .combo_id = 0x22,
    .name = "转头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 16,
    .shake_direction = ANGLE_45_RIGHT,
    .duration_ms = 0,
    .description = "转头"
};

static const action_combo_t action_c8 = {
    .combo_id = 0x23,
    .name = "转头点头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 20,
    .nod_direction = ANGLE_20_BOTH,
    .shake_direction = ANGLE_45_BOTH,
    .duration_ms = 0,
    .description = "转头点头"
};

static const action_combo_t action_c9 = {
    .combo_id = 0x24,
    .name = "抬头",
    .led_pattern = 4,
    .vib_pattern = 2,
    .sound_effect = 18,
    .nod_direction = ANGLE_30_RIGHT,
    .duration_ms = 0,
    .description = "抬头"
};

/* ==================== D区间动作 (消极) ==================== */

static const action_combo_t action_d1 = {
    .combo_id = 0x25,
    .name = "只发声",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 20,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = 0,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .description = "只发声"
};

static const action_combo_t action_d2 = {
    .combo_id = 0x26,
    .name = "被碰后缓慢点头",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 19,
    .nod_direction = ANGLE_15_BOTH,
    .duration_ms = 0,
    .description = "被碰后缓慢点头"
};

static const action_combo_t action_d3 = {
    .combo_id = 0x27,
    .name = "微抖一下+低头",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 21,
    .nod_direction = ANGLE_10_LEFT,
    .nod_priority = 1,
    .shake_priority = 2,
    .shake_direction = ANGLE_10_LEFT,
    .duration_ms = 0,
    .description = "微抖一下+低头"
};

static const action_combo_t action_d4 = {
    .combo_id = 0x28,
    .name = "只发声",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 22,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = 0,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .description = "只发声"
};

static const action_combo_t action_d5 = {
    .combo_id = 0x29,
    .name = "缓慢点头",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 19,
    .nod_direction = ANGLE_15_BOTH,
    .duration_ms = 0,
    .description = "缓慢点头"
};

static const action_combo_t action_d6 = {
    .combo_id = 0x2A,
    .name = "缓慢抬头",
    .led_pattern = 7,
    .vib_pattern = 1,
    .sound_effect = 22,
    .nod_direction = ANGLE_20_RIGHT,
    .duration_ms = 0,
    .description = "缓慢抬头"
};

static const action_combo_t action_d7 = {
    .combo_id = 0x2B,
    .name = "低头静止",
    .led_pattern = 7,
    .vib_pattern = 0,
    .sound_effect = 0,
    .duration_ms = 0,
    .description = "低头静止"
};



/* ==================== 动作查找表 ==================== */

static const action_combo_t* s_zone_actions[] = {
    &action_s1,
    &action_s2,
    &action_s3,
    &action_s4,
    &action_s5,
    &action_s6,
    &action_s7,
    &action_s8,
    &action_s9,
};

static const action_combo_t* a_zone_actions[] = {
    &action_a1,
    &action_a2,
    &action_a3,
    &action_a4,
    &action_a5,
    &action_a6,
    &action_a7,
    &action_a8,
    &action_a9,
};

static const action_combo_t* b_zone_actions[] = {
    &action_b1,
    &action_b2,
    &action_b3,
    &action_b4,
    &action_b5,
    &action_b6,
    &action_b7,
    &action_b8,
    &action_b9,
};

static const action_combo_t* c_zone_actions[] = {
    &action_c1,
    &action_c2,
    &action_c3,
    &action_c4,
    &action_c5,
    &action_c6,
    &action_c7,
    &action_c8,
    &action_c9,
};

static const action_combo_t* d_zone_actions[] = {
    &action_d1,
    &action_d2,
    &action_d3,
    &action_d4,
    &action_d5,
    &action_d6,
    &action_d7,
};


/* ==================== 公共API ==================== */

const action_combo_t* action_table_get_combo_by_id(uint8_t id)
{
    // S区间: 0x01-0x09
    if (id >= 0x01 && id <= 0x09) {
        return s_zone_actions[id - 0x01];
    }
    // A区间: 0x0A-0x12
    if (id >= 0x0A && id <= 0x12) {
        return a_zone_actions[id - 0x0A];
    }
    // B区间: 0x13-0x1B
    if (id >= 0x13 && id <= 0x1B) {
        return b_zone_actions[id - 0x13];
    }
    // C区间: 0x1C-0x24
    if (id >= 0x1C && id <= 0x24) {
        return c_zone_actions[id - 0x1C];
    }
    // D区间: 0x25-0x2B
    if (id >= 0x25 && id <= 0x2B) {
        return d_zone_actions[id - 0x25];
    }
    return NULL;
}

uint8_t action_table_get_total_count(void)
{
    return 43;
}

esp_err_t action_table_init(void)
{
    // 动作表为静态数据，无需初始化
    return ESP_OK;
}

uint8_t action_table_get_combo_count(emotion_zone_t zone)
{
    switch (zone) {
        case EMOTION_ZONE_S:
            return 9;
        case EMOTION_ZONE_A:
            return 9;
        case EMOTION_ZONE_B:
            return 9;
        case EMOTION_ZONE_C:
            return 9;
        case EMOTION_ZONE_D:
            return 7;
        default:
            return 0;
    }
}

const action_combo_t* action_table_get_random_combo(emotion_zone_t zone, trigger_condition_t condition)
{
    uint8_t count = action_table_get_combo_count(zone);
    
    if (count == 0) {
        return NULL;
    }
    
    // 简单随机选择
    uint8_t random_index = rand() % count;
    
    // 根据区间返回对应的动作
    const action_combo_t* const* zone_actions = NULL;
    switch (zone) {
        case EMOTION_ZONE_S:
            zone_actions = s_zone_actions;
            break;
        case EMOTION_ZONE_A:
            zone_actions = a_zone_actions;
            break;
        case EMOTION_ZONE_B:
            zone_actions = b_zone_actions;
            break;
        case EMOTION_ZONE_C:
            zone_actions = c_zone_actions;
            break;
        case EMOTION_ZONE_D:
            zone_actions = d_zone_actions;
            break;
        default:
            return NULL;
    }
    
    return zone_actions[random_index];
}

//--------------------------------------------------------------------------------------------------------------------------
#elif SUPPORT_90_SERVO_MOTOR
//--------------------------------------------------------------------------------------------------------------------------

static const action_combo_t action_s1 = {

    .combo_id = 0x01,
    .name = "被摸的时候连续点头",
    .led_pattern = 1,
    .vib_pattern = 5,
    .sound_effect = 1,
    .nod_direction = ANGLE_25_BOTH,
    .nod_execute_num = 2,
    .ear_left_direction = ANGLE_20_BOTH,
    .ear_right_direction = ANGLE_20_BOTH,
    .description = "被摸的时候连续点头"
};

static const action_combo_t action_s2 = { 
    .combo_id = 0x02,
    .name = "连续转头",
    .led_pattern = 1,
    .vib_pattern = 5,
    .sound_effect = 2,
    .shake_direction = ANGLE_10_BOTH,
    .shake_execute_num =3,
    .shake_priority = 1,
    .duration_ms = 0,
    .ear_left_direction = ANGLE_20_BOTH,
    .ear_left_priority = 2,
    .ear_right_direction = ANGLE_20_BOTH,
    .ear_right_priority = 1,
    .description = "连续转头"
};

static const action_combo_t action_s3 = {
    .combo_id = 0x03,
    .name = "转头后点头",
    .led_pattern = 1,
    .vib_pattern = 5,
    .sound_effect = 3,
    .nod_direction = ANGLE_10_BOTH,
    .nod_priority = 1,
    .shake_direction = ANGLE_25_BOTH,
    .shake_priority = 2,
    .ear_left_direction = ANGLE_10_BOTH,
    .ear_left_repeat_num = 3,
    .ear_right_direction = ANGLE_10_BOTH,
    .ear_right_repeat_num = 3,
    .duration_ms = 0,
    .description = "转头后点头"
};

/* ==================== A区间动作 (积极) ==================== */

static const action_combo_t action_a1 = {
    .combo_id = 0x0A,
    .name = "转头点头",
    .led_pattern = 7,
    .vib_pattern = 4,
    .sound_effect = 6,
    .nod_direction = ANGLE_15_BOTH,
    .shake_direction = ANGLE_15_BOTH,
    .shake_execute_num = 2,
    .nod_execute_num = 2,
    .ear_left_direction = ANGLE_10_BOTH,
    .ear_right_direction = ANGLE_10_BOTH,
    .description = "转头点头"
};

static const action_combo_t action_a2 = {
    .combo_id = 0x0B,
    .name = "转头",
    .led_pattern = 7,
    .vib_pattern = 3,
    .sound_effect = 7,
    .shake_direction = ANGLE_5_BOTH,
    .shake_execute_num = 3,
    .ear_left_direction = ANGLE_10_BOTH,
    .ear_left_priority = 2,
    .ear_right_direction = ANGLE_10_BOTH,
    .ear_right_priority = 1,
    .duration_ms = 0,
    .description = "转头"
};

static const action_combo_t action_a3 = {
    .combo_id = 0x0C,
    .name = "被摸的时候连续微点头",
    .led_pattern = 7,
    .vib_pattern = 4,
    .sound_effect = 8,
    .nod_direction = ANGLE_5_BOTH,
    .duration_ms = 0,
    .ear_left_direction = ANGLE_5_BOTH,
    .ear_left_repeat_num = 3,
    .ear_right_direction = ANGLE_5_BOTH,
    .ear_right_repeat_num = 3,
    .description = "被摸的时候连续微点头"
};

/* ==================== B区间动作 (中性) ==================== */

static const action_combo_t action_b1 = {
    .combo_id = 0x13,
    .name = "转头",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 11,
    .shake_direction = ANGLE_15_BOTH,
    .duration_ms = 0,
    .ear_left_direction = ANGLE_5_BOTH,
    .ear_right_direction = ANGLE_5_BOTH,
    .description = "转头"
};

static const action_combo_t action_b2 = {
    .combo_id = 0x14,
    .name = "点头",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 12,
    .nod_direction = ANGLE_5_BOTH,
    .nod_execute_num = 2,
    .ear_left_direction = ANGLE_10_BOTH,
    .ear_left_priority = 2,
    .ear_right_direction = ANGLE_10_BOTH,
    .ear_right_priority = 1,
    .duration_ms = 0,
    .description = "点头"
};

static const action_combo_t action_b3 = {
    .combo_id = 0x15,
    .name = "抬头震动",
    .led_pattern = 4,
    .vib_pattern = 3,
    .sound_effect = 13,
    .nod_direction = ANGLE_5_RIGHT,
    .duration_ms = 0,
    .ear_left_direction = ANGLE_10_BOTH,
    .ear_left_repeat_num = 2,
    .description = "抬头震动"
};

static const action_combo_t action_b4 = {
    .combo_id = 0x16,
    .name = "抬头转向一边",
    .led_pattern = 4,
    .vib_pattern = 1,
    .sound_effect = 11,
    .nod_direction = ANGLE_5_RIGHT,
    .shake_direction = ANGLE_10_RIGHT,
    .duration_ms = 0,
    .ear_right_direction = ANGLE_10_BOTH,
    .ear_right_repeat_num = 2,
    .description = "抬头转向一边"
};

/* ==================== C区间动作 (平静) ==================== */

static const action_combo_t action_c1 = {
    .combo_id = 0x1C,
    .name = "点头",
    .led_pattern = 3,
    .vib_pattern = 1,
    .sound_effect = 16,
    .nod_direction = ANGLE_5_BOTH,
    .nod_execute_num = 2,
    .ear_left_direction = ANGLE_10_BOTH,
    .duration_ms = 0,
    .description = "点头"
};

static const action_combo_t action_c2 = {
    .combo_id = 0x1D,
    .name = "慢转头一次",
    .led_pattern = 3,
    .vib_pattern = 1,
    .sound_effect = 17,
    .shake_direction = ANGLE_5_BOTH,
    .ear_left_direction = ANGLE_10_BOTH,
    .duration_ms = 0,
    .description = "慢转头一次"
};

static const action_combo_t action_c3 = {
    .combo_id = 0x1E,
    .name = "抬头转头",
    .led_pattern = 3,
    .vib_pattern = 1,
    .sound_effect = 19,
    .nod_direction = ANGLE_2_BOTH,
    .shake_direction = ANGLE_5_BOTH,
    .duration_ms = 0,
    .description = "抬头转头"
};


/* ==================== D区间动作 (消极) ==================== */

static const action_combo_t action_d1 = {
    .combo_id = 0x25,
    .name = "只发声",
    .led_pattern = 3,
    .vib_pattern = 1,
    .sound_effect = 20,
    .nod_direction = 0,
    .nod_speed = 0,
    .nod_duration = 0,
    .shake_direction = ANGLE_5_BOTH,
    .shake_speed = 0,
    .shake_duration = 0,
    .duration_ms = 0,
    .ear_left_direction = ANGLE_5_BOTH,
    .description = "只发声"
};

static const action_combo_t action_d2 = {
    .combo_id = 0x26,
    .name = "被碰后缓慢点头",
    .led_pattern = 7,
    .vib_pattern = 2,
    .sound_effect = 19,
    .nod_direction = ANGLE_2_BOTH,
    .ear_right_direction = ANGLE_5_BOTH,
    .duration_ms = 0,
    .description = "被碰后缓慢点头"
};

static const action_combo_t action_d3 = {
    .combo_id = 0x27,
    .name = "微抖一下+低头",
    .led_pattern = 3,
    .vib_pattern = 1,
    .sound_effect = 21,
    .nod_direction = ANGLE_2_LEFT,
    .nod_priority = 1,
    .shake_priority = 2,
    .shake_direction = ANGLE_2_LEFT,
    .duration_ms = 0,
    .description = "微抖一下+低头"
};


/* ==================== 动作查找表 ==================== */

static const action_combo_t* s_zone_actions[] = {
    &action_s1,
    &action_s2,
    &action_s3,
};

static const action_combo_t* a_zone_actions[] = {
    &action_a1,
    &action_a2,
    &action_a3,
};

static const action_combo_t* b_zone_actions[] = {
    &action_b1,
    &action_b2,
    &action_b3,
    &action_b4,
};

static const action_combo_t* c_zone_actions[] = {
    &action_c1,
    &action_c2,
    &action_c3,
};

static const action_combo_t* d_zone_actions[] = {
    &action_d1,
    &action_d2,
    &action_d3,
};


/* ==================== 公共API ==================== */

const action_combo_t* action_table_get_combo_by_id(uint8_t id)
{
    // S区间: 0x01-0x03
    if (id >= 0x01 && id <= 0x03) {
        return s_zone_actions[id - 0x01];
    }
    // A区间: 0x04-0x6
    if (id >= 0x04 && id <= 0x6) {
        return a_zone_actions[id - 0x04];
    }
    // B区间: 0x7-0xA
    if (id >= 0x7 && id <= 0xA) {
        return b_zone_actions[id - 0x7];
    }
    // C区间: 0xB-0xD
    if (id >= 0xB && id <= 0xD) {
        return c_zone_actions[id - 0xB];
    }
    // D区间: 0xe-0x10
    if (id >= 0xe && id <= 0x10) {
        return d_zone_actions[id - 0xe];
    }
    return NULL;
}

uint8_t action_table_get_total_count(void)
{
    return 16;
}

esp_err_t action_table_init(void)
{
    // 动作表为静态数据，无需初始化
    return ESP_OK;
}

uint8_t action_table_get_combo_count(emotion_zone_t zone)
{
    switch (zone) {
        case EMOTION_ZONE_S:
            return 3;
        case EMOTION_ZONE_A:
            return 3;
        case EMOTION_ZONE_B:
            return 4;
        case EMOTION_ZONE_C:
            return 3;
        case EMOTION_ZONE_D:
            return 3;
        default:
            return 0;
    }
}

const action_combo_t* action_table_get_random_combo(emotion_zone_t zone, trigger_condition_t condition)
{
    uint8_t count = action_table_get_combo_count(zone);
    
    if (count == 0) {
        return NULL;
    }
    
    // 简单随机选择
    uint8_t random_index = rand() % count;
    
    // 根据区间返回对应的动作
    const action_combo_t* const* zone_actions = NULL;
    switch (zone) {
        case EMOTION_ZONE_S:
            zone_actions = s_zone_actions;
            break;
        case EMOTION_ZONE_A:
            zone_actions = a_zone_actions;
            break;
        case EMOTION_ZONE_B:
            zone_actions = b_zone_actions;
            break;
        case EMOTION_ZONE_C:
            zone_actions = c_zone_actions;
            break;
        case EMOTION_ZONE_D:
            zone_actions = d_zone_actions;
            break;
        default:
            return NULL;
    }
    
    return zone_actions[random_index];
}

#endif/*SUPPORT_270_SERVO_MOTOR SUPPORT_90_SERVO_MOTOR*/