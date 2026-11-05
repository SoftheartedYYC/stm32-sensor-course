/**
 * @file    mpu6050_ui.c
 * @brief   MPU6050 的 OLED 显示：显存镜像 + 整页刷新
 *
 * 为什么要多一层显存（1KB RAM）：
 *   oled_show_char() 写一列像素就是一次完整的 I2C 事务（START + 地址 + 控制字 + 数据 + STOP），
 *   一个 8x16 字符要 19 次事务，刷满一屏 64 个字符就是 1200 多次，上百毫秒；
 *   这里先在 RAM 里把 8 页拼好，再用 oled_write_run() 一页 128 字节发一次，
 *   整屏只要 8 次事务（约 25~30ms），刷新才跟得上。
 *
 * 字库直接用 oled_font.h 里的 OLED_F8x16（8x16 ASCII），字形和工程里其他地方完全一致，
 * 所以 APP/oled 一个字节都不用改。
 */

#include "mpu6050_ui.h"
#include "oled_app.h"
#include "oled_font.h"

/* 显存镜像：8 页 x 128 列 */
static uint8_t s_fb[OLED_PAGES][OLED_WIDTH];

/* 每页有没有改动，只刷脏页 */
static uint8_t s_dirty[OLED_PAGES];

/* ------------------------------------------------------------------
 * 显存操作
 * ------------------------------------------------------------------ */

static void fb_char(uint8_t line, uint8_t col, char ch)
{
    uint8_t i;
    uint8_t idx;
    uint8_t page;
    uint8_t x;

    if (line < 1 || line > OLED_LINES || col < 1 || col > OLED_COLS)
    {
        return; /* 越界不画，避免写到别的行/别的地方去 */
    }

    if (ch < (char)OLED_ASCII_FIRST || ch > (char)OLED_ASCII_LAST)
    {
        ch = ' '; /* 字库里没有的字符（含中文 UTF-8 字节）统一显示成空格 */
    }

    idx = (uint8_t)(ch - OLED_ASCII_FIRST);
    page = (uint8_t)((line - 1) * 2); /* 一行 16 像素高 = 2 页 */
    x = (uint8_t)((col - 1) * 8);     /* 一列 8 像素宽 */

    for (i = 0; i < 8; i++)
    {
        s_fb[page][x + i] = OLED_F8x16[idx][i];       /* 上半页 */
        s_fb[page + 1][x + i] = OLED_F8x16[idx][i + 8]; /* 下半页 */
    }

    s_dirty[page] = 1;
    s_dirty[page + 1] = 1;
}

static void fb_string(uint8_t line, uint8_t col, const char *str)
{
    uint8_t i;

    if (str == 0)
    {
        return;
    }

    for (i = 0; str[i] != '\0'; i++)
    {
        fb_char(line, (uint8_t)(col + i), str[i]);
    }
}

/**
 * @brief  把一个"放大 10^dec 倍的定点数"按 width 个字符右对齐写上去
 * @param  v     定点数：dec=2 时 v=102 表示 1.02
 * @param  dec   小数位数 0~2
 * @param  width 占几个字符位（不够就左边补空格，超了截掉高位）
 * @note   故意不用 sprintf("%f")：那会牵进来几 KB 的浮点格式化代码
 */
static void fb_number(uint8_t line, uint8_t col, int32_t v, uint8_t dec, uint8_t width)
{
    char buf[12];
    uint8_t i;
    uint8_t pos;
    uint8_t neg = 0;
    uint32_t mag;

    if (width > 11)
    {
        width = 11;
    }

    if (v < 0)
    {
        neg = 1;
        mag = (uint32_t)(-(v + 1)) + 1u; /* 这样写，v = INT32_MIN 也不会溢出 */
    }
    else
    {
        mag = (uint32_t)v;
    }

    for (i = 0; i < width; i++)
    {
        buf[i] = ' ';
    }

    pos = width;
    if (dec > 0)
    {
        for (i = 0; i < dec; i++)
        {
            if (pos == 0)
            {
                break;
            }
            buf[--pos] = (char)('0' + (char)(mag % 10u));
            mag /= 10u;
        }
        if (pos > 0)
        {
            buf[--pos] = '.';
        }
    }

    do
    {
        if (pos == 0)
        {
            break;
        }
        buf[--pos] = (char)('0' + (char)(mag % 10u));
        mag /= 10u;
    } while (mag != 0u);

    if (neg != 0 && pos > 0)
    {
        buf[--pos] = '-';
    }

    for (i = 0; i < width; i++)
    {
        fb_char(line, (uint8_t)(col + i), buf[i]);
    }
}

/* 浮点 -> 定点，四舍五入（不要 roundf，省一层库函数调用） */
static int32_t to_fixed(float v, float scale)
{
    if (v >= 0.0f)
    {
        return (int32_t)(v * scale + 0.5f);
    }
    return (int32_t)(v * scale - 0.5f);
}

/* 把攒好的显存刷进屏幕：只发有改动的页 */
static void fb_flush(void)
{
    uint8_t page;

    for (page = 0; page < OLED_PAGES; page++)
    {
        if (s_dirty[page] != 0)
        {
            oled_write_run(page, 0, s_fb[page], OLED_WIDTH);
            s_dirty[page] = 0;
        }
    }
}

static void fb_clear(void)
{
    uint8_t page;
    uint8_t col;

    for (page = 0; page < OLED_PAGES; page++)
    {
        for (col = 0; col < OLED_WIDTH; col++)
        {
            s_fb[page][col] = 0x00;
        }
        s_dirty[page] = 1;
    }
}

/* ------------------------------------------------------------------
 * 对外接口
 * ------------------------------------------------------------------ */

void mpu6050_ui_clear(void)
{
    fb_clear();
    fb_flush();
}

void mpu6050_ui_show(const mpu6050_data_t *data)
{
    float pitch;
    float roll;
    float tilt;

    if (data == 0)
    {
        return;
    }

    /* 加速度 -> 角度：平放时三个都在 0 附近，一倾斜就成几十度地变，比看 g 值直观得多 */
    mpu6050_get_angles(data, &pitch, &roll, &tilt);

    /* 列分配固定，一行正好写满 16 列，既不会截断也不会留下上一次的残影：
     *   列 1       P / R / T
     *   列 2       空格
     *   列 3~8     角度，6 个字符、1 位小数（roll 到 ±180.0 也放得下）
     *   列 9~11    deg       —— 字库里没有 '°'(0xB0)，只能用字母写单位
     *   列 12~16   对应轴的加速度，5 个字符、2 位小数（-2.00 ~ 2.00）
     */
    fb_char(1, 1, 'P');
    fb_char(1, 2, ' ');
    fb_number(1, 3, to_fixed(pitch, 10.0f), 1, 6);
    fb_string(1, 9, "deg");
    fb_number(1, 12, to_fixed(data->accel_g.x, 100.0f), 2, 5);

    fb_char(2, 1, 'R');
    fb_char(2, 2, ' ');
    fb_number(2, 3, to_fixed(roll, 10.0f), 1, 6);
    fb_string(2, 9, "deg");
    fb_number(2, 12, to_fixed(data->accel_g.y, 100.0f), 2, 5);

    fb_char(3, 1, 'T');
    fb_char(3, 2, ' ');
    fb_number(3, 3, to_fixed(tilt, 10.0f), 1, 6);
    fb_string(3, 9, "deg");
    fb_number(3, 12, to_fixed(data->accel_g.z, 100.0f), 2, 5);

    /* 第 4 行：温度，顺手用来确认 I2C 真的读通了，右边补空格清残留 */
    fb_string(4, 1, "TEMP ");
    fb_number(4, 6, to_fixed(data->temp_c, 10.0f), 1, 5);
    fb_char(4, 11, 'C');
    fb_char(4, 12, ' ');
    fb_char(4, 13, ' ');
    fb_char(4, 14, ' ');
    fb_char(4, 15, ' ');
    fb_char(4, 16, ' ');

    fb_flush();
}

void mpu6050_ui_message(const char *line1, const char *line2)
{
    fb_clear();

    fb_string(2, 1, line1);
    fb_string(3, 1, line2);

    fb_flush();
}
