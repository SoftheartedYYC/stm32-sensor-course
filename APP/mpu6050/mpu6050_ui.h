/**
 * @file    mpu6050_ui.h
 * @brief   MPU6050 的 OLED 显示（128x64，4 行 x 16 列）
 *
 * 只读 mpu6050_data_t 里的数据，不碰传感器；显示内容全在 mpu6050_ui.c 里。
 * 依赖 APP/oled（oled_app.h + oled_font.h），但不修改它：自己攒一屏显存，
 * 再用 oled_write_run() 整页刷进去，一屏只要 8 次 I2C 事务。
 */
#ifndef __MPU6050_UI_H
#define __MPU6050_UI_H

#include "stm32f10x.h"
#include "mpu6050.h"

/* 刷新一屏（4 行），前三行是角度，每行右边跟着该轴的 g 值：
 *   第 1 行：P  pitch    前后倾（绕 Y 轴），平放 0°、前倾正、后倾负，-90 ~ +90
 *   第 2 行：R  roll     左右倾（绕 X 轴），平放 0°、右倾正、左倾负，-180 ~ +180
 *   第 3 行：T  tilt     合成倾角（板子和水平面的夹角），平放 0°、立起来 90°、翻过来 180°
 *   第 4 行：TEMP        芯片温度
 *
 * 角度只用加速度算，静止时准，一晃就跟着抖（想要平稳就自己加互补滤波/卡尔曼）；
 * 角速度（gyro_dps）没上屏，要看得自己改 mpu6050_ui.c 里第 4 行。            */
void mpu6050_ui_show(const mpu6050_data_t *data);

/* 显示两行普通文字（第 2、3 行），比如初始化失败时的提示 */
void mpu6050_ui_message(const char *line1, const char *line2);

/* 清屏 */
void mpu6050_ui_clear(void);

#endif /* __MPU6050_UI_H */
