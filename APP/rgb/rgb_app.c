/**
 * @file    rgb_app.c
 * @brief   RGB 三色灯驱动：PC6/PC7/PC8，TIM3_CH1/CH2/CH3 全重映射 PWM
 *
 * PWM 参数怎么来的：
 *   TIM3 挂在 APB1，定时器时钟 72MHz。
 *   预分频 PSC = 71 -> 计数时钟 72MHz / (71+1) = 1MHz；
 *   自动重装 ARR = 99 -> PWM 频率 1MHz / (99+1) = 10kHz，占空比正好 0~100 共 100 档。
 *   10kHz 远高于人眼能分辨的闪烁频率，亮度看起来是连续的。
 *
 * 共阳模块为什么要取反：
 *   共阳是"低电平点亮"，占空比越大反而越暗。所以写 CCR 时用 RGB_MAX - 亮度，
 *   亮度 0 -> CCR=100（引脚常高）-> 灭；亮度 100 -> CCR=0（引脚常低）-> 全亮。
 */

#include "rgb_app.h"
#include "delay.h"

#if (RGB_USE_PWM == 0)
/**
 * @brief  普通 GPIO 模式下把一路颜色写成亮/灭
 * @param  pin RGB_R_PIN / RGB_G_PIN / RGB_B_PIN
 * @param  on  1 = 点亮，0 = 熄灭
 */
static void rgb_write_pin(uint16_t pin, uint8_t on)
{
#if RGB_COMMON_ANODE
    if (on)
    {
        GPIO_ResetBits(RGB_PORT, pin); /* 共阳：低电平点亮 */
    }
    else
    {
        GPIO_SetBits(RGB_PORT, pin);
    }
#else
    if (on)
    {
        GPIO_SetBits(RGB_PORT, pin); /* 共阴：高电平点亮 */
    }
    else
    {
        GPIO_ResetBits(RGB_PORT, pin);
    }
#endif
}
#endif /* RGB_USE_PWM == 0 */

#if RGB_USE_PWM
/**
 * @brief  把 0~100 的亮度换算成比较值 CCR
 * @param  percent 亮度，0 = 灭，100 = 最亮
 * @return 要写进 CCR 的值（共阳时取反）
 */
static uint16_t rgb_duty(uint8_t percent)
{
    if (percent > RGB_MAX)
    {
        percent = RGB_MAX;
    }
#if RGB_COMMON_ANODE
    return (uint16_t)(RGB_MAX - percent);
#else
    return (uint16_t)percent;
#endif
}
#endif /* RGB_USE_PWM */

/**
 * @brief  初始化 RGB 三色灯
 * @note   PWM 模式下先算好"熄灭"的占空比，再使能定时器，
 *         这样共阳模块不会在上电瞬间闪一下白光。
 */
void rgb_init(void)
{
#if RGB_USE_PWM
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    uint16_t off_duty = rgb_duty(0); /* 先算熄灭值：共阴是 0，共阳是 100 */

    RCC_APB2PeriphClockCmd(RGB_GPIO_RCC | RCC_APB2Periph_AFIO, ENABLE); /* GPIO + 重映射都要 AFIO 时钟 */
    RCC_APB1PeriphClockCmd(RGB_PWM_RCC, ENABLE);                        /* TIM3 在 APB1 */

    /* TIM3 全重映射：CH1~CH4 从 PA6/PA7/PB0/PB1 改到 PC6/PC7/PC8/PC9 */
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);

    /* 三个颜色脚配成复用推挽：定时器直接接管引脚输出 */
    GPIO_InitStructure.GPIO_Pin = RGB_R_PIN | RGB_G_PIN | RGB_B_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RGB_PORT, &GPIO_InitStructure);

    /* 1MHz 计数时钟，ARR=99 -> 10kHz PWM，0~100 正好 100 档 */
    TIM_TimeBaseStructure.TIM_Period = RGB_MAX - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(RGB_PWM_TIM, &TIM_TimeBaseStructure);

    /* 三个通道统一用 PWM1 模式：CNT < CCR 时输出有效电平 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_Pulse = off_duty; /* 初始占空比 = 熄灭 */
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    TIM_OC1Init(RGB_PWM_TIM, &TIM_OCInitStructure);
    TIM_OC2Init(RGB_PWM_TIM, &TIM_OCInitStructure);
    TIM_OC3Init(RGB_PWM_TIM, &TIM_OCInitStructure);

    /* 打开 CCR 预装载：写 CCR 在下一个周期生效，不会切出半截波形 */
    TIM_OC1PreloadConfig(RGB_PWM_TIM, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(RGB_PWM_TIM, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(RGB_PWM_TIM, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(RGB_PWM_TIM, ENABLE);

    TIM_Cmd(RGB_PWM_TIM, ENABLE); /* 启动 PWM */
#else
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RGB_GPIO_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = RGB_R_PIN | RGB_G_PIN | RGB_B_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RGB_PORT, &GPIO_InitStructure);
#endif /* RGB_USE_PWM */

    rgb_off(); /* 上电先全灭，等主循环写状态 */

#if RGB_INIT_SELFTEST
    rgb_self_test(); /* 自检：红 -> 绿 -> 蓝 -> 白，确认模块和接线 */
#endif
}

/**
 * @brief  直接给三路亮度
 * @param  r 红色亮度 0~100
 * @param  g 绿色亮度 0~100
 * @param  b 蓝色亮度 0~100
 * @note   PWM 关闭时只判断"是否为 0"：非 0 就全亮，不能调亮度。
 */
void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
#if RGB_USE_PWM
    TIM_SetCompare1(RGB_PWM_TIM, rgb_duty(r));
    TIM_SetCompare2(RGB_PWM_TIM, rgb_duty(g));
    TIM_SetCompare3(RGB_PWM_TIM, rgb_duty(b));
#else
    rgb_write_pin(RGB_R_PIN, (uint8_t)(r > 0));
    rgb_write_pin(RGB_G_PIN, (uint8_t)(g > 0));
    rgb_write_pin(RGB_B_PIN, (uint8_t)(b > 0));
#endif
}

/**
 * @brief  用预定义颜色名点灯
 * @param  color RGB_RED / RGB_GREEN / ... / RGB_WHITE
 */
void rgb_show(rgb_color_t color)
{
    switch (color)
    {
    case RGB_RED:
        rgb_set(100, 0, 0);
        break;
    case RGB_GREEN:
        rgb_set(0, 100, 0);
        break;
    case RGB_BLUE:
        rgb_set(0, 0, 100);
        break;
    case RGB_YELLOW:
        rgb_set(100, 100, 0);
        break;
    case RGB_CYAN:
        rgb_set(0, 100, 100);
        break;
    case RGB_MAGENTA:
        rgb_set(100, 0, 100);
        break;
    case RGB_WHITE:
        rgb_set(100, 100, 100);
        break;
    case RGB_BLACK:
    default:
        rgb_set(0, 0, 0);
        break;
    }
}

/**
 * @brief  三色全灭
 */
void rgb_off(void)
{
    rgb_set(0, 0, 0);
}

/**
 * @brief  上电自检：红 -> 绿 -> 蓝 -> 白 各 500ms，最后熄灭
 * @note   阻塞约 2 秒；默认不自动调用，把 RGB_INIT_SELFTEST 改成 1 即可在 rgb_init() 里跑
 */
void rgb_self_test(void)
{
    rgb_show(RGB_RED);
    delay_ms(500);
    rgb_show(RGB_GREEN);
    delay_ms(500);
    rgb_show(RGB_BLUE);
    delay_ms(500);
    rgb_show(RGB_WHITE);
    delay_ms(500);
    rgb_off();
}
