/**
 * @file    tick_irq.c
 * @brief   1ms 时基：TIM6 更新中断，顺序调用挂上来的钩子
 *
 * 中断里只做两件事：毫秒计数 + 依次调用钩子。钩子必须短（读几个 IO、跑个小状态机），
 * 不能在里面 delay、printf、等标志位。
 */

#include "tick_irq.h"

static volatile uint32_t s_ms = 0;                              /* 毫秒计数 */
static void (*s_hooks[TICK_MAX_HOOKS])(void);                    /* 1ms 钩子表 */
static volatile uint8_t s_hook_count = 0;                        /* 已挂几个 */

/**
 * @brief  初始化 TIM6 为 1ms 时基（不启动）
 */
void TICK_Init(void)
{
    TIM_TimeBaseInitTypeDef tb;
    NVIC_InitTypeDef nvic;

    RCC_APB1PeriphClockCmd(TICK_TIM_RCC, ENABLE);

    /* 72MHz / (71+1) = 1MHz；每 1000 个计数溢出一次 = 1ms */
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = 72 - 1;
    tb.TIM_Period = 1000 - 1;
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TICK_TIM, &tb);

    TIM_ClearFlag(TICK_TIM, TIM_FLAG_Update);
    TIM_ITConfig(TICK_TIM, TIM_IT_Update, ENABLE);

    /* 优先级 3：低于数码管(TIM4=1)、蜂鸣器(TIM2=2)和按键 EXTI(2)，时基晚一两微秒无所谓 */
    nvic.NVIC_IRQChannel = TIM6_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 3;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    TIM_Cmd(TICK_TIM, DISABLE);
}

/**
 * @brief  启动 1ms 心跳
 */
void TICK_Start(void)
{
    TIM_SetCounter(TICK_TIM, 0);
    TIM_Cmd(TICK_TIM, ENABLE);
}

/**
 * @brief  开机以来的毫秒数
 */
uint32_t TICK_GetMs(void)
{
    return s_ms;
}

/**
 * @brief  挂一个每 1ms 调用一次的函数
 * @param  fn 钩子函数，必须短小、不能阻塞
 * @retval 1 = 挂上，0 = 参数为空或钩子已满
 */
uint8_t TICK_AddHook(void (*fn)(void))
{
    if ((fn == 0) || (s_hook_count >= TICK_MAX_HOOKS))
    {
        return 0;
    }

    s_hooks[s_hook_count] = fn;
    s_hook_count++;
    return 1;
}

/**
 * @brief  TIM6 更新中断：毫秒计数 + 依次执行钩子
 * @note   中断名固定是 TIM6_IRQHandler，只能在工程里定义一次（本文件定义）
 */
void TIM6_IRQHandler(void)
{
    uint8_t i;
    uint8_t n;

    if (TIM_GetITStatus(TICK_TIM, TIM_IT_Update) == RESET)
    {
        return;
    }

    TIM_ClearITPendingBit(TICK_TIM, TIM_IT_Update);

    s_ms++;

    n = s_hook_count; /* 先取快照，防止钩子里再加钩子时越界 */
    for (i = 0; i < n; i++)
    {
        if (s_hooks[i] != 0)
        {
            s_hooks[i]();
        }
    }
}
