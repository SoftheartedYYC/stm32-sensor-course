/**
 * @file    stepper_app.c
 * @brief   步进电机驱动：PE12=EN、PE13=STP、PE14=DIR，TIM7 中断发脉冲
 *
 * 脉冲怎么来：
 *   TIM7 挂 APB1、时钟 72MHz；预分频 71 -> 计数 1MHz（1 个计数 = 1us）；
 *   自动重装值 = 半个脉冲周期 - 1，所以每半个周期进一次更新中断：
 *   中断里把 STP 翻转一次 -> 高、低各半个周期 -> 一个完整脉冲走一步。
 *   例如 400 步/秒：半个周期 1250us，高 1250us、低 1250us。
 *
 * 为什么用中断而不是主循环翻转：
 *   步进电机的转速和脉冲数都由定时器决定，主循环去刷 OLED、读传感器都不会影响步频，
 *   也不会因为别的地方加了 delay 就丢步。主循环只管"走多少步 / 停"。
 *
 * 引脚都在 GPIOE，初始化时一次配完；如果以后把某个脚挪到别的端口，
 * 需要把 stepper_init 里的 GPIO_Init 拆成两个。
 */

#include "stepper_app.h"

static volatile uint8_t  s_busy = 0;        /* 1 = 正在发脉冲 */
static volatile uint8_t  s_stp_level = 0;   /* 当前 STP 电平 */
static volatile uint8_t  s_continuous = 0;  /* 1 = 连续转，不数步数 */
static volatile uint32_t s_remain = 0;      /* 还剩多少步（定长模式） */
static volatile uint32_t s_sps = STEPPER_DEFAULT_SPS;

/* ---------------------------------------------------------------- 引脚 */

static void stepper_set_stp(uint8_t level)
{
    if (level)
    {
        GPIO_SetBits(STEPPER_STP_PORT, STEPPER_STP_PIN);
    }
    else
    {
        GPIO_ResetBits(STEPPER_STP_PORT, STEPPER_STP_PIN);
    }
}

/* 按 STEPPER_EN_ACTIVE_LEVEL 输出使能电平 */
static void stepper_apply_en(uint8_t on)
{
#if STEPPER_EN_ACTIVE_LEVEL
    if (on)
    {
        GPIO_SetBits(STEPPER_EN_PORT, STEPPER_EN_PIN);
    }
    else
    {
        GPIO_ResetBits(STEPPER_EN_PORT, STEPPER_EN_PIN);
    }
#else
    if (on)
    {
        GPIO_ResetBits(STEPPER_EN_PORT, STEPPER_EN_PIN); /* 低电平使能 */
    }
    else
    {
        GPIO_SetBits(STEPPER_EN_PORT, STEPPER_EN_PIN);
    }
#endif
}

/* 按 STEPPER_DIR_CW_LEVEL 输出方向电平 */
static void stepper_apply_dir(uint8_t dir)
{
    uint8_t level;

    if (dir == STEPPER_DIR_CW)
    {
        level = STEPPER_DIR_CW_LEVEL;
    }
    else
    {
        level = (uint8_t)(STEPPER_DIR_CW_LEVEL ? 0 : 1);
    }

    if (level)
    {
        GPIO_SetBits(STEPPER_DIR_PORT, STEPPER_DIR_PIN);
    }
    else
    {
        GPIO_ResetBits(STEPPER_DIR_PORT, STEPPER_DIR_PIN);
    }
}

/* ---------------------------------------------------------------- 定时器 */

/* 步频 -> 半个周期的微秒数（限制在 5us ~ 65535us，太窄驱动器认不到脉冲） */
static uint16_t stepper_half_us(uint32_t sps)
{
    uint32_t half;

    if (sps < STEPPER_SPS_MIN)
    {
        sps = STEPPER_SPS_MIN;
    }
    if (sps > STEPPER_SPS_MAX)
    {
        sps = STEPPER_SPS_MAX;
    }

    half = 500000uL / sps;
    if (half < 5u)
    {
        half = 5u;
    }
    if (half > 65535u)
    {
        half = 65535u;
    }
    return (uint16_t)half;
}

/* 停止发脉冲：关定时器、STP 拉低、状态清零 */
static void stepper_stop_pulses(void)
{
    TIM_Cmd(STEPPER_TIM, DISABLE);
    stepper_set_stp(0);

    s_busy = 0;
    s_stp_level = 0;
    s_remain = 0;
    s_continuous = 0;
}

/* 开始发脉冲：从低电平起步，按当前步频装重装值 */
static void stepper_start_pulses(void)
{
    uint16_t half = stepper_half_us(s_sps);

    stepper_set_stp(0);
    s_stp_level = 0;

    TIM_SetCounter(STEPPER_TIM, 0);
    TIM_SetAutoreload(STEPPER_TIM, (uint16_t)(half - 1u));
    TIM_Cmd(STEPPER_TIM, ENABLE);
}

/* ---------------------------------------------------------------- 对外接口 */

/**
 * @brief  初始化 PE12/PE13/PE14 和 TIM7，初始化完电机失能、不转
 */
void stepper_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef tb;
    NVIC_InitTypeDef nvic;
    uint16_t half;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
    RCC_APB1PeriphClockCmd(STEPPER_TIM_RCC, ENABLE);

    /* 先写"失能 / 顺时针 / 无脉冲"的电平，再切成输出，避免上电抖一下 */
    stepper_apply_en(0);
    stepper_apply_dir(STEPPER_DIR_CW);
    stepper_set_stp(0);

    GPIO_InitStructure.GPIO_Pin = STEPPER_EN_PIN | STEPPER_STP_PIN | STEPPER_DIR_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STEPPER_EN_PORT, &GPIO_InitStructure);

    /* 72MHz / (71+1) = 1MHz，1 个计数 = 1us；ARR = 半个脉冲周期 */
    half = stepper_half_us(s_sps);
    TIM_TimeBaseStructInit(&tb);
    tb.TIM_Prescaler = 72 - 1;
    tb.TIM_Period = (uint16_t)(half - 1u);
    tb.TIM_ClockDivision = TIM_CKD_DIV1;
    tb.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(STEPPER_TIM, &tb);

    TIM_ClearFlag(STEPPER_TIM, TIM_FLAG_Update);
    TIM_ITConfig(STEPPER_TIM, TIM_IT_Update, ENABLE);

    /* 比数码管扫描（TIM4，抢占 1）低一档：晚几微秒没关系，别抢屏幕 */
    nvic.NVIC_IRQChannel = STEPPER_TIM_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    stepper_stop_pulses();
}

/**
 * @brief  使能/失能电机；失能会立刻停止发脉冲
 */
void stepper_enable(uint8_t on)
{
    stepper_apply_en(on ? 1 : 0);

    if (!on)
    {
        stepper_stop_pulses();
    }
}

/**
 * @brief  设置方向（只改 DIR 电平，不影响正在走的脉冲）
 */
void stepper_set_dir(uint8_t dir)
{
    stepper_apply_dir(dir);
}

/**
 * @brief  设置步频（步/秒）；会限制在 STEPPER_SPS_MIN ~ STEPPER_SPS_MAX，运行中也能改
 */
void stepper_set_speed(uint32_t steps_per_sec)
{
    if (steps_per_sec < STEPPER_SPS_MIN)
    {
        steps_per_sec = STEPPER_SPS_MIN;
    }
    if (steps_per_sec > STEPPER_SPS_MAX)
    {
        steps_per_sec = STEPPER_SPS_MAX;
    }

    s_sps = steps_per_sec;

    if (s_busy)
    {
        TIM_SetAutoreload(STEPPER_TIM, (uint16_t)(stepper_half_us(s_sps) - 1u));
    }
}

/**
 * @brief  读当前步频（步/秒）
 */
uint32_t stepper_get_speed(void)
{
    return s_sps;
}

/**
 * @brief  按当前方向走 steps 步（非阻塞）
 * @note   走的过程中可以忙别的；走完 s_busy 自动变 0，脉冲停在低电平
 */
void stepper_move_steps(uint32_t steps)
{
    if (steps == 0)
    {
        return;
    }

    stepper_stop_pulses(); /* 先停掉上一段，重新计数 */

    s_remain = steps;
    s_continuous = 0;
    s_busy = 1;

    stepper_start_pulses();
}

/**
 * @brief  走指定角度（非阻塞）：正数顺时针、负数逆时针
 * @note   0.9°/步时 90° = 100 步，1.8°/步时 90° = 50 步；四舍五入到最近一步
 */
void stepper_move_deg(int16_t degrees)
{
    uint32_t abs_deg = (uint32_t)((degrees < 0) ? -(int32_t)degrees : (int32_t)degrees);
    uint32_t steps;

    steps = (abs_deg * STEPPER_STEPS_PER_REV + 180u) / 360u;
    if (steps == 0)
    {
        return;
    }

    stepper_set_dir((degrees >= 0) ? STEPPER_DIR_CW : STEPPER_DIR_CCW);
    stepper_move_steps(steps);
}

/**
 * @brief  连续转，直到 stepper_stop()
 */
void stepper_run(uint8_t dir)
{
    stepper_set_dir(dir);
    stepper_stop_pulses();

    s_continuous = 1;
    s_remain = 0;
    s_busy = 1;

    stepper_start_pulses();
}

/**
 * @brief  立刻停止发脉冲
 */
void stepper_stop(void)
{
    stepper_stop_pulses();
}

/**
 * @brief  是否还在走：1 = 走，0 = 停
 */
uint8_t stepper_is_busy(void)
{
    return s_busy;
}

/**
 * @brief  一圈多少步：1.8° -> 200，0.9° -> 400
 */
uint32_t stepper_steps_per_rev(void)
{
    return STEPPER_STEPS_PER_REV;
}

/* ---------------------------------------------------------------- 中断 */

/**
 * @brief  TIM7 更新中断：半个周期翻转一次 STP，顺便数剩余步数
 * @note   中断名固定 TIM7_IRQHandler，只能在工程里定义一次（本文件定义）
 */
void TIM7_IRQHandler(void)
{
    if (TIM_GetITStatus(STEPPER_TIM, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(STEPPER_TIM, TIM_IT_Update);

        if (s_busy == 0)
        {
            return; /* 已经停了：可能是没处理完的挂起中断 */
        }

        if (s_stp_level == 0)
        {
            stepper_set_stp(1); /* 上升沿：驱动器在这一下走一步 */
            s_stp_level = 1;
        }
        else
        {
            stepper_set_stp(0); /* 下降沿：一个完整脉冲结束 */
            s_stp_level = 0;

            if (s_continuous == 0)
            {
                if (s_remain > 0)
                {
                    s_remain--;
                }
                if (s_remain == 0)
                {
                    stepper_stop_pulses(); /* 步数走完，自己停 */
                }
            }
        }
    }
}
