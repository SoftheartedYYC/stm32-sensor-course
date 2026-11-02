/**
 * @file    main.c
 * @brief   程序入口：初始化 + 主循环
 *
 * 各模块的初始化都挪到 main.h 的 Board_Init() 里了，这里只保留入口和主循环。
 *
 * 现在的演示流程：步进电机正转一圈 -> 反转一圈，循环往复。
 *   - 1.8°/步：一圈 200 步；400 步/秒 -> 约 0.5 秒一圈
 *   - 0.9°/步：一圈 400 步；400 步/秒 -> 约 1 秒一圈
 *   脉冲由 TIM7 中断发，stepper_move_deg() 只负责"定多少步、什么方向"，函数立刻返回。
 */
#include "main.h" /* 模块头文件 + Board_Init() */

/* 演示用的步频（步/秒）：400 对 1.8° 电机约 2 转/秒；太快会丢步，先从 400 试 */
#define STEP_DEMO_SPS   400u

/* 圈与圈之间停一下（ms），想连续转就改成 0 */
#define STEP_PAUSE_MS   300u

/**
 * @brief  每秒一次的采样（由 main.h 里的软定时器置标志）
 * @note   主循环空闲时、以及"等电机转完"的间隙都会调用，保证转电机时不丢采样
 */
static void app_sample_if_due(void)
{
    if (s_sample_due == 0u)
    {
        return;
    }
    s_sample_due = 0;

    /* TODO: 在这里读传感器 / 刷新显示：
     *   dht11_read(&humi, &temp);
     *   oled_draw_all(...);
     * 这里在主循环（线程上下文），可以慢、可以 delay */
}

/**
 * @brief  等电机把当前这段走完，期间照常处理采样
 */
static void wait_stepper_done(void)
{
    while (stepper_is_busy() != 0u)
    {
        app_sample_if_due();
    }
}

int main(void)
{
    Board_Init(); /* 一次性初始化所有模块，具体顺序见 main.h */

    /* 步进电机：先设速度再使能（失能状态下设置也生效，这里只是顺序好看） */
    stepper_set_speed(STEP_DEMO_SPS);
    stepper_enable(1);

    while (1)
    {
        /* ---- 正转一圈 ---- */
        stepper_move_deg(360);      /* 1.8°: 200 步，0.9°: 400 步；非阻塞，立刻返回 */
        wait_stepper_done();        /* 转完之前停在这里，但采样照常跑 */

        if (STEP_PAUSE_MS > 0u)
        {
            delay_ms(STEP_PAUSE_MS);
        }
        app_sample_if_due();

        /* ---- 反转一圈 ---- */
        stepper_move_deg(-360);     /* 负角度 = 反方向 */
        wait_stepper_done();

        if (STEP_PAUSE_MS > 0u)
        {
            delay_ms(STEP_PAUSE_MS);
        }
        app_sample_if_due();
    }
}
