/**
 * @file    key_app.h
 * @brief   四个独立按键：KEY0=PE0、KEY1=PE1、KEY2=PE2、KEY3=PE3
 *
 * 接法：按键一端接 MCU 引脚，另一端接 GND；引脚配成内部上拉输入，
 *       松开读到高电平，按下读到低电平（最常见、最省外围器件的接法）。
 *
 * 用法：主循环每 10ms 调一次 key_scan()。它负责 50ms 消抖，
 *       只在"按下的那一刻"返回一次事件；按住超过 500ms 后每 100ms 连发，
 *       事件的 bit7 置 1（KEY_EVENT_REPEAT），方便长按快速加减阈值。
 *       返回 KEY_NONE(0) 表示这一拍没有新事件。
 */

#ifndef __KEY_APP_H
#define __KEY_APP_H

#include "stm32f10x.h" /* uint8_t / GPIO_Pin_x */

/* ------------------------------------------------------------------
 * 接线：换引脚只改这 4 行
 * ------------------------------------------------------------------ */
#define KEY_PORT GPIOE       /* 四个键都在 GPIOE */
#define KEY0_PIN GPIO_Pin_0  /* KEY0 = PE0：循环选择光敏阈值/温度阈值/湿度阈值/退出 */
#define KEY1_PIN GPIO_Pin_1  /* KEY1 = PE1：光敏 +5 / 温度 +1°C / 湿度 +5%RH */
#define KEY2_PIN GPIO_Pin_2  /* KEY2 = PE2：光敏 -5 / 温度 -1°C / 湿度 -5%RH */
#define KEY3_PIN GPIO_Pin_3  /* KEY3 = PE3：静音（停止本次报警） */

/* ------------------------------------------------------------------
 * 事件定义
 * ------------------------------------------------------------------ */
#define KEY_NONE 0          /* 没有按键事件 */
#define KEY_ID_0 1          /* 键号，低 4 位 */
#define KEY_ID_1 2
#define KEY_ID_2 3
#define KEY_ID_3 4
#define KEY_EVENT_REPEAT 0x80 /* 与键号或在一起：这是长按连发，不是第一次按下 */

/* 初始化 PE0~PE3 为上拉输入 */
void key_init(void);

/* 扫描按键，建议每 10ms 调用一次 */
uint8_t key_scan(void);

#endif /* __KEY_APP_H */
