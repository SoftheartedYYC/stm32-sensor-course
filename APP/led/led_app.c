/**
 * @file led_app.c
 * @brief 板载 LED 统一驱动：D0=PB5，D1=PE5，低电平点亮
 */

#include "led_app.h"

#define LED0_PORT GPIOB
#define LED1_PORT GPIOE
#define LED0_PIN  GPIO_Pin_5
#define LED1_PIN  GPIO_Pin_5

void led_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOE, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = LED0_PIN;
    GPIO_Init(LED0_PORT, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = LED1_PIN;
    GPIO_Init(LED1_PORT, &GPIO_InitStructure);

    led_all(0);
}

void led_on(uint8_t id)
{
    if (id == 0) GPIO_ResetBits(LED0_PORT, LED0_PIN);
    if (id == 1) GPIO_ResetBits(LED1_PORT, LED1_PIN);
}

void led_off(uint8_t id)
{
    if (id == 0) GPIO_SetBits(LED0_PORT, LED0_PIN);
    if (id == 1) GPIO_SetBits(LED1_PORT, LED1_PIN);
}

/* 调用一次翻转一次，循环调用并加延时就是闪烁 */
void led_blink(uint8_t id)
{
    if (id == 0)
    {
        if (GPIO_ReadOutputDataBit(LED0_PORT, LED0_PIN) == 0)
            GPIO_SetBits(LED0_PORT, LED0_PIN);
        else
            GPIO_ResetBits(LED0_PORT, LED0_PIN);
    }
    if (id == 1)
    {
        if (GPIO_ReadOutputDataBit(LED1_PORT, LED1_PIN) == 0)
            GPIO_SetBits(LED1_PORT, LED1_PIN);
        else
            GPIO_ResetBits(LED1_PORT, LED1_PIN);
    }
}

void led_all(uint8_t on)
{
    if (on)
    {
        led_on(0);
        led_on(1);
    }
    else
    {
        led_off(0);
        led_off(1);
    }
}

/* 兼容旧代码的无参数接口 */
void led0_on(void)   { led_on(0); }
void led0_off(void)  { led_off(0); }
void led0_turn(void) { led_blink(0); }
void led1_on(void)   { led_on(1); }
void led1_off(void)  { led_off(1); }
void led1_turn(void) { led_blink(1); }
