/**
 * @file    main.c
 * @brief   程序入口：初始化 + 主循环
 *
 * 各模块的初始化都挪到 main.h 的 Board_Init() 里了，这里只保留入口和主循环。
 */
#include "main.h" /* 模块头文件 + Board_Init() */

int main(void)
{
    Board_Init(); /* 一次性初始化所有模块，具体顺序见 main.h */

    while (1)
    {
        /* 每秒一次的采样节拍（由 main.h 里的软定时器置标志） */
        if (s_sample_due != 0)
        {
            s_sample_due = 0;

            /* 在这里读传感器 / 刷新显示：
             *   dht11_read(&humi, &temp);
             *   oled_draw_all(...);
             * 注意别在 TIM6 中断里做，这里是主循环，可以慢，也可以 delay */
        }
    }
}
