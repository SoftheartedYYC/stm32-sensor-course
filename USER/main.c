/**
 * @file    main.c
 * @brief   程序入口：初始化 + 主循环
 *
 * 各模块的初始化都挪到 main.h 的 Board_Init() 里了，这里只保留入口和主循环。
 *
 * 现在的流程：上电后步进电机正转一圈（360°）就停住不动。
 *   一圈脉冲数、细分、速度都由 stepper_app.h 里的 STEPPER_MOTOR 自动推导：
 *   1.8° -> 200 × 256 = 51200 脉冲/圈；0.9° -> 400 × 128 = 51200 脉冲/圈。
 *   STEPPER_REV_MS = 1000 表示约 1 秒转完；转完保持使能，电机锁在 360° 位置。
 *   （改驱动器细分，STEPPER_MICROSTEP 和 STEP_DEMO_SPS 要一起改）
 *   脉冲由 TIM7 中断发，stepper_move_deg() 只负责"定多少步、什么方向"，函数立刻返回。
 */
#include "main.h" /* 模块头文件 + Board_Init() */

/* 演示速度不用手填：由 stepper_app.h 的 STEPPER_REV_MS 自动算成"脉冲/秒"
 * 想改快慢，去改 STEPPER_REV_MS（1000 = 1 秒一圈，500 = 2 圈/秒） */
#define STEP_DEMO_SPS   STEPPER_SPEED_SPS

/* 每圈补偿的脉冲数：1.8°/256 细分时 1 个脉冲 = 0.00703°。
 *   实测"每转一圈都少 0.1°"（显示 359.9）时改成 1 试试；
 *   如果只是偶尔差、或者两圈误差不翻倍，就别动它（多半是屏幕 0.1° 分辨率/闭环还在收敛）。 */
#define STEP_TURN_TRIM  0

/* 一圈实际发多少个脉冲 = 理论值 + 补偿 */
#define STEP_TURN_PULSES (STEPPER_STEPS_PER_REV + (uint32_t)STEP_TURN_TRIM)

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
    delay_ms(200);              /* 给驱动器使能留点时间，别把第一个脉冲吞掉 */

    /* ---- 只转一圈（360°）就停 ---- */
    stepper_move_steps(STEP_TURN_PULSES); /* 3200 个脉冲（+STEP_TURN_TRIM 补偿） */
    wait_stepper_done();        /* 等它转完，期间照常处理采样 */
    delay_ms(300);              /* 停一下，等闭环把最后一点误差收敛，再看屏幕 */

    /* 转完停在这里：EN 保持使能，电机锁在 360° 的位置（用手转不动）。
     * 想松开电机就调 stepper_enable(0)；想再转一圈就在下面 while 里重新调用
     * stepper_move_deg(360) 即可。 */
    while (1)
    {
        app_sample_if_due();
    }
}
