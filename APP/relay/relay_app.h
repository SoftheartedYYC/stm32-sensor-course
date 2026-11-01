/**
 * @file    relay_app.h
 * @brief   两路继电器驱动：IN1 = PC0，IN2 = PC1，高电平触发
 *
 * 接线：
 *   继电器模块 VCC -> 5V（按模块要求，常见是 5V）
 *   继电器模块 GND -> GND（必须和 STM32 共地，否则 IN 上的电平没有参考）
 *   IN1            -> PC0
 *   IN2            -> PC1
 *   输出端 COM/NO/NC 接被控设备；强电侧由模块的光耦/继电器隔离。
 *
 * 触发极性：
 *   本工程按"高电平触发"写：PC0/PC1 = 1 时继电器吸合，= 0 时释放。
 *   如果买到的是低电平触发模块，把 RELAY_ON_LEVEL 改成 0 即可，不用动其它代码。
 *
 * 安全：
 *   上电初始化时先把两路都写成"释放"电平，再切成推挽输出，
 *   避免初始化瞬间继电器"啪"地吸合一下。
 *   继电器输出侧可能接 220V 强电，接线和调试时务必断电操作。
 */

#ifndef __RELAY_APP_H
#define __RELAY_APP_H

#include "stm32f10x.h" /* uint8_t / GPIO_Pin_x */

/* ------------------------------------------------------------------
 * 接线：换引脚/换触发极性只改这 4 行
 * ------------------------------------------------------------------ */
#define RELAY_PORT      GPIOC        /* 两路继电器都在 GPIOC */
#define RELAY1_PIN      GPIO_Pin_0   /* IN1 = PC0 */
#define RELAY2_PIN      GPIO_Pin_1   /* IN2 = PC1 */
#define RELAY_ON_LEVEL  1            /* 1 = 高电平吸合（本工程）；0 = 低电平吸合 */

/* 通道号：relay_on(RELAY_CH1) 比 relay_on(1) 好读 */
#define RELAY_CH1 1
#define RELAY_CH2 2

/* 初始化 PC0/PC1 为推挽输出，并让两路都释放 */
void relay_init(void);

/* 指定通道吸合/释放：on 非 0 = 吸合，0 = 释放；ch 只认 RELAY_CH1 / RELAY_CH2 */
void relay_set(uint8_t ch, uint8_t on);
void relay_on(uint8_t ch);
void relay_off(uint8_t ch);

/* 翻转指定通道，用于手动/调试 */
void relay_toggle(uint8_t ch);

/* 两路一起吸合或释放 */
void relay_all(uint8_t on);

/* 读当前输出状态：1 = 吸合，0 = 释放（读的是输出锁存器，不是触点反馈） */
uint8_t relay_get(uint8_t ch);

#endif /* __RELAY_APP_H */
