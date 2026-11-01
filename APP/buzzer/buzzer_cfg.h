/**
 * @file    buzzer_cfg.h
 * @brief   蜂鸣器配置（整个工程只需要改这个文件）
 *
 * 手头的蜂鸣器不一样，响的方式也不一样。本工程把它们统一成三种"类型"，
 * 只改下面 BUZZER_TYPE 这一行，轮询版（APP/buzzer）和中断版（APP/interrupt）
 * 都会自动按对应方式工作，其它代码一行都不用动：
 *
 *   BUZZER_TYPE_ACTIVE_LOW   有源蜂鸣器模块，低电平触发 -> 引脚给低电平就响（最常见）
 *   BUZZER_TYPE_ACTIVE_HIGH  有源蜂鸣器模块，高电平触发 -> 引脚给高电平才响
 *   BUZZER_TYPE_PASSIVE      无源蜂鸣器（裸蜂鸣器 / 没有驱动板）-> 必须给方波才响
 *
 * 怎么判断手上是哪种：
 *   1) 模块背面丝印写着"低电平触发 / 高电平触发"，或者有三个引脚（VCC/GND/IN）：
 *      有源，按丝印在前两个里选一个；
 *   2) 只有两个引脚、没有三极管、丝印只写 BUZZER：无源，选第三个；
 *   3) 拿不准就挨个试：反正只改宏，不用改代码。
 *
 * 三种类型的对外用法完全一样（BEEP_On / BEEP_Off / BuzzerIRQ_Beep ...），
 * 所以业务代码不需要区分它们。
 */
#ifndef __BUZZER_CFG_H
#define __BUZZER_CFG_H

#include "stm32f10x.h"

/* ==================================================================
 * 1. 蜂鸣器类型：改这一行就行
 * ================================================================== */
#define BUZZER_TYPE_ACTIVE_LOW   0   /* 有源，低电平响 */
#define BUZZER_TYPE_ACTIVE_HIGH  1   /* 有源，高电平响 */
#define BUZZER_TYPE_PASSIVE      2   /* 无源，要方波（脉冲）才响 */

#define BUZZER_TYPE  BUZZER_TYPE_ACTIVE_LOW

/* ==================================================================
 * 2. 接线：换引脚只改这三行（GPIOA~G 都在 APB2 上，RCC 宏跟着端口改）
 * ================================================================== */
#define BUZZER_PORT       GPIOB
#define BUZZER_PIN        GPIO_Pin_10
#define BUZZER_GPIO_RCC   RCC_APB2Periph_GPIOB

/* ==================================================================
 * 3. 无源蜂鸣器的方波频率（有源蜂鸣器用不到）
 *    常见谐振频率 2700Hz / 4000Hz，选和手上器件最接近的，声音最大
 * ================================================================== */
#define BUZZER_FREQ_HZ    2700u

/* ==================================================================
 * 下面是自动推导出来的，不用改
 *   BUZZER_NEED_SQUARE : 1 = 要方波（无源），0 = 给电平就响（有源）
 *   BUZZER_LEVEL_ON    : "响"的时候引脚是什么电平
 *   BUZZER_LEVEL_OFF   : "静音"的时候引脚是什么电平
 * ================================================================== */
#if (BUZZER_TYPE == BUZZER_TYPE_ACTIVE_LOW)
#define BUZZER_NEED_SQUARE   0
#define BUZZER_LEVEL_ON      0
#define BUZZER_LEVEL_OFF     1
#elif (BUZZER_TYPE == BUZZER_TYPE_ACTIVE_HIGH)
#define BUZZER_NEED_SQUARE   0
#define BUZZER_LEVEL_ON      1
#define BUZZER_LEVEL_OFF     0
#elif (BUZZER_TYPE == BUZZER_TYPE_PASSIVE)
#define BUZZER_NEED_SQUARE   1
#define BUZZER_LEVEL_ON      0   /* 无源：低电平是"导通"，但只给电平不会响 */
#define BUZZER_LEVEL_OFF     1   /* 静音时拉高，线圈不过直流 */
#else
#error "BUZZER_TYPE 只能填 BUZZER_TYPE_ACTIVE_LOW / BUZZER_TYPE_ACTIVE_HIGH / BUZZER_TYPE_PASSIVE"
#endif

/* 半个方波周期的微秒数：2700Hz -> 185us */
#define BUZZER_HALF_US    (500000u / BUZZER_FREQ_HZ)

/* 按类型输出"响 / 静音"电平 */
#define BUZZER_SET_LEVEL(lv)                                    \
    do {                                                        \
        if (lv) {                                               \
            GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);              \
        } else {                                                \
            GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);            \
        }                                                       \
    } while (0)

#define BUZZER_ON()     BUZZER_SET_LEVEL(BUZZER_LEVEL_ON)
#define BUZZER_OFF()    BUZZER_SET_LEVEL(BUZZER_LEVEL_OFF)

/* 翻转引脚：直接写 BSRR / BRR，只动一次寄存器，中断里也够快 */
#define BUZZER_TOGGLE()                                         \
    do {                                                        \
        if ((BUZZER_PORT->ODR & BUZZER_PIN) != 0) {             \
            BUZZER_PORT->BRR  = BUZZER_PIN;                     \
        } else {                                                \
            BUZZER_PORT->BSRR = BUZZER_PIN;                     \
        }                                                       \
    } while (0)

#endif /* __BUZZER_CFG_H */
