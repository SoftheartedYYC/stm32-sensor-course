/**
 * @file    stepper_app.h
 * @brief   步进电机驱动（中断发脉冲）：EN = PE12，STP = PE13，DIR = PE14
 *
 * 原理：
 *   步进驱动器（TB6600 / A4988 / DRV8825 之类）只要 STP 上给一个脉冲就走一步，
 *   DIR 决定方向，EN 使能。本驱动用 TIM7 的更新中断每半个周期翻转一次 STP，
 *   高、低各半个周期 -> 一个完整脉冲 = 一步，脉冲频率 = 步频。
 *   发脉冲全靠硬件定时器，主循环不用管，只在需要时启动/停止。
 *
 * 步距角：
 *   电机是 1.8°/步（200 步/圈）还是 0.9°/步（400 步/圈），改下面的
 *   STEPPER_STEP_ANGLE 宏即可，角度换算（stepper_move_deg）会自动跟着变。
 *
 * 接线：
 *   EN  -> PE12   使能，默认低电平有效（STEPPER_EN_ACTIVE_LEVEL 可改）
 *   STP -> PE13   脉冲，TIM7 中断里翻转
 *   DIR -> PE14   方向，默认高电平 = 顺时针（STEPPER_DIR_CW_LEVEL 可改）
 *   驱动器 VCC/GND 按模块要求供电，**必须和 STM32 共地**；
 *   电机线圈 A+/A-/B+/B- 接驱动器，不要直接接单片机。
 *
 * 资源：
 *   TIM7（APB1，72MHz）+ 3 个推挽 GPIO；TIM7_IRQHandler 在本文件里定义，
 *   别的模块不要再用 TIM7。
 */

#ifndef __STEPPER_APP_H
#define __STEPPER_APP_H

#include "stm32f10x.h" /* uint8_t / GPIO_Pin_x */

/* ------------------------------------------------------------------
 * 接线与极性：换引脚/换触发电平只改这里
 * ------------------------------------------------------------------ */
#define STEPPER_EN_PORT          GPIOE
#define STEPPER_EN_PIN           GPIO_Pin_12  /* EN  = PE12 */
#define STEPPER_STP_PORT         GPIOE
#define STEPPER_STP_PIN          GPIO_Pin_13  /* STP = PE13 */
#define STEPPER_DIR_PORT         GPIOE
#define STEPPER_DIR_PIN          GPIO_Pin_14  /* DIR = PE14 */

#define STEPPER_EN_ACTIVE_LEVEL  0            /* 0 = 低电平使能（常见驱动板） */
#define STEPPER_DIR_CW_LEVEL     1            /* DIR = 1 时顺时针（接线不同就改 0） */

/* 发脉冲用的定时器：TIM7 空闲，换成别的定时器时记得同步 TIMx_IRQHandler */
#define STEPPER_TIM              TIM7
#define STEPPER_TIM_RCC          RCC_APB1Periph_TIM7
#define STEPPER_TIM_IRQn         TIM7_IRQn

/* ------------------------------------------------------------------
 * 步距角 + 驱动器细分：一圈脉冲数 = 整步步数 × 细分
 *   STEPPER_ANGLE_18 -> 1.8°/步，整步 200 步/圈（Emm42 屏幕上"电机类型"也选 1.8°）
 *   STEPPER_ANGLE_09 -> 0.9°/步，整步 400 步/圈（Emm42 屏幕上"电机类型"也选 0.9°）
 * ------------------------------------------------------------------ */
#define STEPPER_ANGLE_18   0
#define STEPPER_ANGLE_09   1
/* 本工程用的电机：1.8°/步（200 步/圈） */
#define STEPPER_STEP_ANGLE STEPPER_ANGLE_18

/* 驱动器细分：填 Emm42 屏幕上"细分"那个值，1 = 整步（不分细分）。
 *   本工程用的 Emm42 屏幕细分 = 256 -> 1.8° 电机一圈 = 200 × 256 = 51200 个脉冲。
 *   这里不填 256 的话，stepper_move_deg(360) 只发 200 个脉冲，
 *   电机只转 200/(200×256) 圈 = 1.4°，看起来就是"每次只动一点点"。
 *   细分越大，同样转速需要越高的脉冲频率（脉冲频率 = 细分 × 步频，Emm42 上限约 120kHz）。 */
#define STEPPER_MICROSTEP  256u

#if (STEPPER_STEP_ANGLE == STEPPER_ANGLE_09)
#define STEPPER_STEPS_PER_REV_FULL 400u   /* 0.9°：360 / 0.9 */
#else
#define STEPPER_STEPS_PER_REV_FULL 200u   /* 1.8°：360 / 1.8 */
#endif

/* 一圈实际需要的脉冲数（含细分） */
#define STEPPER_STEPS_PER_REV ((uint32_t)STEPPER_STEPS_PER_REV_FULL * STEPPER_MICROSTEP)

/* 方向参数：stepper_run(STEPPER_DIR_CW) 比 stepper_run(1) 好读 */
#define STEPPER_DIR_CW    1
#define STEPPER_DIR_CCW   0

/* 步频（步/秒）范围与默认值；太快电机带不动会丢步，太慢会共振
 * 上限 60000 是给高细分留的余量：脉冲半周期 = 500000/步频 us，
 * 60000 步/秒 -> 8us 半周期，仍在驱动器输入能力（~120kHz）和 5us 下限之间 */
#define STEPPER_SPS_MIN       10u
#define STEPPER_SPS_MAX       60000u
#define STEPPER_DEFAULT_SPS   400u

/* 初始化 PE12/PE13/PE14 为推挽输出 + TIM7 时基与更新中断；初始化完电机是"失能、不转" */
void stepper_init(void);

/* 使能/失能电机（失能时会立刻停脉冲） */
void stepper_enable(uint8_t on);

/* 设置方向：STEPPER_DIR_CW / STEPPER_DIR_CCW */
void stepper_set_dir(uint8_t dir);

/* 设置步频（步/秒），会自动限制在 STEPPER_SPS_MIN ~ STEPPER_SPS_MAX；运行中也能改 */
void stepper_set_speed(uint16_t steps_per_sec);

/* 读当前步频 */
uint16_t stepper_get_speed(void);

/* 按当前方向走 steps 步（非阻塞：立刻返回，脉冲由中断发完） */
void stepper_move_steps(uint32_t steps);

/* 走指定角度（非阻塞）：正数顺时针、负数逆时针，范围 -32767 ~ 32767；按 STEPPER_STEP_ANGLE 换算步数 */
void stepper_move_deg(int16_t degrees);

/* 连续转，直到 stepper_stop()；dir = STEPPER_DIR_CW / STEPPER_DIR_CCW */
void stepper_run(uint8_t dir);

/* 立刻停止发脉冲（电机停在当前步，不会自己滑行） */
void stepper_stop(void);

/* 1 = 还在走（定长或连续），0 = 停着 */
uint8_t stepper_is_busy(void);

/* 一圈多少步（200 或 400），方便自己算角度 */
uint32_t stepper_steps_per_rev(void);

#endif /* __STEPPER_APP_H */
