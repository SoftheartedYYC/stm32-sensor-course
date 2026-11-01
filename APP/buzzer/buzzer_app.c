/**
 * @file    buzzer_app.c
 * @brief   蜂鸣器驱动（轮询版）：按 buzzer_cfg.h 里的类型自动选工作方式
 *
 * 有源（低/高电平触发）：引脚给电平就响，BEEP_On / BEEP_Off 直接搞定；
 * 无源：必须给方波，BEEP_Poll（或 BEEP_Square）每调一次翻转半个周期。
 * 两条分支由 BUZZER_NEED_SQUARE 在编译期选定，没用的那条不会编进程序。
 */
#include "buzzer_app.h"
#include "delay.h"

#if BUZZER_NEED_SQUARE
static volatile uint8_t s_on = 0;   /* 无源版：1 = 正在响，BEEP_Poll 才继续出方波 */
#endif

/** @brief  蜂鸣器引脚初始化：推挽输出，上电后静音
 *  @note   先把输出寄存器写成"静音"电平再切输出，避免初始化瞬间响一下；
 *          用推挽而不是开漏：无源蜂鸣器要"对称"的方波，开漏的高电平是悬空的
 */
void BEEP_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(BUZZER_GPIO_RCC, ENABLE);   /* 先给 GPIO 供上时钟 */

    BUZZER_OFF();                                      /* 先定静音电平，再切输出 */
#if BUZZER_NEED_SQUARE
    s_on = 0;
#endif

    GPIO_InitStructure.GPIO_Pin   = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;  /* 推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);
}

/** @brief  开始响
 *  @note   有源：给"响"的电平就完事；无源：置标志，等 BEEP_Poll 出方波
 */
void BEEP_On(void)
{
#if BUZZER_NEED_SQUARE
    s_on = 1;
#else
    BUZZER_ON();
#endif
}

/** @brief  静音：引脚回到"静音"电平
 */
void BEEP_Off(void)
{
#if BUZZER_NEED_SQUARE
    s_on = 0;
#endif
    BUZZER_OFF();
}

/** @brief  轮询出方波
 *  @note   无源：翻转一次 + 延时半个周期（约 185us），在 while 里反复调即可；
 *          有源：不需要任何操作，函数是空的，写成空函数是为了让两种类型的
 *                业务代码完全一样（无源的 BEEP_Poll 不用改就能直接编过）
 */
void BEEP_Poll(void)
{
#if BUZZER_NEED_SQUARE
    if (s_on == 0)
    {
        return;                       /* 静音：不动引脚，保持高电平 */
    }

    BUZZER_TOGGLE();                  /* 半个周期翻转一次 */
    delay_us(BUZZER_HALF_US);
#endif
}

/** @brief  输出半个周期的方波（兼容旧接口）
 *  @note   无源：翻转 + 延时，循环调用就是连续方波；
 *          有源：给"响"的电平，不需要翻转
 */
void BEEP_Square(void)
{
#if BUZZER_NEED_SQUARE
    BUZZER_TOGGLE();
    delay_us(BUZZER_HALF_US);
#else
    BUZZER_ON();
#endif
}

/** @brief  阻塞式响 ms 毫秒，返回时已经静音
 *  @param  ms 发声时长，单位 ms
 *  @note   无源：按"要翻转多少个半周期"循环出方波（会有几 us 的循环开销，
 *          实际时长比 ms 略长一点，做提示音完全够用）；
 *          有源：直接 delay_ms(ms)
 */
void BEEP_Beep(uint16_t ms)
{
    if (ms == 0)
    {
        return;
    }

    BEEP_On();

#if BUZZER_NEED_SQUARE
    {
        uint32_t halves = ((uint32_t)ms * 1000u) / BUZZER_HALF_US;  /* 需要翻转的次数 */

        if (halves == 0)
        {
            halves = 1;
        }

        while (halves-- != 0u)
        {
            BEEP_Poll();
        }
    }
#else
    delay_ms(ms);                     /* 有源：电平一直保持着，等时间到就行 */
#endif

    BEEP_Off();
}
