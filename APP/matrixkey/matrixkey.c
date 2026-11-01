/**
 * @file    matrixkey.c
 * @brief   4×4 矩阵键盘驱动：行线 PF0~PF3，列线 PF4~PF7
 *
 * 扫描原理：
 *   行线平时全部输出高电平，列线内部上拉，所以没有按键时 4 根列线都是高。
 *   依次把一行拉低，再读 4 根列线：哪一根列线变低，就说明"这一行 × 这一列"
 *   交叉处的按键被按下。4 行扫完约 20us。
 *
 * 消抖与连按：
 *   第一次发现按键后等 10ms 再读一次，仍是低才认为有效；返回键值后置
 *   busy=1，按住不放期间只扫描、不重复返回；等到 4 行都读不到按键（松手）
 *   才清 busy，允许下一次按键。这样主循环不会因为按住不放反复执行 10ms 消抖。
 */

#include "matrixkey.h"
#include "delay.h"

/* 读 4 根列线：低 4 位有效，1 = 高电平（没按下），0 = 低电平（按下） */
static uint8_t MATRIXKEY_ReadCol(void)
{
    return (uint8_t)((GPIO_ReadInputData(MATRIX_PORT) >> MATRIX_COL_SHIFT) & 0x0Fu);
}

void MATRIXKEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(MATRIX_RCC, ENABLE);

    /* 先写高电平，再切成输出：避免初始化瞬间把 4 行同时拉低 */
    GPIO_SetBits(MATRIX_PORT, MATRIX_ROW_PINS);

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* 行线：推挽输出，平时全部输出高 */
    GPIO_InitStructure.GPIO_Pin = MATRIX_ROW_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(MATRIX_PORT, &GPIO_InitStructure);

    /* 列线：上拉输入，没按键时读到高，按下被行线拉低 */
    GPIO_InitStructure.GPIO_Pin = MATRIX_COL_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(MATRIX_PORT, &GPIO_InitStructure);

    GPIO_SetBits(MATRIX_PORT, MATRIX_ROW_PINS);
}

uint8_t MATRIXKEY_Scan(void)
{
    static uint8_t busy = 0; /* 1 = 上一次的键还按着，不重复返回 */
    uint8_t row;
    uint8_t col;
    uint8_t col_data;
    uint8_t key_pressed = 0;

    for (row = 0; row < 4; row++)
    {
        /* 先全高，再只把当前行拉低 */
        GPIO_SetBits(MATRIX_PORT, MATRIX_ROW_PINS);
        GPIO_ResetBits(MATRIX_PORT, (uint16_t)(1u << (MATRIX_ROW_SHIFT + row)));
        delay_us(5); /* 等列线电平稳定 */

        col_data = MATRIXKEY_ReadCol();
        if (col_data == 0x0Fu)
        {
            continue; /* 这一行没有键按下 */
        }

        key_pressed = 1;

        if (busy)
        {
            continue; /* 已经按住同一个键：只记录"还按着"，不重复返回 */
        }

        delay_ms(10); /* 消抖 */
        col_data = MATRIXKEY_ReadCol();
        if (col_data == 0x0Fu)
        {
            key_pressed = 0; /* 抖了一下又松开，不算 */
            continue;
        }

        for (col = 0; col < 4; col++)
        {
            if ((col_data & (1u << col)) == 0)
            {
                busy = 1;
                GPIO_SetBits(MATRIX_PORT, MATRIX_ROW_PINS);
                return (uint8_t)(row * 4 + col + 1);
            }
        }
    }

    GPIO_SetBits(MATRIX_PORT, MATRIX_ROW_PINS);

    if (!key_pressed)
    {
        busy = 0; /* 4 行都没有键，确认已经松手 */
    }
    return 0;
}
