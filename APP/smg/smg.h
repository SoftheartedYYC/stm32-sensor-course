/**
 * @file    smg.h
 * @brief   4 位数码管驱动：段选 PD0~PD7，位选 PG9~PG12
 *
 * 接线：
 *   段选 A B C D E F G DP -> PD0 PD1 PD2 PD3 PD4 PD5 PD6 PD7（推挽输出）
 *   位选 D1 D2 D3 D4       -> PG9 PG10 PG11 PG12
 *
 * 显示方式：
 *   4 位共用一组段码线，必须动态扫描。本驱动用 TIM4 每 1ms 中断扫一位，
 *   4ms 扫完一轮（250Hz）。主循环只负责往显示缓冲区写内容，不会被 OLED
 *   刷新或 DHT11 读取的阻塞拖慢，所以不会出现"只亮一位/一位特别亮"。
 *
 * 接线/极性宏（接反了显示会乱，先看这里）：
 *   SMG_COMMON_ANODE     1 = 共阳（段码低电平点亮）；0 = 共阴（高电平点亮）
 *   SMG_DIG_ACTIVE_HIGH  1 = 位选高电平选通；0 = 低电平选通（PNP 三极管驱动）
 *   SMG_DIG_REVERSE      0 = PG9~PG12 对应从左到右 D1~D4；1 = 反过来
 *   SMG_DIG_OUT_OD       0 = 位选推挽；1 = 位选开漏（PNP 基极上拉到 5V 时用）
 *   SMG_INIT_SELFTEST    1 = 上电逐位自检（约 1.5s）；0 = 直接进入正常显示
 *
 * 用法：
 *   SMG_Init();
 *   SMG_ShowTemp(253);   // 显示 25.3
 *   SMG_ShowNum(1234);   // 显示 1234
 *   SMG_ShowDashes();    // 显示 ----
 *   SMG_Clear();         // 全灭
 */

#ifndef __SMG_H
#define __SMG_H

#include "stm32f10x.h" /* uint8_t / GPIO_TypeDef / TIM4 */

/* ------------------------------------------------------------------
 * 段选：PD0~PD7（A~DP）
 * ------------------------------------------------------------------ */
#define SMG_SEG_PORT   GPIOD
#define SMG_SEG_RCC    RCC_APB2Periph_GPIOD
#define SMG_SEG_SHIFT  0
#define SMG_SEG_MASK   ((uint16_t)(0x00FFu << SMG_SEG_SHIFT))

/* ------------------------------------------------------------------
 * 位选：PG9~PG12（D1~D4）
 * ------------------------------------------------------------------ */
#define SMG_DIG_PORT   GPIOG
#define SMG_DIG_RCC    RCC_APB2Periph_GPIOG
#define SMG_DIG_SHIFT  9
#define SMG_DIG_MASK   ((uint16_t)(0x000Fu << SMG_DIG_SHIFT)) /* 0x1E00 */

/* ------------------------------------------------------------------
 * 显示参数
 * ------------------------------------------------------------------ */
#define SMG_DIGIT_NUM       4    /* 4 位 */
#define SMG_COMMON_ANODE    1    /* 1 = 共阳（低电平点亮），0 = 共阴 */
#define SMG_DIG_ACTIVE_HIGH 0    /* 0 = 低电平选通（PNP 驱动），1 = 高电平选通 */
#define SMG_DIG_REVERSE     0    /* 0 = PG9 是最左位；1 = PG12 是最左位 */
#define SMG_DIG_OUT_OD      0    /* 0 = 推挽；1 = 开漏（位选经 PNP、基极上拉到 5V 时用） */
#define SMG_GHOST_US        10   /* 换位前的消影等待（us），有串影可加到 20~50 */
#define SMG_INIT_SELFTEST   0    /* 1 = 上电逐位自检；0 = 不耽搁开机 */
#define SMG_SELFTEST_MS     300  /* 自检时每位停留时间（ms） */

/* 字形索引：0~9 直接对应数字，10 是负号，SMG_BLANK 表示该位不显示 */
#define SMG_MINUS     10
#define SMG_GLYPH_NUM 11
#define SMG_BLANK     0xFFu

/* 初始化 GPIO + TIM4 扫描中断，调用前必须先 delay_init() */
void SMG_Init(void);

/* 改某一位的内容：pos = 0~3（左到右），num = 0~9 / SMG_MINUS / SMG_BLANK */
void SMG_ShowOne(uint8_t pos, uint8_t num);

/* 兼容旧接口：把 4 位内容写进缓冲区（buf[0]~buf[3] = 左到右） */
void SMG_Display(const uint8_t *buf);

/* 显示 0~9999 的整数，右对齐，前导位不亮；超范围显示 "----" */
void SMG_ShowNum(uint16_t value);

/* 显示温度：temp_x10 是 0.1°C 为单位的温度，如 253 -> "25.3" */
void SMG_ShowTemp(int16_t temp_x10);

/* 4 位全显示 "-"，用作传感器读失败的占位 */
void SMG_ShowDashes(void);

/* 4 位全灭 */
void SMG_Clear(void);

/* 逐位点亮自检（不影响正常显示，调用后会阻塞约 1.5s） */
void SMG_SelfTest(void);

#endif /* __SMG_H */
