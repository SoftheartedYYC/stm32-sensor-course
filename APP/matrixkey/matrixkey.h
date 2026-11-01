/**
 * @file    matrixkey.h
 * @brief   4×4 矩阵键盘驱动：行线 PF0~PF3，列线 PF4~PF7
 *
 * 接线：
 *   行线 L1~L4 -> PF0~PF3（推挽输出，扫描时逐行拉低）
 *   列线 C1~C4 -> PF4~PF7（上拉输入，按下读到低电平）
 *
 * 键值编号：1~16，行优先：
 *   第 1 行（PF0 拉低）：1 2 3 4
 *   第 2 行（PF1 拉低）：5 6 7 8
 *   第 3 行（PF2 拉低）：9 10 11 12
 *   第 4 行（PF3 拉低）：13 14 15 16
 *
 * 如果键盘排线是反的（行接 PF4~PF7、列接 PF0~PF3），只需要把
 * MATRIX_ROW_SHIFT 改成 4、MATRIX_COL_SHIFT 改成 0，不用改扫描代码。
 */

#ifndef __MATRIXKEY_H
#define __MATRIXKEY_H

#include "stm32f10x.h" /* uint8_t / uint16_t / GPIOF */

/* ------------------------------------------------------------------
 * 接线：行列接反时只改这两个 SHIFT
 * ------------------------------------------------------------------ */
#define MATRIX_PORT       GPIOF
#define MATRIX_RCC        RCC_APB2Periph_GPIOF
#define MATRIX_ROW_SHIFT  0    /* 行线从 PF0 开始 */
#define MATRIX_COL_SHIFT  4    /* 列线从 PF4 开始 */

#define MATRIX_ROW_PINS   ((uint16_t)(0x000Fu << MATRIX_ROW_SHIFT)) /* PF0~PF3 */
#define MATRIX_COL_PINS   ((uint16_t)(0x000Fu << MATRIX_COL_SHIFT)) /* PF4~PF7 */

/* 初始化 PF0~PF3 推挽输出、PF4~PF7 上拉输入 */
void MATRIXKEY_Init(void);

/* 扫描一次：返回 1~16，没有键按下返回 0；按住不放不会重复返回 */
uint8_t MATRIXKEY_Scan(void);

#endif /* __MATRIXKEY_H */
