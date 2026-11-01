/**
 * @file    oled_app.h
 * @brief   0.96 寸 OLED（SSD1306，128x64）驱动接口 —— 软件 I2C，SDA=PB6、SCL=PB7
 *
 * 坐标约定（和课程示例一致，方便照抄示例代码）：
 *   line 行号 1~4，col 列号 1~16；原点在左上角，向下为行、向右为列。
 *   中文字模是 16x16 的，占 2 个 ASCII 列宽（col 和 col+1）。
 */

#ifndef __OLED_APP_H /* 头文件保护宏：没定义过才进入正文 */
#define __OLED_APP_H /* 定义保护宏，第二次包含时直接跳到 #endif */

#include "stm32f10x.h" /* 标准外设库：uint8_t / GPIO / RCC */
#include "oled_font.h" /* 字库：8x16 ASCII + 16x16 中文，以及 OLED_CN_* 索引 */

/* ------------------------------------------------------------------
 * 接线：换引脚只改这 4 行（端口 + 引脚掩码），其余代码不用动
 * ------------------------------------------------------------------ */
#define OLED_SDA_PORT GPIOB      /* SDA 所在端口 */
#define OLED_SDA_PIN  GPIO_Pin_6 /* SDA = PB6 */
#define OLED_SCL_PORT GPIOB      /* SCL 所在端口 */
#define OLED_SCL_PIN  GPIO_Pin_7 /* SCL = PB7 */

/* SSD1306 的 I2C 从机地址（8 位写地址）：
 * 模块背面把 SA0 接地或悬空 -> 0x78；把 SA0 接 VCC -> 0x7A。
 * 市面上绝大多数 0.96" 模块都是 0x78。 */
#define OLED_I2C_ADDR 0x78

/* ------------------------------------------------------------------
 * 屏幕几何参数
 * ------------------------------------------------------------------ */
#define OLED_WIDTH  128                  /* 横向 128 像素 */
#define OLED_HEIGHT 64                   /* 纵向 64 像素 */
#define OLED_PAGES  (OLED_HEIGHT / 8)    /* SSD1306 按"页"寻址，每页 8 行，共 8 页 */
#define OLED_LINES  4                    /* 8x16 字体一屏 4 行 */
#define OLED_COLS   16                   /* 8x16 字体一行 16 个字符 */

/* ------------------------------------------------------------------
 * 对外接口
 * ------------------------------------------------------------------ */

/* 初始化 OLED：配置 PB6/PB7 并写入 SSD1306 的上电配置序列，最后清屏。
 * 自带上电延时，不依赖 delay_init()，可以放在 main 里最先调用。
 * 注意：初始化之前可以用 oled_set_addr() 换从机地址。 */
void oled_init(void);

/* 运行时改从机地址（8 位写地址，如 0x78 / 0x7A）
 * 默认用上面的 OLED_I2C_ADDR；有些模块背面把 SA0 接了 VCC，地址是 0x7A，
 * 这种情况在 main.c 里探测出来后调用本函数即可，不用改头文件重编译。 */
void oled_set_addr(uint8_t addr8);

/* 整屏清空（写 0x00），初始化后和需要重画整屏时调用 */
void oled_clear(void);

/* 在 (line, col) 显示一个 ASCII 字符，范围 0x20(' ') ~ 0x7E('~') */
void oled_show_char(uint8_t line, uint8_t col, char ch);

/* 在 (line, col) 起显示一个以 '\0' 结尾的字符串 */
void oled_show_string(uint8_t line, uint8_t col, const char *str);

/* 在 (line, col) 起显示定长十进制数：高位补 0，只取低 len 位。
 * 用"定长"是为了刷新时不用擦除旧内容 —— 长度不变就不会有残影。 */
void oled_show_num(uint8_t line, uint8_t col, uint32_t num, uint8_t len);

/* 在 (line, col) 起显示一个 16x16 中文字，索引取 oled_font.h 里的 OLED_CN_xxx，
 * 占 2 个 ASCII 列宽（col 与 col+1） */
void oled_show_cn(uint8_t line, uint8_t col, uint8_t index);

/* 从第 page 页、第 col 列开始，连续写入 len 个显存字节（len 最大 128）。
 * 一整行只发一次 I2C 事务（地址 + 0x40 + 128 个数据），比逐字节写快一个数量级，
 * 给"整行刷新的滚动字幕"用；data 里的每一字节仍然对应一列 8 个像素。 */
void oled_write_run(uint8_t page, uint8_t col, const uint8_t *data, uint8_t len);

#endif /* __OLED_APP_H */
