/**
 * @file    rgb_app.h
 * @brief   RGB 三色灯驱动：R=PC6、G=PC7、B=PC8（TIM3 全重映射 PWM）
 *
 * 硬件：
 *   4 脚 RGB 模块有两种：
 *     - 共阴：公共端接 GND，给高电平点亮（RGB_COMMON_ANODE = 0）；
 *     - 共阳：公共端接 3V3/5V，给低电平点亮（RGB_COMMON_ANODE = 1）。
 *   **本工程这块模块是共阳（低电平有效）**，所以下面配成 1：
 *   R=PC6、G=PC7、B=PC8，公共端接 3V3，引脚拉低对应的颜色才亮。
 *
 * 为什么用 PWM：
 *   三路 GPIO 只能亮 7 种颜色，没法调亮度、没法配出橙色/紫色这类中间色。
 *   用 TIM3 的三个通道输出 10kHz PWM，占空比 0~100 可调，三色就能混出任意颜色。
 *
 * 引脚为什么是 PC6/PC7/PC8：
 *   TIM3 的通道 1/2/3 默认在 PA6/PA7/PB0，PB0 已经被光敏 DO 占用；
 *   开 TIM3 全重映射后正好落到 PC6/PC7/PC8，和本工程其它引脚不冲突。
 *   （全重映射还会把 CH4 挪到 PC9，本工程不用 CH4，PC9 留空。）
 *
 * 不用 PWM 也能跑：
 *   把 RGB_USE_PWM 改成 0，三个脚就按普通推挽输出用，rgb_set() 只认"亮/灭"，
 *   这时引脚可以换成任意空闲 IO（同时改下面的 3 个 PIN 宏和 RGB_GPIO_RCC）。
 */

#ifndef __RGB_APP_H
#define __RGB_APP_H

#include "stm32f10x.h" /* uint8_t / GPIO_Pin_x / TIM3 */

/* ------------------------------------------------------------------
 * 接线与工作模式：换引脚/换共阴共阳/关 PWM 都只改这一小段
 * ------------------------------------------------------------------ */
#define RGB_USE_PWM       1            /* 1 = TIM3 硬件 PWM（可调色）；0 = 普通 GPIO 开关 */
#define RGB_PORT          GPIOC        /* 三个颜色脚都在 GPIOC */
#define RGB_GPIO_RCC      RCC_APB2Periph_GPIOC
#define RGB_R_PIN         GPIO_Pin_6   /* R = PC6 = TIM3_CH1（全重映射） */
#define RGB_G_PIN         GPIO_Pin_7   /* G = PC7 = TIM3_CH2（全重映射） */
#define RGB_B_PIN         GPIO_Pin_8   /* B = PC8 = TIM3_CH3（全重映射） */
#define RGB_COMMON_ANODE  1            /* 0 = 共阴（高电平点亮）；1 = 共阳（低电平点亮）
                                        * 本模块是共阳/低电平有效，所以取 1 */
#define RGB_MAX           100          /* 亮度/占空比满量程：0~100 */
#define RGB_INIT_SELFTEST 0            /* 1 = rgb_init() 里跑一遍红→绿→蓝→白自检 */
#define RGB_PWM_TIM       TIM3
#define RGB_PWM_RCC       RCC_APB1Periph_TIM3

/* 常用颜色，rgb_show() 直接用 */
typedef enum
{
    RGB_BLACK = 0, /* 全灭 */
    RGB_RED,       /* 红 */
    RGB_GREEN,     /* 绿 */
    RGB_BLUE,      /* 蓝 */
    RGB_YELLOW,    /* 黄 = 红 + 绿 */
    RGB_CYAN,      /* 青 = 绿 + 蓝 */
    RGB_MAGENTA,   /* 品红 = 红 + 蓝 */
    RGB_WHITE      /* 白 = 三色全亮 */
} rgb_color_t;

/* 初始化三路 PWM（或普通输出），上电先熄灭 */
void rgb_init(void);

/* 直接给三路亮度：0~100，超出按 100 处理；PWM 关闭时只认 0/非 0 */
void rgb_set(uint8_t r, uint8_t g, uint8_t b);

/* 用预定义颜色名点灯 */
void rgb_show(rgb_color_t color);

/* 三色全灭 */
void rgb_off(void);

/* 上电自检：红 -> 绿 -> 蓝 -> 白 各 500ms，最后熄灭（阻塞约 2s） */
void rgb_self_test(void);

#endif /* __RGB_APP_H */
