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
        /* 在这里写你的功能 */
    }
}
