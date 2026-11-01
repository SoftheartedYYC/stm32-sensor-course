/**
 * @file    hcsr04.h
 * @brief   HC-SR04 超声波测距（定时器输入捕获版）
 *
 * 接线：
 *   VCC  -> 5V
 *   GND  -> GND（必须和 MCU 共地）
 *   Trig -> PB12（推挽输出，给 >=10us 高电平触发）
 *   Echo -> PA8 （TIM1_CH1 输入捕获，1us 分辨率）
 *
 * 原理：
 *   Trig 给一个 10us 以上的高电平，模块内部发 8 个 40kHz 脉冲；
 *   同时 Echo 拉高，收到回波后拉低；
 *   Echo 高电平的宽度 = 声波往返时间，距离(cm) = 宽度(us) / 58。
 *
 * 定时器配置：
 *   TIM1 挂 APB2（72MHz），预分频 71 -> 1MHz，1 个计数 = 1us；
 *   16 位计数器，65.5ms 溢出，而 HC-SR04 最远 4m 的回波约 23ms，够用。
 *   上升沿捕获后记下时间，下降沿再捕获一次，两个值相减就是宽度。
 *
 * 注意：Echo 是 5V 输出。PA8 是 5V 容忍脚，可以直接接；
 *       想更稳妥就串 1k 电阻、Echo 对 GND 并 2k 分压到 3.3V。
 */
#ifndef __HCSR04_H
#define __HCSR04_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 接线：Trig 可以换任意空闲 GPIO；Echo 必须接 TIM1_CH1（PA8），
 *       换通道要连中断里的 CC1 一起改，不只是改宏。
 * ------------------------------------------------------------------ */
#define HCSR04_TRIG_PORT  GPIOB
#define HCSR04_TRIG_PIN   GPIO_Pin_12
#define HCSR04_ECHO_PORT  GPIOA
#define HCSR04_ECHO_PIN   GPIO_Pin_8

#define HCSR04_TRIG_US    12     /* 触发脉宽，手册要求 >=10us */
#define HCSR04_MIN_US     116u   /* 2cm 对应的回波宽度 */
#define HCSR04_MAX_US     23200u /* 400cm 对应的回波宽度 */

/* 初始化：Trig 推挽输出、Echo 下拉输入、TIM1_CH1 双边沿捕获中断
 * 不依赖 delay_init()，但下面的测量函数要等回波，必须先 delay_init() */
void HCSR04_Init(void);

/* 测一次，返回原始回波宽度（us）。1 = 成功，0 = 等不到回波/超时 */
uint8_t HCSR04_ReadUs(uint16_t *width_us);

/* 测一次，返回距离。1 = 成功，0 = 超时或超出 2~400cm 量程
 * 阻塞时间：正常情况下等于回波宽度（几毫秒），最坏约 60ms */
uint8_t HCSR04_ReadCm(uint16_t *cm);
uint8_t HCSR04_ReadMm(uint16_t *mm);

#endif /* __HCSR04_H */
