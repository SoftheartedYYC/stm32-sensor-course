/**
 * @file    oled_app.c
 * @brief   0.96 寸 OLED（SSD1306，128x64）驱动：软件模拟 I2C，SDA=PB6、SCL=PB7
 *
 * 为什么用软件 I2C：
 *   PB6/PB7 正好是 STM32F103 的 I2C1 引脚，但 F1 的硬件 I2C 有著名的
 *   "总线被拉死"毛病（从机在 ACK 位被复位/中断打断，SCL 被钉在低电平）。
 *   OLED 一次刷新也就几百字节，软件模拟完全够用，而且时序看得见、好排错。
 *
 * 引脚为什么必须配成开漏输出（GPIO_Mode_Out_OD）：
 *   I2C 是"线与"总线：任何一方都只能把线拉低，拉高要靠上拉电阻。
 *   OLED 模块板上已经焊了 4.7k 上拉电阻，所以"开漏输出 + 模块上拉"就是标准 I2C 电平。
 *   注意 F1 的 GPIO 内部上拉只在输入模式下才生效，所以这里不能指望内部上拉。
 */

#include "oled_app.h" /* 本模块接口，同时带进 stm32f10x.h 与 oled_font.h */
#include "iic.h"      /* 总线探测：自动选 0x78 / 0x7A */

/* ------------------------------------------------------------------
 * 把两根线拉高/拉低
 * 直接用 SetBits/ResetBits 写 BSRR/BRR 寄存器，比 GPIO_WriteBit 少一次判断，
 * 在高频软件 I2C 里能省下不少时间。
 * ------------------------------------------------------------------ */
#define OLED_SCL_H() GPIO_SetBits(OLED_SCL_PORT, OLED_SCL_PIN)   /* SCL 输出高（靠外部上拉） */
#define OLED_SCL_L() GPIO_ResetBits(OLED_SCL_PORT, OLED_SCL_PIN) /* SCL 输出低（主动拉低） */
#define OLED_SDA_H() GPIO_SetBits(OLED_SDA_PORT, OLED_SDA_PIN)   /* SDA 输出高（靠外部上拉） */
#define OLED_SDA_L() GPIO_ResetBits(OLED_SDA_PORT, OLED_SDA_PIN) /* SDA 输出低（主动拉低） */

/* ------------------------------------------------------------------
 * I2C 半周期延时
 *
 * 为什么需要：
 *   72MHz 下 GPIO 翻转一次只要十几个周期，不加延时的 SCL 能跑到 1MHz 左右，
 *   超出 SSD1306 数据手册的 400kHz 上限。很多模块确实能扛，但"能用"不等于"合规"，
 *   也不该拿它赌自己手上这块屏。
 *   这里用 volatile 空循环把每个半周期拉到 1.5us 上下，SCL 约 300kHz，留足余量。
 *
 * 为什么不用 delay_us()：
 *   那个函数依赖 delay_init() 先跑过（fac_ms 为 0 时 SysTick 会死等），
 *   一旦用上，oled_* 全家都会跟着有这个依赖。空循环自成一体，
 *   oled_init() 才能被放在 main 里的任意位置。
 *
 * 怎么调：
 *   屏幕花屏/不亮 -> 把 OLED_I2C_DELAY_LOOPS 调大（更慢更稳）；
 *   嫌刷新慢     -> 调小。0 表示不加延时（回到"最快但不合规"的状态）。
 * ------------------------------------------------------------------ */
#define OLED_I2C_DELAY_LOOPS 16 /* 约 1.5us；SCL 约 300kHz */

#define OLED_I2C_DELAY()                                                      \
    do                                                                        \
    {                                                                         \
        volatile uint8_t oled_dly_i; /* volatile：防止编译器把这个空循环优化掉 */ \
        for (oled_dly_i = 0; oled_dly_i < OLED_I2C_DELAY_LOOPS; oled_dly_i++)  \
        {                                                                     \
            /* 空转，只为消耗时间 */                                          \
        }                                                                     \
    } while (0)

/* SSD1306 控制字节：跟着从机地址后面的第一个字节，说明"接下来这个字节是命令还是数据" */
#define OLED_CTRL_CMD  0x00 /* 0x00 = 后面是命令 */
#define OLED_CTRL_DATA 0x40 /* 0x40 = 后面是显存数据 */

/* 当前从机地址（8 位写地址）。默认取 OLED_I2C_ADDR，
 * main.c 探测到模块在 0x7A 时会用 oled_set_addr() 改掉它。 */
static uint8_t oled_addr = OLED_I2C_ADDR;

void oled_set_addr(uint8_t addr8)
{
    oled_addr = addr8;
}

/* ------------------------------------------------------------------
 * 内部函数声明
 * ------------------------------------------------------------------ */
static void oled_i2c_init(void);
static void oled_i2c_start(void);
static void oled_i2c_stop(void);
static void oled_i2c_send_byte(uint8_t byte);
static void oled_write_cmd(uint8_t cmd);
static void oled_write_data(uint8_t data);
static void oled_set_cursor(uint8_t page, uint8_t col);
static uint32_t oled_pow10(uint8_t n);
static void oled_power_on_delay(void);

/* ------------------------------------------------------------------
 * I2C 时序层
 * ------------------------------------------------------------------ */

/**
 * @brief  把 SDA/SCL 配成开漏输出并都拉高（总线空闲态）
 * @note   上电时引脚默认是浮空输入，先配成开漏输出并输出 1，
 *         总线进入"两根线都被上拉电阻拉高"的空闲状态，再发命令。
 */
static void oled_i2c_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure; /* 定义 GPIO 配置结构体 */

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); /* GPIOB 挂在 APB2，先开时钟配置才生效 */

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;  /* 开漏输出：只能拉低，拉高交给外部上拉 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; /* 50MHz 档：软件 I2C 翻转频繁，选快一点 */

    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN; /* 先配 SCL 一根线 */
    GPIO_Init(OLED_SCL_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = OLED_SDA_PIN; /* 同一份模式/速度，换成 SDA 再配一次 */
    GPIO_Init(OLED_SDA_PORT, &GPIO_InitStructure);

    OLED_SCL_H(); /* 空闲态 SCL = 1 */
    OLED_SDA_H(); /* 空闲态 SDA = 1 */
}

/**
 * @brief  I2C 起始条件
 * @note   规则：SCL 为高电平期间，SDA 由高变低 = START。
 */
static void oled_i2c_start(void)
{
    OLED_SDA_H(); /* 先确保 SDA 为高 */
    OLED_SCL_H(); /* 再确保 SCL 为高（此时总线空闲） */
    OLED_I2C_DELAY();

    OLED_SDA_L(); /* SCL 保持高，SDA 拉低 -> 起始条件成立 */
    OLED_I2C_DELAY();

    OLED_SCL_L(); /* 拉低 SCL，准备传第一个数据位 */
    OLED_I2C_DELAY();
}

/**
 * @brief  I2C 停止条件
 * @note   规则：SCL 为高电平期间，SDA 由低变高 = STOP。
 */
static void oled_i2c_stop(void)
{
    OLED_SDA_L(); /* 先把 SDA 拉低，制造"由低变高"的起点 */
    OLED_I2C_DELAY();

    OLED_SCL_H(); /* 拉高 SCL */
    OLED_I2C_DELAY();

    OLED_SDA_H(); /* SCL 保持高，SDA 拉高 -> 停止条件成立 */
    OLED_I2C_DELAY();
}

/**
 * @brief  I2C 发送一个字节（高位在前 MSB first）
 * @param  byte 要发送的字节
 * @note   数据只能在 SCL 为低电平期间改变，SCL 为高电平期间从机采样，
 *         所以顺序一定是"摆好 SDA -> 拉高 SCL（从机读）-> 拉低 SCL（换下一位）"。
 *         第 9 个时钟是应答位，本驱动不看 ACK：OLED 只写不读，
 *         漏掉 ACK 检查换来的是更少的分支和更稳的时序。
 */
static void oled_i2c_send_byte(uint8_t byte)
{
    uint8_t i;

    for (i = 0; i < 8; i++) /* 一个字节 8 位 */
    {
        if (byte & (0x80 >> i)) /* 取出当前最高位，判断该摆 1 还是 0 */
        {
            OLED_SDA_H(); /* 该位是 1：释放 SDA，由上拉电阻拉高 */
        }
        else
        {
            OLED_SDA_L(); /* 该位是 0：主动把 SDA 拉低 */
        }

        OLED_SCL_H();      /* 拉高 SCL，从机在这一刻采样 SDA */
        OLED_I2C_DELAY();  /* SCL 高电平保持够久，时钟频率才在规格内 */
        OLED_SCL_L();      /* 拉低 SCL，允许改变 SDA，准备下一位 */
        OLED_I2C_DELAY();  /* SCL 低电平也要保持，否则占空比失衡、从机可能丢位 */
    }

    OLED_SCL_H();     /* 第 9 个时钟：应答位 */
    OLED_I2C_DELAY();
    OLED_SCL_L();     /* 不读 SDA，直接跳过；从机若拉低 SDA 表示 ACK，这里不处理 */
    OLED_I2C_DELAY();
}

/**
 * @brief  写一条命令
 * @param  cmd 命令字节
 * @note   一次完整事务：START + 从机地址 + 控制字节(0x00) + 命令 + STOP
 */
static void oled_write_cmd(uint8_t cmd)
{
    oled_i2c_start();
    oled_i2c_send_byte(oled_addr);   /* 从机地址（写方向） */
    oled_i2c_send_byte(OLED_CTRL_CMD);   /* 控制字节：说明后面是命令 */
    oled_i2c_send_byte(cmd);             /* 命令本体 */
    oled_i2c_stop();
}

/**
 * @brief  写一个字节显存数据
 * @param  data 要写入显存的数据（一列 8 个像素，bit0 在最上面）
 */
static void oled_write_data(uint8_t data)
{
    oled_i2c_start();
    oled_i2c_send_byte(oled_addr);   /* 从机地址（写方向） */
    oled_i2c_send_byte(OLED_CTRL_DATA);  /* 控制字节：说明后面是显存数据 */
    oled_i2c_send_byte(data);            /* 数据本体，写入后列地址自动 +1 */
    oled_i2c_stop();
}

/**
 * @brief  设置写入位置（SSD1306 页寻址模式）
 * @param  page 页号 0~7，一页 = 8 行像素
 * @param  col  列号 0~127
 */
static void oled_set_cursor(uint8_t page, uint8_t col)
{
    if (page >= OLED_PAGES || col >= OLED_WIDTH) /* 越界直接丢弃，避免把显存写到屏幕以外 */
    {
        return;
    }

    oled_write_cmd(0xB0 | page);               /* 命令 0xB0+page：设置页地址 */
    oled_write_cmd(0x10 | ((col & 0xF0) >> 4)); /* 设置列地址高 4 位 */
    oled_write_cmd(0x00 | (col & 0x0F));        /* 设置列地址低 4 位 */
}

/**
 * @brief  求 10 的 n 次方
 * @param  n 指数 0~9
 * @retval 10^n
 * @note   oled_show_num 需要按位取数字，用它做除数。
 *         不用 math.h 的 pow：那要链接浮点库，白白多几 KB Flash。
 */
static uint32_t oled_pow10(uint8_t n)
{
    uint32_t result = 1;

    while (n--)
    {
        result *= 10;
    }
    return result;
}

/**
 * @brief  上电延时
 * @note   SSD1306 上电后要等内部电荷泵稳定（数据手册要求 ≥100ms）才能发命令。
 *         这里故意用空循环而不是 delay_ms()：delay_ms 依赖 delay_init() 先跑过
 *         （fac_ms 为 0 时 SysTick 会死等），让 oled_init 自成一体更安全。
 *         72MHz 下 1000*1000 次空循环约 50~100ms，对本用途足够。
 */
static void oled_power_on_delay(void)
{
    volatile uint32_t i, j; /* volatile：防止编译器把空循环优化掉 */

    for (i = 0; i < 1000; i++)
    {
        for (j = 0; j < 1000; j++)
        {
            /* 故意空转，只为消耗时间 */
        }
    }
}

/* ------------------------------------------------------------------
 * 对外接口
 * ------------------------------------------------------------------ */

/**
 * @brief  初始化 OLED
 * @note   顺序：上电延时 -> 配置引脚 -> 写 SSD1306 初始化命令 -> 清屏
 *         命令序列和课程示例（江协科技 4-1 OLED 显示屏）完全一致，
 *         换屏/花屏时不要乱改，先按这里的注释逐条对照数据手册。
 */
void oled_init(void)
{
    oled_power_on_delay(); /* 上电延时，等电荷泵稳住 */
    oled_i2c_init();       /* 配置 PB6/PB7 为开漏输出并置空闲态 */

    /* 自动选从机地址：0x78 最常见；模块背面 SA0 接 VCC 时是 0x7A。
     * 两个都没有应答就保持头文件里的默认值，后面写命令自然也不会成功。 */
    if (IIC_Probe(0x78) != 0)
    {
        oled_set_addr(0x78);
    }
    else if (IIC_Probe(0x7A) != 0)
    {
        oled_set_addr(0x7A);
    }

    oled_write_cmd(0xAE); /* 关闭显示（配置期间先关，避免看到花屏过程） */

    oled_write_cmd(0xD5); /* 设置显示时钟分频比 / 振荡器频率 */
    oled_write_cmd(0x80); /* 分频比 1，频率默认 */

    oled_write_cmd(0xA8); /* 设置多路复用率（占空比） */
    oled_write_cmd(0x3F); /* 1/64 占空比，对应 64 行 */

    oled_write_cmd(0xD3); /* 设置显示偏移 */
    oled_write_cmd(0x00); /* 不偏移 */

    oled_write_cmd(0x40); /* 设置显示起始行 = 第 0 行 */

    oled_write_cmd(0xA1); /* 段重映射：0xA1 左右正常，0xA0 左右镜像 */

    oled_write_cmd(0xC8); /* 扫描方向：0xC8 上下正常，0xC0 上下镜像 */

    oled_write_cmd(0xDA); /* 设置 COM 引脚硬件配置 */
    oled_write_cmd(0x12); /* 交替式 COM 配置，适配 128x64 */

    oled_write_cmd(0x81); /* 设置对比度（亮度） */
    oled_write_cmd(0xCF); /* 0x00~0xFF，0xCF 是常见的亮而不糊的值 */

    oled_write_cmd(0xD9); /* 设置预充电周期 */
    oled_write_cmd(0xF1);

    oled_write_cmd(0xDB); /* 设置 VCOMH 取消选择电平 */
    oled_write_cmd(0x30);

    oled_write_cmd(0xA4); /* 输出跟随显存（0xA5 则无视显存整屏点亮） */

    oled_write_cmd(0xA6); /* 正常显示（0xA7 反色） */

    oled_write_cmd(0x8D); /* 电荷泵设置 */
    oled_write_cmd(0x14); /* 打开电荷泵：模块只给 3V3 时靠它升压点亮面板 */

    oled_write_cmd(0xAF); /* 打开显示 */

    oled_clear(); /* 清屏，避免显存里的随机值上电显示成雪花 */
}

/**
 * @brief  整屏清空
 * @note   每页 128 个 0 一次性发完：8 次 I2C 事务，比逐字节写 1024 次快得多。
 */
void oled_clear(void)
{
    static const uint8_t zero[OLED_WIDTH] = {0}; /* 128 个 0，放 Flash 不占 RAM */
    uint8_t page;

    for (page = 0; page < OLED_PAGES; page++) /* 逐页 */
    {
        oled_write_run(page, 0, zero, OLED_WIDTH); /* 一页 128 列一次写完 */
    }
}

/**
 * @brief  连续写入一页内的多个显存字节
 * @param  page 页号 0~7
 * @param  col  起始列 0~127
 * @param  data 数据首地址
 * @param  len  字节数，1~128
 * @note   SSD1306 支持一次事务里连写多个数据字节，列地址自动 +1。
 *         逐字节写时每字节都要 START/地址/控制字/STOP，开销是数据的 4 倍；
 *         整行刷新（滚动字幕）走这条路径，128 字节只要一次事务。
 */
void oled_write_run(uint8_t page, uint8_t col, const uint8_t *data, uint8_t len)
{
    uint8_t i;

    if (page >= OLED_PAGES || col >= OLED_WIDTH || len == 0) /* 越界直接丢弃 */
    {
        return;
    }

    if ((uint16_t)col + len > OLED_WIDTH) /* 截断到本页末尾，避免绕回行首 */
    {
        len = (uint8_t)(OLED_WIDTH - col);
    }

    oled_set_cursor(page, col); /* 设定起始页/列 */

    oled_i2c_start();
    oled_i2c_send_byte(oled_addr);  /* 从机地址（写方向） */
    oled_i2c_send_byte(OLED_CTRL_DATA); /* 控制字节：后面全是显存数据 */
    for (i = 0; i < len; i++)
    {
        oled_i2c_send_byte(data[i]); /* 连写，列地址自动 +1 */
    }
    oled_i2c_stop();
}

/**
 * @brief  显示一个 ASCII 字符
 * @param  line 行 1~4
 * @param  col  列 1~16
 * @param  ch   字符，范围 0x20(' ') ~ 0x7E('~')
 * @note   8x16 的字模要拆成上下两半页写：上半页 8 字节、下半页 8 字节。
 */
void oled_show_char(uint8_t line, uint8_t col, char ch)
{
    uint8_t i;
    uint8_t index;

    if (ch < (char)OLED_ASCII_FIRST || ch > (char)OLED_ASCII_LAST) /* 字库里没有的字符 */
    {
        ch = ' '; /* 统一显示成空格，免得下标越界读到别的字模 */
    }

    index = (uint8_t)(ch - OLED_ASCII_FIRST); /* 字模下标 = ASCII 码 - 0x20 */

    oled_set_cursor((uint8_t)((line - 1) * 2), (uint8_t)((col - 1) * 8)); /* 定位到上半页 */
    for (i = 0; i < 8; i++)
    {
        oled_write_data(OLED_F8x16[index][i]); /* 写上半页 8 列 */
    }

    oled_set_cursor((uint8_t)((line - 1) * 2 + 1), (uint8_t)((col - 1) * 8)); /* 定位到下半页 */
    for (i = 0; i < 8; i++)
    {
        oled_write_data(OLED_F8x16[index][i + 8]); /* 写下半页 8 列 */
    }
}

/**
 * @brief  显示字符串
 * @param  line 起始行 1~4
 * @param  col  起始列 1~16
 * @param  str  以 '\0' 结尾的字符串
 */
void oled_show_string(uint8_t line, uint8_t col, const char *str)
{
    uint8_t i;

    for (i = 0; str[i] != '\0'; i++) /* 一直写到字符串结尾 */
    {
        oled_show_char(line, (uint8_t)(col + i), str[i]); /* 每个字符占 1 列，依次右移 */
    }
}

/**
 * @brief  显示定长十进制数
 * @param  line 起始行 1~4
 * @param  col  起始列 1~16
 * @param  num  要显示的数
 * @param  len  显示位数 1~10，不足补前导 0
 * @note   定长显示的好处：刷新时不用先擦旧内容。比如 999 -> 1000，
 *         位数变化会留下残影，定长就把这个问题从源头去掉了。
 */
void oled_show_num(uint8_t line, uint8_t col, uint32_t num, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len; i++)
    {
        /* 取第 i 位：先把高位去掉，再取个位，最后换成 ASCII 码 */
        oled_show_char(line, (uint8_t)(col + i),
                       (char)(num / oled_pow10((uint8_t)(len - i - 1)) % 10 + '0'));
    }
}

/**
 * @brief  显示一个 16x16 中文字
 * @param  line  行 1~4
 * @param  col   起始列 1~16，字占 col 与 col+1 两列
 * @param  index 字模索引，取 oled_font.h 里的 OLED_CN_xxx
 * @note   中文字模宽 16 像素 = 2 个 ASCII 列，所以列坐标按 (col-1)*8 换算成像素后，
 *         上下两页各写 16 列。这样中文和 ASCII 可以混排在同一行，列号能对齐。
 */
void oled_show_cn(uint8_t line, uint8_t col, uint8_t index)
{
    uint8_t i;
    uint8_t x;

    if (index >= OLED_CN_NUM) /* 索引越界就什么都不画，避免读飞 */
    {
        return;
    }

    x = (uint8_t)((col - 1) * 8); /* 起始像素列 */

    oled_set_cursor((uint8_t)((line - 1) * 2), x); /* 定位到上半页 */
    for (i = 0; i < 16; i++)
    {
        oled_write_data(OLED_CF16x16[index][i]); /* 字模前 16 字节 = 上半页的 16 列 */
    }

    oled_set_cursor((uint8_t)((line - 1) * 2 + 1), x); /* 定位到下半页 */
    for (i = 0; i < 16; i++)
    {
        oled_write_data(OLED_CF16x16[index][i + 16]); /* 字模后 16 字节 = 下半页的 16 列 */
    }
}
