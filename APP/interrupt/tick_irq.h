/**
 * @file    tick_irq.h
 * @brief   1ms 时基（TIM6 更新中断），给按键、矩阵键盘等模块提供统一心跳
 *
 * 为什么要有这个模块：
 *   按键消抖、长按连发、矩阵键盘扫描都需要一个稳定的 1ms 节拍。
 *   如果每个模块各开一个定时器，工程里会到处都是定时器中断；
 *   这里用空闲的 TIM6 统一产生 1ms 中断，别的模块通过 TICK_AddHook()
 *   挂一个"每毫秒执行一次"的函数，共用同一个时基。
 *
 * 用法（顺序不能反）：
 *   TICK_Init();        // 配 TIM6，但先不启动
 *   KeyIRQ_Init();      // 里面 TICK_AddHook(按键状态机)
 *   MatrixIRQ_Init();   // 里面 TICK_AddHook(矩阵扫描)
 *   TICK_Start();       // 钩子都挂好了，再启动 1ms 心跳
 */
#ifndef __TICK_IRQ_H
#define __TICK_IRQ_H

#include "stm32f10x.h"

#define TICK_TIM       TIM6
#define TICK_TIM_RCC   RCC_APB1Periph_TIM6
#define TICK_MAX_HOOKS 4   /* 最多挂 4 个 1ms 钩子 */

/* 初始化 TIM6：72MHz/(71+1) = 1MHz，ARR=999 -> 1ms 一次更新中断；定时器先不启动 */
void TICK_Init(void);

/* 启动 1ms 心跳（钩子都挂好之后再调） */
void TICK_Start(void);

/* 开机以来经过的毫秒数（在中断里自增，32 位，溢出用减法比较照样正确） */
uint32_t TICK_GetMs(void);

/* 挂一个每 1ms 调用一次的函数：返回 1 = 成功，0 = 钩子已满 */
uint8_t TICK_AddHook(void (*fn)(void));

#endif /* __TICK_IRQ_H */
