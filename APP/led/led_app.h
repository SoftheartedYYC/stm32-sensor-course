/**
 * @file led_app.h
 * @brief 板载 LED 驱动：D0 = PB5（光敏状态灯），D1 = PE5（心跳灯）
 *
 * 本板两颗 LED 都是"低电平点亮"：阳极经 510R 接 3V3，阴极接 MCU。
 * 所以引脚输出 0 = 亮，输出 1 = 灭。
 *
 * 写法参考课程示例（江协科技 3-4 按键控制 LED 的 LED.c）：
 * 一颗灯一组 ON / OFF / Turn 函数，都没有参数，看名字就知道干什么。
 */

#ifndef __LED_APP_H /* 头文件保护宏：没定义过才进入正文 */
#define __LED_APP_H /* 定义保护宏，第二次包含时直接跳到 #endif */

#include "stm32f10x.h" /* 标准外设库：GPIO 相关定义 */

void led_init(void); /* 初始化两颗 LED（推挽输出）并熄灭 */

void led_on(uint8_t id); /* 点亮指定 LED：0=PB5，1=PE5 */
void led_off(uint8_t id); /* 熄灭指定 LED */
void led_blink(uint8_t id); /* 翻转指定 LED，用于闪烁 */
void led_all(uint8_t on); /* 同时点亮或熄灭两颗 LED */

/* 兼容旧代码的无参数接口 */
void led0_on(void);
void led0_off(void);
void led0_turn(void);
void led1_on(void);
void led1_off(void);
void led1_turn(void);

#endif /* __LED_APP_H */
