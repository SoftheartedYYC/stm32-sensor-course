/**
 * @file    matrix_irq.h
 * @brief   4×4 矩阵键盘（中断版）：TIM6 1ms 扫描，行 PF0~PF3、列 PF4~PF7
 *
 * 和 APP/matrixkey 的轮询版区别：
 *   轮询版由主循环调 MATRIXKEY_Scan()，而且一检测到按键就在函数里 delay_ms(10)
 *   消抖，主循环会被拖一下；
 *   中断版把"每 1ms 扫一行"放进 TIM6 中断，4ms 扫完一轮，
 *   连续 3 轮（约 12ms）稳定才认按下，主循环只从队列取键值。
 *
 * 键值编号和轮询版一致：1~16，行优先。
 */
#ifndef __MATRIX_IRQ_H
#define __MATRIX_IRQ_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 接线：行 PF0~PF3（推挽输出）、列 PF4~PF7（上拉输入）
 * 行列接反时把两个 SHIFT 对调（4 / 0）
 * ------------------------------------------------------------------ */
#define MKEY_PORT       GPIOF
#define MKEY_RCC        RCC_APB2Periph_GPIOF
#define MKEY_ROW_SHIFT  0
#define MKEY_COL_SHIFT  4

#define MKEY_ROW_PINS   ((uint16_t)(0x000Fu << MKEY_ROW_SHIFT))
#define MKEY_COL_PINS   ((uint16_t)(0x000Fu << MKEY_COL_SHIFT))

#define MKEY_STABLE_CYCLES 3 /* 连续 3 轮（4ms/轮）稳定才认，约 12~16ms 消抖 */

/* 初始化：配 4 行输出、4 列输入，先选中第 0 行，并挂上 1ms 钩子 */
void MatrixIRQ_Init(void);

/* 取一个键值：1~16，没有按键返回 0。按住不放只返回一次 */
uint8_t MatrixIRQ_Scan(void);

#endif /* __MATRIX_IRQ_H */
