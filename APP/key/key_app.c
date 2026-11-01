/**
 * @file    key_app.c
 * @brief   四个按键的扫描与消抖：PE0~PE3，上拉输入，按下为低电平
 *
 * 消抖为什么这么做：
 *   机械按键按下/松开的一瞬间触点会弹跳，电平在几毫秒内可能来回变好几次。
 *   这里每 10ms 采一次，连续 5 次（50ms）都读到低电平才算真的按下，
 *   中间只要有一次读到高电平就把计数清零。50ms 远小于人的反应时间，
 *   手感和"一按就响应"没区别，但不会出现按一次跳好几个数的问题。
 *
 * 连发（长按）：
 *   光敏阈值 0~95、温度阈值 0~50°C、湿度阈值 20~90%RH，一档一档按太累。
 *   按住 500ms 后，每 100ms 再报一次同一个键，并带上 KEY_EVENT_REPEAT 标志，
 *   主程序看到这个标志就知道该继续加/减，而不是重新"按了一下"。
 */

#include "key_app.h"

#define KEY_DEBOUNCE_TICKS 5  /* 5 次 x 10ms = 50ms 消抖 */
#define KEY_HOLD_TICKS     50 /* 按住 50 次（500ms）后开始连发 */
#define KEY_REPEAT_TICKS   10 /* 连发间隔 10 次（100ms） */

/* 键号 -> 引脚，顺序和 KEY_ID_x 对应 */
static const uint16_t key_pins[4] = {KEY0_PIN, KEY1_PIN, KEY2_PIN, KEY3_PIN};

static uint8_t key_count[4];  /* 连续读到低电平的次数（消抖用） */
static uint8_t key_hold[4];   /* 消抖之后又按住了多久（连发用） */

/**
 * @brief  初始化 PE0~PE3 为上拉输入
 * @note   四个键模式一样，可以一次性配置；不用外部上拉电阻。
 */
void key_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); /* GPIOE 挂在 APB2 */

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;       /* 上拉输入：松开=1，按下=0 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   /* 输入模式下速度位无影响 */
    GPIO_InitStructure.GPIO_Pin = KEY0_PIN | KEY1_PIN | KEY2_PIN | KEY3_PIN;
    GPIO_Init(KEY_PORT, &GPIO_InitStructure);
}

/**
 * @brief  扫描四个按键
 * @retval KEY_NONE 或 (键号 | 连发标志)
 * @note   每个键独立计数，所以"同时按两个"也能分别识别；
 *         一次调用最多返回一个事件，另一个会在下一拍（10ms 后）返回。
 */
uint8_t key_scan(void)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        if (GPIO_ReadInputDataBit(KEY_PORT, key_pins[i]) == Bit_RESET) /* 低电平 = 按下 */
        {
            if (key_count[i] < KEY_DEBOUNCE_TICKS)
            {
                key_count[i]++;

                if (key_count[i] == KEY_DEBOUNCE_TICKS) /* 刚好稳定 50ms：报第一次 */
                {
                    key_hold[i] = 0;
                    return (uint8_t)(i + KEY_ID_0);
                }
            }
            else
            {
                key_hold[i]++;

                if (key_hold[i] >= KEY_HOLD_TICKS) /* 按住够久，开始连发 */
                {
                    /* 回退一个连发间隔，下一拍正好再报一次 */
                    key_hold[i] = (uint8_t)(KEY_HOLD_TICKS - KEY_REPEAT_TICKS);
                    return (uint8_t)((i + KEY_ID_0) | KEY_EVENT_REPEAT);
                }
            }
        }
        else /* 松开了：两个计数都清零，下次按下重新消抖 */
        {
            key_count[i] = 0;
            key_hold[i] = 0;
        }
    }

    return KEY_NONE;
}
