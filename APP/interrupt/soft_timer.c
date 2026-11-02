/**
 * @file    soft_timer.c
 * @brief   软定时器实现：一张固定大小的指针表 + 1ms 递减计数
 *
 * 设计取舍：
 *   - 不用动态内存：表是静态数组，8 个槽位够用，嵌入式里也更稳；
 *   - 结构体由调用者定义（static soft_timer_t xxx;），模块只存指针，
 *     这样不用提前知道每个任务的名字/数量，也不会有隐藏的 RAM 开销；
 *   - 到期判断用 16 位计数递减，1ms 一次，最长周期 65535ms（约 65 秒）；
 *   - 所有回调都在 TIM6 中断里执行，所以必须短小，不能阻塞。
 */

#include "soft_timer.h"

static soft_timer_t *s_timers[SOFT_TIMER_MAX]; /* 注册进来的任务 */
static volatile uint8_t s_count = 0;           /* 当前注册了几个 */

/**
 * @brief  清空软定时器表（上电时调一次）
 */
void SOFTTIMER_Init(void)
{
    uint8_t i;

    for (i = 0; i < SOFT_TIMER_MAX; i++)
    {
        s_timers[i] = 0;
    }
    s_count = 0;
}

/**
 * @brief  注册并启动一个周期任务
 * @param  timer     调用者定义的 soft_timer_t 变量
 * @param  fn        到期要调用的函数（在中断里执行，必须短小）
 * @param  period_ms 周期（ms），0 会按 1ms 处理
 * @retval 表项序号（>=0）；-1 = 参数非法或表已满
 */
int8_t SOFTTIMER_Add(soft_timer_t *timer, void (*fn)(void), uint16_t period_ms)
{
    if ((timer == 0) || (fn == 0) || (s_count >= SOFT_TIMER_MAX))
    {
        return -1;
    }

    timer->fn = fn;
    timer->period_ms = (period_ms == 0u) ? 1u : period_ms;
    timer->remain_ms = timer->period_ms;
    timer->running = 1;

    s_timers[s_count] = timer;
    s_count++;

    return (int8_t)(s_count - 1);
}

/**
 * @brief  重新开始计时（从满周期开始）
 */
void SOFTTIMER_Start(soft_timer_t *timer)
{
    if (timer == 0)
    {
        return;
    }

    timer->remain_ms = timer->period_ms;
    timer->running = 1;
}

/**
 * @brief  暂停计时（回调不再被调用）
 */
void SOFTTIMER_Stop(soft_timer_t *timer)
{
    if (timer != 0)
    {
        timer->running = 0;
    }
}

/**
 * @brief  改周期，并立刻按新周期重新计时
 */
void SOFTTIMER_SetPeriod(soft_timer_t *timer, uint16_t period_ms)
{
    if (timer == 0)
    {
        return;
    }

    timer->period_ms = (period_ms == 0u) ? 1u : period_ms;
    timer->remain_ms = timer->period_ms;
}

/**
 * @brief  是否正在计时
 */
uint8_t SOFTTIMER_IsRunning(const soft_timer_t *timer)
{
    if (timer == 0)
    {
        return 0;
    }
    return timer->running;
}

/**
 * @brief  1ms 心跳：把所有到期的任务跑一遍
 * @note   由 TIM6 更新中断（tick_irq.c）调用，应用层不用直接调
 */
void SOFTTIMER_Tick(void)
{
    uint8_t i;
    uint8_t n = s_count; /* 先取快照：回调里如果又注册，也不会越界 */
    soft_timer_t *t;

    for (i = 0; i < n; i++)
    {
        t = s_timers[i];
        if ((t == 0) || (t->running == 0))
        {
            continue;
        }

        if (t->remain_ms > 0u)
        {
            t->remain_ms--;
        }

        if (t->remain_ms == 0u)
        {
            t->remain_ms = t->period_ms; /* 先装下一周期，再跑回调 */
            if (t->fn != 0)
            {
                t->fn();
            }
        }
    }
}
