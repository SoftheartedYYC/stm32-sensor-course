/**
 * @file    oled_font.h
 * @brief   OLED 字库声明：8x16 ASCII + 16x16 中文（自动生成，勿手改）
 *
 * 本文件由 tools/gen_oled_font.py 自动生成，请不要手工修改；
 * 要加字/换字体请改脚本顶部的 CN_CHARS 与 TTF，然后重新运行脚本。
 *
 * 字模格式（SSD1306 页寻址，列行式）：
 *   8x16  ASCII 字模 = 16 字节：上半页 8 字节 + 下半页 8 字节；
 *   16x16 中文字模  = 32 字节：上半页 16 字节 + 下半页 16 字节；
 *   每字节对应 1 个像素列，bit0 是该列最上面那个像素，bit7 是最下面那个。
 */

#ifndef __OLED_FONT_H
#define __OLED_FONT_H

#include "stm32f10x.h" /* 提供 uint8_t */

/* ---- 8x16 ASCII 字库：覆盖 0x20(' ') ~ 0x7E('~') ---- */
#define OLED_ASCII_FIRST 0x20 /* 字库第一个字符的 ASCII 码 */
#define OLED_ASCII_LAST  0x7E /* 字库最后一个字符的 ASCII 码 */
#define OLED_ASCII_NUM   (OLED_ASCII_LAST - OLED_ASCII_FIRST + 1) /* 字模个数 = 95 */

/* ---- 16x16 中文字库：索引即 OLED_CF16x16[] 的行号 ---- */
enum
{
    OLED_CN_GUANG  = 0, /* 光 */
    OLED_CN_MIN    = 1, /* 敏 */
    OLED_CN_DIAN   = 2, /* 电 */
    OLED_CN_ZU     = 3, /* 阻 */
    OLED_CN_ZHI    = 4, /* 值 */
    OLED_CN_LIANG  = 5, /* 亮 */
    OLED_CN_AN     = 6, /* 暗 */
    OLED_CN_WEN    = 7, /* 温 */
    OLED_CN_DU     = 8, /* 度 */
    OLED_CN_MO     = 9, /* 模 */
    OLED_CN_NI     = 10, /* 拟 */
    OLED_CN_LIANG2 = 11, /* 量 */
    OLED_CN_KAI    = 12, /* 开 */
    OLED_CN_GUAN   = 13, /* 关 */
    OLED_CN_FU     = 14, /* 伏 */
    OLED_CN_YA     = 15, /* 压 */
    OLED_CN_ZHAO   = 16, /* 照 */
    OLED_CN_SHE    = 17, /* 摄 */
    OLED_CN_SHI    = 18, /* 氏 */
    OLED_CN_DEG    = 19, /* ℃ */
    OLED_CN_YE     = 20, /* 叶 */
    OLED_CN_YU     = 21, /* 雨 */
    OLED_CN_CHEN   = 22, /* 晨 */
    OLED_CN_FEN    = 23, /* 分 */
    OLED_CN_YU2    = 24, /* 阈 */
    OLED_CN_CHAO   = 25, /* 超 */
    OLED_CN_BIAO   = 26, /* 标 */
    OLED_CN_SHI2   = 27, /* 湿 */

    OLED_CN_NUM /* 中文字模总数，仅用于边界检查 */
};

extern const uint8_t OLED_F8x16[OLED_ASCII_NUM][16]; /* 8 宽 16 高 ASCII 字模，16 字节/字 */
extern const uint8_t OLED_CF16x16[OLED_CN_NUM][32];   /* 16 宽 16 高中文字模，32 字节/字 */

#endif /* __OLED_FONT_H */
