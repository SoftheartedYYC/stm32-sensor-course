/**
 * @file    relay_app.c
 * @brief   三路继电器驱动：PC0/PC1/PC2 推挽输出，高电平触发
 *
 * 模块原理：
 *   常见的多路继电器模块板上已经带好光耦、三极管和续流二极管，
 *   IN1/IN2/IN3 只是光耦输入侧，单片机给电平就能控制，不需要自己算线圈电流。
 *   高电平触发 = IN 为高时线圈通电、COM-NO 导通；低电平触发则相反。
 *
 * 为什么先写输出锁存器再切输出模式：
 *   GPIO 复位后是浮空输入，输出锁存器的初值不确定。如果先切成推挽输出、
 *   再写 0，中间会有一小段时间输出锁存器里的 1，高电平触发的继电器就会
 *   在上电瞬间"啪"一下。先 SetBits/ResetBits 再 GPIO_Init，切换瞬间电平
 *   已经是"释放"，不会误动作。蜂鸣器驱动也是同样的思路。
 *
 * 加路数：在 relay_app.h 里加一个 RELAYx_PIN 和对应通道号，再把
 *   RELAY_CH_COUNT 加 1，本文件的引脚表补一行即可（下面的函数都是循环的）。
 */

#include "relay_app.h"

/* 通道号 1/2/3 -> 引脚。要加第四路，就在 relay_app.h 里加宏、这里补一行 */
static const uint16_t s_relay_pin[RELAY_CH_COUNT] = {
    RELAY1_PIN, /* RELAY_CH1 = PC0 */
    RELAY2_PIN, /* RELAY_CH2 = PC1 */
    RELAY3_PIN, /* RELAY_CH3 = PC2 */
};

/* 三路引脚位掩码，初始化时一次配完 */
#define RELAY_ALL_PINS (RELAY1_PIN | RELAY2_PIN | RELAY3_PIN)

/**
 * @brief  把通道号换算成引脚位掩码
 * @param  ch RELAY_CH1 ~ RELAY_CH3
 * @return 对应的 GPIO_Pin_x；通道号非法时返回 0（不会碰到任何引脚）
 */
static uint16_t relay_pin_of(uint8_t ch)
{
    if ((ch < 1) || (ch > RELAY_CH_COUNT))
    {
        return 0;
    }
    return s_relay_pin[ch - 1];
}

/**
 * @brief  把一路继电器写成指定状态
 * @param  pin 该通道的 GPIO_Pin_x
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
 * @brief  初始化 PC0/PC1/PC2 为推挽输出，上电后三路都释放
 */
void relay_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); /* GPIOC 时钟 */

    /* 先把三个输出锁存器写成"释放"电平，再切成输出模式，避免上电误吸合 */
#if RELAY_ON_LEVEL
    GPIO_ResetBits(RELAY_PORT, RELAY_ALL_PINS); /* 高触发：0 = 释放 */
#else
    GPIO_SetBits(RELAY_PORT, RELAY_ALL_PINS);   /* 低触发：1 = 释放 */
#endif

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; /* 推挽输出：能主动输出高/低 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = RELAY_ALL_PINS;
    GPIO_Init(RELAY_PORT, &GPIO_InitStructure);

    relay_all(0); /* 再保险一次：三路都释放 */
}

/**
 * @brief  指定通道吸合/释放
 * @param  ch RELAY_CH1(1) / RELAY_CH2(2) / RELAY_CH3(3)
 * @param  on 非 0 = 吸合，0 = 释放
 */
void relay_set(uint8_t ch, uint8_t on)
{
    uint16_t pin = relay_pin_of(ch);

    if (pin != 0)
    {
        relay_write(pin, on);
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
 *         打断，别的通道状态可能被旧值覆盖。这里只操作一路，影响更小。
 */
void relay_toggle(uint8_t ch)
{
    relay_set(ch, (uint8_t)(relay_get(ch) ? 0 : 1));
}

/**
 * @brief  三路一起吸合或释放
 */
void relay_all(uint8_t on)
{
    uint8_t ch;

    for (ch = 1; ch <= RELAY_CH_COUNT; ch++)
    {
        relay_set(ch, on);
    }
}

/**
 * @brief  读当前输出状态
 * @return 1 = 吸合，0 = 释放；通道号非法时返回 0
 * @note   读的是 GPIO 输出锁存器（ODR），不是继电器触点反馈。
 *         模块没有触点反馈脚时，这是唯一能确认"软件让它吸合了"的方法。
 */
uint8_t relay_get(uint8_t ch)
{
    uint16_t pin = relay_pin_of(ch);

    if (pin == 0)
    {
        return 0;
    }
    return (uint8_t)(GPIO_ReadOutputDataBit(RELAY_PORT, pin) == RELAY_ON_LEVEL);
}
