/**
 * @file    main.c
 * @brief   程序入口：初始化 + 主循环
 *
 * 各模块的初始化都挪到 main.h 的 Board_Init() 里了，这里只保留入口和主循环。
 *
 * 现在的流程：上电后步进电机正转一圈（360°）就停住不动。
 *   1.8° 电机 + Emm42 细分 16：一圈 = 200 × 16 = 3200 个脉冲。
 *   3200 步/秒 -> 约 1 秒转完；转完保持使能，电机锁在 360° 位置。
 *   （改驱动器细分，STEPPER_MICROSTEP 和 STEP_DEMO_SPS 要一起改）
 *   脉冲由 TIM7 中断发，stepper_move_deg() 只负责"定多少步、什么方向"，函数立刻返回。
 */
#include "main.h" /* 模块头文件 + Board_Init() */

/* 演示用的步频（步/秒）：1.8° + 细分 16 时一圈 3200 步
 * 3200 步/秒 = 约 1 秒转完（上限 20000，约 6 圈/秒）；
 * 注意这是"脉冲数/秒"，不是"整步/秒" */
#define STEP_DEMO_SPS   3200u

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

    /* ---- 只转一圈（360°）就停 ---- */
    stepper_move_deg(360);      /* 1.8° + 细分 16 -> 200 x 16 = 3200 个脉冲 */
    wait_stepper_done();        /* 等它转完，期间照常处理采样 */

    /* 转完停在这里：EN 保持使能，电机锁在 360° 的位置（用手转不动）。
     * 想松开电机就调 stepper_enable(0)；想再转一圈就在下面 while 里重新调用
     * stepper_move_deg(360) 即可。 */
    while (1)
    {
        app_sample_if_due();
    }
}
