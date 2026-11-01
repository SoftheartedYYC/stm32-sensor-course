/**
 * @file    hcsr04.c
 * @brief   HC-SR04 超声波测距：TIM1_CH1 输入捕获
 *
 * 捕获流程：
 *   1. Trig 给 12us 高电平；
 *   2. 模块发 8 个 40kHz 脉冲，Echo 变高 -> TIM1 捕获上升沿，记下计数器值；
 *   3. 收到回波，Echo 变低 -> 捕获下降沿，两个捕获值相减就是高电平宽度；
 *   4. 距离(cm) = 宽度(us) / 58。
 *
 * 为什么用"两次捕获相减"而不是"上升沿清零计数器"：
 *   中断响应有几个微秒的延迟，清零会把这段延迟算进去；
 *   直接相减只用捕获寄存器里的硬件时间戳，更准。
 *   16 位计数器 65.5ms 溢出一次，而回波最宽约 38ms，减法取 16 位结果正好正确。
 */

#include "hcsr04.h"
#include "delay.h"

static volatile uint8_t  s_active;   /* 1 = 正在等回波，期间其它边沿才处理 */
static volatile uint8_t  s_phase;    /* 0 = 等上升沿；1 = 已捕获上升、等下降沿 */
static volatile uint8_t  s_done;     /* 1 = 这次已经捕获到下降沿 */
static volatile uint16_t s_rise_us;  /* 上升沿时的计数器值 */
static volatile uint16_t s_width_us; /* 回波宽度（us） */

/**
 * @brief  初始化 Trig / Echo 引脚和 TIM1_CH1 输入捕获
 * @note   仅配置，不依赖 delay_init()；测量函数里等回波才需要延时
 */
void HCSR04_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tb;
    TIM_ICInitTypeDef ic;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* Trig：推挽输出，空闲低电平 */
    GPIO_ResetBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
    gpio.GPIO_Pin = HCSR04_TRIG_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HCSR04_TRIG_PORT, &gpio);

    /* Echo：下拉输入，模块空闲时是低电平 */
    gpio.GPIO_Pin = HCSR04_ECHO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(HCSR04_ECHO_PORT, &gpio);

    /* TIM1：72MHz / (71+1) = 1MHz，1 个计数 = 1us */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = 72 - 1;
    tb.TIM_Period = 0xFFFF;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM1, &tb);

    /* CH1 直接映射到 TI1（PA8），先等上升沿 */
    TIM_ICStructInit(&ic);
    ic.TIM_Channel = TIM_Channel_1;
    ic.TIM_ICPolarity = TIM_ICPolarity_Rising;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    ic.TIM_ICFilter = 0x0;
    TIM_ICInit(TIM1, &ic);

    TIM_ClearFlag(TIM1, TIM_FLAG_CC1);
    TIM_ITConfig(TIM1, TIM_IT_CC1, ENABLE);

    /* 优先级 2：和蜂鸣器/按键同级，高于 1ms 时基 */
    nvic.NVIC_IRQChannel = TIM1_CC_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    s_phase = 0;
    s_done = 0;
    s_rise_us = 0;
    s_width_us = 0;
    s_active = 0;

    TIM_Cmd(TIM1, ENABLE); /* 输入捕获要一直计数 */
}

/**
 * @brief  测一次，返回原始回波宽度
 * @param  width_us 输出：Echo 高电平宽度，单位 us
 * @retval 1 = 成功，0 = 等不到上升/下降沿（超时）
 * @note   阻塞：正常情况下几毫秒；最坏等 10ms + 60ms
 */
uint8_t HCSR04_ReadUs(uint16_t *width_us)
{
    uint32_t timeout;

    if (width_us == 0)
    {
        return 0;
    }

    /* 清状态，先等上升沿 */
    s_done = 0;
    s_phase = 0;
    s_active = 1;
    TIM_OC1PolarityConfig(TIM1, TIM_ICPolarity_Rising);

    /* 触发：>=10us 高电平 */
    GPIO_SetBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
    delay_us(HCSR04_TRIG_US);
    GPIO_ResetBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);

    /* 等上升沿：模块要先发 8 个 40kHz 脉冲，一般 1ms 内到 */
    timeout = 0;
    while ((s_phase == 0) && (s_done == 0))
    {
        if (++timeout > 1000) /* 10ms */
        {
            s_active = 0;
            return 0;
        }
        delay_us(10);
    }

    /* 等下降沿：4m 约 23ms，留 60ms 余量 */
    timeout = 0;
    while (s_done == 0)
    {
        if (++timeout > 6000) /* 60ms */
        {
            s_active = 0;
            return 0;
        }
        delay_us(10);
    }

    *width_us = s_width_us;
    return 1;
}

/**
 * @brief  测一次，返回距离（cm）
 * @param  cm 输出：2~400（超出量程按失败处理）
 * @retval 1 = 成功，0 = 超时或超量程
 */
uint8_t HCSR04_ReadCm(uint16_t *cm)
{
    uint16_t us;

    if (cm == 0)
    {
        return 0;
    }

    if (HCSR04_ReadUs(&us) == 0)
    {
        return 0;
    }

    if ((us < HCSR04_MIN_US) || (us > HCSR04_MAX_US))
    {
        return 0; /* 太近、太远，或者根本没遇到障碍物（38ms 那种） */
    }

    *cm = (uint16_t)((us + 29u) / 58u);
    return 1;
}

/**
 * @brief  测一次，返回距离（mm）
 * @param  mm 输出：20~4000（超出量程按失败处理）
 * @retval 1 = 成功，0 = 超时或超量程
 */
uint8_t HCSR04_ReadMm(uint16_t *mm)
{
    uint16_t us;

    if (mm == 0)
    {
        return 0;
    }

    if (HCSR04_ReadUs(&us) == 0)
    {
        return 0;
    }

    if ((us < HCSR04_MIN_US) || (us > HCSR04_MAX_US))
    {
        return 0;
    }

    *mm = (uint16_t)(((uint32_t)us * 10u + 29u) / 58u);
    return 1;
}

/**
 * @brief  TIM1 捕获中断：上升沿记时间，下降沿算宽度
 * @note   中断名固定是 TIM1_CC_IRQHandler，只能定义一次
 */
void TIM1_CC_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM1, TIM_IT_CC1) != RESET)
    {
        TIM_ClearITPendingBit(TIM1, TIM_IT_CC1);

        if (s_active == 0)
        {
            return; /* 不是测量期间的边沿，忽略 */
        }

        if (s_phase == 0)
        {
            /* 上升沿：记下硬件时间戳，改成等下降沿 */
            s_rise_us = (uint16_t)TIM_GetCapture1(TIM1);
            TIM_OC1PolarityConfig(TIM1, TIM_ICPolarity_Falling);
            s_phase = 1;
        }
        else
        {
            /* 下降沿：两次捕获相减就是高电平宽度（16 位回绕也正确） */
            s_width_us = (uint16_t)(TIM_GetCapture1(TIM1) - s_rise_us);
            TIM_OC1PolarityConfig(TIM1, TIM_ICPolarity_Rising);
            s_phase = 0;
            s_done = 1;
            s_active = 0;
        }
    }
}
