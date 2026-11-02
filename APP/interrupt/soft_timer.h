/**
 * @file    soft_timer.h
 * @brief   软定时器：在 TIM6 的 1ms 时基上，"按周期"执行多个任务
 *
 * 解决的问题：
 *   按键扫描要 1ms、矩阵扫描要 1ms、温度采样要 1s……如果每个功能各开一个定时器，
 *   定时器不够用、中断也散。这里只用 TIM6 一个 1ms 中断，在里面按各自的周期
 *   调用注册进来的函数——就是最朴素的"软定时器 / 协作式调度"：
 *   一个硬件中断，多个软件任务。
 *
 * 用法（注册要放在 TICK_Start() 之前，也就是各模块的 init 里）：
 *
 *   static soft_timer_t s_timer;
 *
 *   static void my_task(void) { ... }        // 必须短小，不能阻塞
 *
 *   SOFTTIMER_Add(&s_timer, my_task, 10);    // 每 10ms 跑一次
 *
 * 注意：
 *   1. 回调是在 TIM6 中断里执行的：只做读 IO、跑个小状态机这种短活；
 *      不能 delay、不能等 I2C/串口、不能 printf。
 *   2. 耗时的工作（例如 DHT11 读一次要 5ms）不要直接放回调里，
 *      回调只置一个标志，主循环看到标志再去做。
 *   3. 注册（SOFTTIMER_Add）不能在 TICK_Start() 之后、运行途中做，
 *      表项数量是固定的，中断遍历时不做加锁。
 */

#ifndef __SOFT_TIMER_H
#define __SOFT_TIMER_H

#include "stm32f10x.h"

#define SOFT_TIMER_MAX 8u   /* 最多同时跑 8 个软定时任务 */

/* 一个软定时器（由使用者定义变量，模块只保存指针） */
typedef struct
{
    void (*fn)(void);      /* 到期要调用的函数 */
    uint16_t period_ms;    /* 周期，单位 ms */
    uint16_t remain_ms;    /* 距离下次到期还有多少 ms */
    uint8_t running;       /* 1 = 正在计时 */
} soft_timer_t;

/* 清空软定时器表（上电时调一次，必须在各模块注册之前） */
void SOFTTIMER_Init(void);

/* 注册并立即启动一个周期任务：返回表项序号（>=0），-1 = 参数错或表满 */
int8_t SOFTTIMER_Add(soft_timer_t *timer, void (*fn)(void), uint16_t period_ms);

/* 重新开始计时（从满周期开始） */
void SOFTTIMER_Start(soft_timer_t *timer);

/* 暂停/继续：Stop 之后不再调用回调，调用 Start 恢复 */
void SOFTTIMER_Stop(soft_timer_t *timer);

/* 改周期（ms，最小 1），立刻按新周期重新计时 */
void SOFTTIMER_SetPeriod(soft_timer_t *timer, uint16_t period_ms);

/* 1 = 正在计时 */
uint8_t SOFTTIMER_IsRunning(const soft_timer_t *timer);

/* 由 TIM6 的 1ms 中断调用：把所有到期的任务跑一遍（tick_irq.c 已调用，应用不用管） */
void SOFTTIMER_Tick(void);

#endif /* __SOFT_TIMER_H */
