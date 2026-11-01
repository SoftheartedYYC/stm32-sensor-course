/**
 * @file    buzzer_irq.h
 * @brief   蜂鸣器驱动（中断版）：类型、引脚、频率全在 buzzer_cfg.h 里配
 *
 * 三种蜂鸣器共用同一套接口，只改 buzzer_cfg.h 里的 BUZZER_TYPE 宏：
 *   有源低电平触发 / 有源高电平触发  -> 引脚给电平就响，定时器只负责"响多久"；
 *   无源（要方波）                  -> TIM2 每半个周期翻转一次引脚，出 2.7kHz 方波。
 *
 * 和 APP/buzzer 里的轮询版区别：
 *   轮询版要主循环不停调 BEEP_Poll()，发声期间 CPU 被占着；
 *   中断版由硬件定时器打拍子，主循环只调 BuzzerIRQ_Start() / BuzzerIRQ_Stop()，
 *   不占主循环，声音也是连续的。
 *
 * 硬件连接：BEEP -> PB10（可在 buzzer_cfg.h 改），推挽输出。
 */
#ifndef __BUZZER_IRQ_H
#define __BUZZER_IRQ_H

#include "stm32f10x.h"
#include "buzzer_cfg.h"

/* ------------------------------------------------------------------
 * 接线：换引脚改 buzzer_cfg.h；TIM2 只当时基，不做重映射
 * ------------------------------------------------------------------ */
#define BUZZER_IRQ_PORT      BUZZER_PORT
#define BUZZER_IRQ_PIN       BUZZER_PIN
#define BUZZER_IRQ_GPIO_RCC  BUZZER_GPIO_RCC
#define BUZZER_IRQ_TIM       TIM2
#define BUZZER_IRQ_TIM_RCC   RCC_APB1Periph_TIM2

/* 兼容旧名字，值同样来自 buzzer_cfg.h */
#define BUZZER_IRQ_FREQ_HZ   BUZZER_FREQ_HZ
#define BUZZER_IRQ_HALF_US   BUZZER_HALF_US

/* 定时器中断节拍（us）：
 *   无源：半个方波周期（2700Hz -> 185us），中断里翻转引脚，出方波；
 *   有源：1ms，只当计时器用——Beep 到点关电平，引脚本身不翻转 */
#if BUZZER_NEED_SQUARE
#define BUZZER_IRQ_TICK_US   BUZZER_HALF_US
#else
#define BUZZER_IRQ_TICK_US   1000u
#endif

/* 上电自检：1 = BuzzerIRQ_Init() 里响 200ms，确认接线和类型选对了 */
#define BUZZER_INIT_SELFTEST 1

/* 初始化：引脚推挽输出（先静音）+ TIM2 时基 + 更新中断 + NVIC，定时器先不启动 */
void BuzzerIRQ_Init(void);

/* 连续鸣叫：有源给电平；无源启动定时器一直翻转，直到调用 Stop */
void BuzzerIRQ_Start(void);

/* 停止鸣叫：关定时器并把引脚拉到静音电平（静音时不进中断，零开销） */
void BuzzerIRQ_Stop(void);

/* 响 ms 毫秒后自动停（非阻塞：函数立刻返回，靠中断数剩余节拍） */
void BuzzerIRQ_Beep(uint16_t ms);

/* 当前是否在响：1 = 响，0 = 静音 */
uint8_t BuzzerIRQ_IsOn(void);

/* 上电自检：响 200ms 后自动停（非阻塞） */
void BuzzerIRQ_SelfTest(void);

#endif /* __BUZZER_IRQ_H */
