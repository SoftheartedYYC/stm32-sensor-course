/**
 * @file    buzzer_irq.c
 * @brief   蜂鸣器（中断版）：TIM2 定时中断，按 buzzer_cfg.h 里的类型工作
 *
 * 无源蜂鸣器（BUZZER_TYPE_PASSIVE）：
 *   TIM2 挂 APB1，定时器时钟 72MHz；预分频 71 -> 计数频率 1MHz（1 个计数 = 1us）；
 *   自动重装值 184 -> 每 185 个计数溢出一次，即 185us 进一次更新中断；
 *   中断里翻转一次 PB10，高、低各 185us -> 一个完整周期 370us -> 2.7kHz。
 *   静音时 TIM_Cmd(DISABLE)，中断完全停掉。
 *
 * 有源蜂鸣器（BUZZER_TYPE_ACTIVE_LOW / _ACTIVE_HIGH）：
 *   给电平就响，根本不需要翻转。定时器改成 1ms 一拍，只用来数 Beep 的时长；
 *   连续鸣叫（Start）时连定时器都不用开，直接输出电平，零中断开销。
 *
 * 两种情况下对外接口（Start / Stop / Beep）完全一样，业务代码不用区分。
 */

#include "buzzer_irq.h"

static volatile uint8_t  s_on = 0;     /* 1 = 正在响 */
static volatile uint16_t s_remain = 0; /* Beep 剩余节拍数；0 = 不自动停 */

/**
 * @brief  蜂鸣器引脚配推挽输出，并先写静音电平
 * @note   先写 ODR 再切输出，避免初始化瞬间响一下；
 *         静音电平由 BUZZER_TYPE 决定（有源高触发是低电平，其余是高电平）
 */
static void buzzer_pin_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(BUZZER_IRQ_GPIO_RCC, ENABLE);

    BUZZER_OFF();                          /* 先定静音电平（高/低看类型） */

    gpio.GPIO_Pin = BUZZER_IRQ_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_IRQ_PORT, &gpio);
}

/**
 * @brief  初始化蜂鸣器：GPIO + TIM2 时基 + 更新中断 + NVIC
 * @note   初始化完是静音状态，定时器不启动；不依赖 delay_init()
 */
void BuzzerIRQ_Init(void)
{
    TIM_TimeBaseInitTypeDef tb;
    NVIC_InitTypeDef nvic;

    buzzer_pin_init();

    RCC_APB1PeriphClockCmd(BUZZER_IRQ_TIM_RCC, ENABLE);

    /* 72MHz / (71+1) = 1MHz，1 个计数 = 1us；
     * 无源：每 185 次计数溢出 = 185us = 半个方波周期；
     * 有源：每 1000 次计数溢出 = 1ms，只当计时器 */
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = 72 - 1;
    tb.TIM_Period = (uint16_t)(BUZZER_IRQ_TICK_US - 1);
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(BUZZER_IRQ_TIM, &tb);

    TIM_ClearFlag(BUZZER_IRQ_TIM, TIM_FLAG_Update);
    TIM_ITConfig(BUZZER_IRQ_TIM, TIM_IT_Update, ENABLE);

    /* 优先级比数码管扫描（TIM4，抢占 1）低一档：屏幕扫描优先，蜂鸣器晚一两微秒无所谓 */
    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    TIM_Cmd(BUZZER_IRQ_TIM, DISABLE); /* 静音时不跑中断 */

#if BUZZER_INIT_SELFTEST
    BuzzerIRQ_SelfTest(); /* 上电自检：响 200ms，确认接线和类型选对了 */
#endif
}

/**
 * @brief  开始连续鸣叫
 * @note   无源：启动定时器，中断里一直翻转引脚；
 *         有源：直接输出"响"的电平，定时器关掉不跑中断；
 *         已经在连续响时直接返回，避免每圈主循环都重置计数器、把相位打乱
 */
void BuzzerIRQ_Start(void)
{
    if ((s_on != 0) && (s_remain == 0))
    {
        return;
    }

    s_remain = 0; /* 0 = 不自动停 */
    s_on = 1;

#if BUZZER_NEED_SQUARE
    TIM_SetCounter(BUZZER_IRQ_TIM, 0);
    TIM_Cmd(BUZZER_IRQ_TIM, ENABLE);
#else
    TIM_Cmd(BUZZER_IRQ_TIM, DISABLE); /* 有源不需要打拍子 */
    BUZZER_ON();
#endif
}

/**
 * @brief  停止鸣叫：关定时器、引脚回到静音电平
 */
void BuzzerIRQ_Stop(void)
{
    s_on = 0;
    s_remain = 0;

    TIM_Cmd(BUZZER_IRQ_TIM, DISABLE);
    BUZZER_OFF();
}

/**
 * @brief  响 ms 毫秒后自动停（非阻塞）
 * @param  ms 发声时长，单位 ms
 * @note   把时长换算成"多少个定时器节拍"，中断里每进一次减 1，减到 0 自己停；
 *         函数本身立刻返回，主循环可以接着干别的事。
 *         无源：1 拍 = 半个方波周期；有源：1 拍 = 1ms
 */
void BuzzerIRQ_Beep(uint16_t ms)
{
    uint32_t n;
    uint16_t ticks;

    if (ms == 0)
    {
        return;
    }

    BuzzerIRQ_Stop(); /* 先停掉上一段，重新计时 */

    n = ((uint32_t)ms * 1000u) / BUZZER_IRQ_TICK_US; /* 需要的定时器节拍数 */
    if (n == 0u)
    {
        n = 1u; /* 至少响一拍 */
    }
    if (n > 65535u)
    {
        n = 65535u; /* s_remain 是 16 位；无源时 65535 拍约 12 秒，够用了 */
    }
    ticks = (uint16_t)n;

    s_remain = ticks;
    s_on = 1;

#if !BUZZER_NEED_SQUARE
    BUZZER_ON(); /* 有源：先把电平给上，定时器只负责到点关掉 */
#endif

    TIM_SetCounter(BUZZER_IRQ_TIM, 0);
    TIM_Cmd(BUZZER_IRQ_TIM, ENABLE);
}

/**
 * @brief  当前是否在响
 * @retval 1 = 响，0 = 静音
 */
uint8_t BuzzerIRQ_IsOn(void)
{
    return s_on;
}

/**
 * @brief  上电自检：响 200ms 后自动停
 * @note   非阻塞，初始化时调用不会拖慢启动
 */
void BuzzerIRQ_SelfTest(void)
{
    BuzzerIRQ_Beep(200);
}

/**
 * @brief  TIM2 更新中断：无源时翻转引脚，顺便处理 Beep 的剩余节拍
 * @note   中断名固定是 TIM2_IRQHandler，只能在工程里定义一次（本文件定义）
 */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(BUZZER_IRQ_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(BUZZER_IRQ_TIM, TIM_IT_Update);

        if (s_on == 0)
        {
            return; /* 已经停了：可能是一个还没处理的挂起中断 */
        }

#if BUZZER_NEED_SQUARE
        BUZZER_TOGGLE(); /* 无源：每半个周期翻转一次，产生方波 */
#endif
        /* 有源：引脚电平一直保持，中断只管数 Beep 的时长，什么都不用做 */

        if (s_remain != 0) /* Beep 模式：数够节拍就停 */
        {
            s_remain--;
            if (s_remain == 0)
            {
                BuzzerIRQ_Stop();
            }
        }
    }
}
