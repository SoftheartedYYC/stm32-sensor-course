/**
 * @file    key_irq.h
 * @brief   四个独立按键（中断版）：PE0~PE3，EXTI 双边沿 + 1ms 时基
 *
 * 事件模型（每个键都会产生下面四种事件）：
 *   短按   KEY_EVENT_SHORT ：按下后在 LONG_MS 内松开，且 DOUBLE_MS 内没有第二次按下
 *   长按   KEY_EVENT_LONG  ：按住满 LONG_MS，立即报一次（不等松开）
 *   连发   KEY_EVENT_REPEAT：长按之后每 REPEAT_MS 报一次，方便快速加减阈值
 *   双击   KEY_EVENT_DOUBLE：两次短按间隔小于 DOUBLE_MS，第二次松开时报出
 *
 * 注意：因为要区分双击，**短按事件会等 DOUBLE_MS 才报**（默认 300ms）；
 *       长按在 500ms 时立刻报，不用等松开。
 *
 * 事件编码：低 4 位是键号，高 4 位是事件类型 —— 用 KEY_ID_x | KEY_EVENT_xxx 拼起来。
 */
#ifndef __KEY_IRQ_H
#define __KEY_IRQ_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 接线：四个键都在 PE 口，按下 = 低电平
 * ------------------------------------------------------------------ */
#define KEYIRQ_PORT   GPIOE
#define KEYIRQ_PIN0   GPIO_Pin_0
#define KEYIRQ_PIN1   GPIO_Pin_1
#define KEYIRQ_PIN2   GPIO_Pin_2
#define KEYIRQ_PIN3   GPIO_Pin_3

/* 时间参数（ms） */
#define KEYIRQ_DEBOUNCE_MS 20  /* 边沿后稳定这么久才认 */
#define KEYIRQ_LONG_MS     500 /* 按住满 500ms 算长按 */
#define KEYIRQ_DOUBLE_MS   300 /* 两次短按间隔小于 300ms 算双击 */
#define KEYIRQ_REPEAT_MS   100 /* 长按后每 100ms 连发一次 */

/* ------------------------------------------------------------------
 * 事件定义
 *   低 4 位 = 键号（1~4），高 4 位 = 事件类型
 *   例：KEY_ID_1 | KEY_EVENT_DOUBLE 表示"KEY1 双击"
 * ------------------------------------------------------------------ */
#define KEY_NONE         0x00
#define KEY_ID_0         1
#define KEY_ID_1         2
#define KEY_ID_2         3
#define KEY_ID_3         4

#define KEY_EVENT_SHORT  0x10 /* 短按（松开后等双击窗口到期才报） */
#define KEY_EVENT_LONG   0x20 /* 长按（按住满 500ms 时立刻报） */
#define KEY_EVENT_DOUBLE 0x40 /* 双击（第二次松开时报） */
#define KEY_EVENT_REPEAT 0x80 /* 长按后的连发 */

#define KEY_ID_MASK      0x0F /* 取键号：ev & KEY_ID_MASK */
#define KEY_EVENT_MASK   0xF0 /* 取事件：ev & KEY_EVENT_MASK */

/* 初始化：PE0~PE3 上拉输入 + 4 条 EXTI（双边沿）+ NVIC，并挂上 1ms 钩子 */
void KeyIRQ_Init(void);

/* 取一个按键事件；没有事件返回 KEY_NONE */
uint8_t KeyIRQ_Scan(void);

#endif /* __KEY_IRQ_H */
