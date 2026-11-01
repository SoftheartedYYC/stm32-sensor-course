/**
 * @file    relay_app.c
 * @brief   两路继电器驱动：PC0/PC1 推挽输出，高电平触发
 *
 * 模块原理：
 *   常见的"两路继电器模块"板上已经带好光耦、三极管和续流二极管，
 *   IN1/IN2 只是光耦输入侧，单片机给电平就能控制，不需要自己算线圈电流。
 *   高电平触发 = IN 为高时线圈通电、COM-NO 导通；低电平触发则相反。
 *
 * 为什么先写输出锁存器再切输出模式：
 *   GPIO 复位后是浮空输入，输出锁存器的初值不确定。如果先切成推挽输出、
 *   再写 0，中间会有一小段时间输出锁存器里的 1，高电平触发的继电器就会
 *   在上电瞬间"啪"一下。先 SetBits/ResetBits 再 GPIO_Init，切换瞬间电平
 *   已经是"释放"，不会误动作。蜂鸣器驱动也是同样的思路。
 */

#include "relay_app.h"

/**
 * @brief  把一路继电器写成指定状态
 * @param  pin RELAY1_PIN 或 RELAY2_PIN
 * @param  on  1 = 吸合，0 = 释放
 * @note   用 #if 在编译期决定极性，运行时不再比较电平，代码更直观。
 */
static void relay_write(uint16_t pin, uint8_t on)
{
#if RELAY_ON_LEVEL
    if (on)
    {
        GPIO_SetBits(RELAY_PORT, pin); /* 高电平 -> 吸合 */
    }
    else
    {
        GPIO_ResetBits(RELAY_PORT, pin); /* 低电平 -> 释放 */
    }
#else
    if (on)
    {
        GPIO_ResetBits(RELAY_PORT, pin); /* 低电平 -> 吸合 */
    }
    else
    {
        GPIO_SetBits(RELAY_PORT, pin); /* 高电平 -> 释放 */
    }
#endif
}

/**
 * @brief  初始化 PC0/PC1 为推挽输出，上电后两路都释放
 */
void relay_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); /* GPIOC 时钟 */

    /* 先把两个输出锁存器写成"释放"电平，再切成输出模式，避免上电误吸合 */
#if RELAY_ON_LEVEL
    GPIO_ResetBits(RELAY_PORT, RELAY1_PIN | RELAY2_PIN); /* 高触发：0 = 释放 */
#else
    GPIO_SetBits(RELAY_PORT, RELAY1_PIN | RELAY2_PIN);   /* 低触发：1 = 释放 */
#endif

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; /* 推挽输出：能主动输出高/低 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = RELAY1_PIN | RELAY2_PIN;
    GPIO_Init(RELAY_PORT, &GPIO_InitStructure);

    relay_all(0); /* 再保险一次：两路都释放 */
}

/**
 * @brief  指定通道吸合/释放
 * @param  ch RELAY_CH1(1) 或 RELAY_CH2(2)
 * @param  on 非 0 = 吸合，0 = 释放
 */
void relay_set(uint8_t ch, uint8_t on)
{
    if (ch == RELAY_CH1)
    {
        relay_write(RELAY1_PIN, on);
    }
    else if (ch == RELAY_CH2)
    {
        relay_write(RELAY2_PIN, on);
    }
}

/**
 * @brief  指定通道吸合
 */
void relay_on(uint8_t ch)
{
    relay_set(ch, 1);
}

/**
 * @brief  指定通道释放
 */
void relay_off(uint8_t ch)
{
    relay_set(ch, 0);
}

/**
 * @brief  翻转指定通道
 * @note   先读输出锁存器再取反；不要用 ODR ^= 直接写，读改写之间若被中断
 *         打断，另一路的状态可能被旧值覆盖。这里只操作一路，影响更小。
 */
void relay_toggle(uint8_t ch)
{
    relay_set(ch, (uint8_t)(relay_get(ch) ? 0 : 1));
}

/**
 * @brief  两路一起吸合或释放
 */
void relay_all(uint8_t on)
{
    relay_set(RELAY_CH1, on);
    relay_set(RELAY_CH2, on);
}

/**
 * @brief  读当前输出状态
 * @return 1 = 吸合，0 = 释放；通道号非法时返回 0
 * @note   读的是 GPIO 输出锁存器（ODR），不是继电器触点反馈。
 *         模块没有触点反馈脚时，这是唯一能确认"软件让它吸合了"的方法。
 */
uint8_t relay_get(uint8_t ch)
{
    if (ch == RELAY_CH1)
    {
        return (uint8_t)(GPIO_ReadOutputDataBit(RELAY_PORT, RELAY1_PIN) == RELAY_ON_LEVEL);
    }
    if (ch == RELAY_CH2)
    {
        return (uint8_t)(GPIO_ReadOutputDataBit(RELAY_PORT, RELAY2_PIN) == RELAY_ON_LEVEL);
    }
    return 0;
}
