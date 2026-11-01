/**
 * @file    buzzer_app.h
 * @brief   蜂鸣器驱动（轮询版）：类型、引脚、频率全在 buzzer_cfg.h 里配
 *
 * 三种蜂鸣器（有源低触发 / 有源高触发 / 无源要方波）对外接口一模一样，
 * 只需要改 buzzer_cfg.h 里的 BUZZER_TYPE 宏，本文件和业务代码都不用动：
 *
 *   BEEP_Init();                 // 初始化，上电静音
 *   BEEP_On();                   // 开始响
 *   while (...) { BEEP_Poll(); } // 无源：靠它出方波；有源：空函数，什么也不做
 *   BEEP_Off();                  // 静音
 *
 * 也可以用阻塞版的 BEEP_Beep(ms)，一句话响 ms 毫秒。
 */
#ifndef __BUZZER_APP_H
#define __BUZZER_APP_H

#include "stm32f10x.h"
#include "buzzer_cfg.h"

/* 兼容旧代码用的引脚 / 频率别名，真正生效的值在 buzzer_cfg.h */
#define BEEP_PORT        BUZZER_PORT
#define BEEP_PIN         BUZZER_PIN
#define BEEP_ON()        BUZZER_ON()
#define BEEP_OFF()       BUZZER_OFF()
#define BEEP_FREQ_HZ     BUZZER_FREQ_HZ
#define BEEP_HALF_US     BUZZER_HALF_US
#define BEEP_NEED_SQUARE BUZZER_NEED_SQUARE

/** @brief  蜂鸣器引脚初始化（推挽输出，初始静音）
 *  @note   静音电平按 BUZZER_TYPE 自动选：有源低触发/无源拉高，有源高触发拉低
 */
void BEEP_Init(void);

/** @brief  开始响
 *  @note   有源：直接把引脚拉到"响"的电平，立刻出声；
 *          无源：只置标志，还要在主循环里反复调 BEEP_Poll() 才会出方波
 */
void BEEP_On(void);

/** @brief  静音：引脚回到"静音"电平
 *  @note   无源蜂鸣器会立刻停在当前电平（拉高），不会一直通直流
 */
void BEEP_Off(void);

/** @brief  轮询：无源时翻转一次引脚并延时半个周期；有源时什么也不做
 *  @note   要在主循环里一直调（或者放在 1ms 定时中断里调），才有无源方波
 */
void BEEP_Poll(void);

/** @brief  输出半个周期的方波（兼容旧接口）
 *  @note   无源：翻转一次 + 延时半个周期，循环调用即持续鸣响；
 *          有源：等于 BEEP_On()，直接给电平，不需要方波
 */
void BEEP_Square(void);

/** @brief  阻塞式响 ms 毫秒，返回时已经静音
 *  @param  ms 发声时长，单位 ms
 *  @note   需要先 delay_init()；无源时靠循环翻转产生方波，期间 CPU 被占用，
 *          要非阻塞请用中断版的 BuzzerIRQ_Beep(ms)
 */
void BEEP_Beep(uint16_t ms);

#endif /* __BUZZER_APP_H */
