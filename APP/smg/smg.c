/**
 * @file    smg.c
 * @brief   4 位数码管驱动：段选 PD0~PD7，位选 PG9~PG12，TIM4 1ms 扫描
 *
 * 为什么用定时器扫描，而不是在主循环里 for 四位 + delay_ms：
 *   本工程主循环里有 OLED 的软件 I2C（一次刷新几十 ms）和 DHT11 读取
 *   （一次约 20ms）。如果扫描放在主循环，这些阻塞期间数码管会停在某一位，
 *   看起来就是"一位特别亮、其它位灭"。放到 TIM4 中断里每 1ms 扫一位，
 *   主循环再怎么阻塞，显示都照常刷新。
 *
 * 扫描顺序（每一位都必须这样）：
 *   ① 段码全部熄灭
 *   ② 4 位位选全部关断
 *   ③ 等一小段消影时间（SMG_GHOST_US）
 *   ④ 送这一位的段码
 *   ⑤ 只选通这一位
 *   任何"换位选"的时刻段码都必须是灭的，否则上一位的笔画会印到新一位上。
 *
 * 段码表 bit 定义：bit0=a bit1=b ... bit6=g bit7=dp。
 *   共阳（低电平点亮）：0 = 亮；共阴（高电平点亮）：1 = 亮。
 */

#include "smg.h"
#include "delay.h"

/* ------------------------------------------------------------------
 * 段码表
 * ------------------------------------------------------------------ */
#if SMG_COMMON_ANODE
/* 共阳：0 = 该段亮；最后一个 0xBF 是负号 '-'（只有 g 段亮） */
static const uint8_t SEG_TAB[SMG_GLYPH_NUM] = {
    0xC0, /* 0 */
    0xF9, /* 1 */
    0xA4, /* 2 */
    0xB0, /* 3 */
    0x99, /* 4 */
    0x92, /* 5 */
    0x82, /* 6 */
    0xF8, /* 7 */
    0x80, /* 8 */
    0x90, /* 9 */
    0xBF  /* - */
};
#define SMG_SEG_OFF 0xFFu /* 8 段全高 = 全灭 */
#define SMG_SEG_ON  0x00u /* 8 段全低 = 全亮 */
#else
/* 共阴：1 = 该段亮；最后一个 0x40 是负号 '-'（只有 g 段亮） */
static const uint8_t SEG_TAB[SMG_GLYPH_NUM] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F, /* 9 */
    0x40  /* - */
};
#define SMG_SEG_OFF 0x00u
#define SMG_SEG_ON  0xFFu
#endif

/* ------------------------------------------------------------------
 * 位选：PG9~PG12
 * ------------------------------------------------------------------ */
#define SMG_DIG_BIT(pos) ((uint16_t)(1u << (SMG_DIG_SHIFT + ((SMG_DIG_REVERSE) ? (3u - (pos)) : (pos)))))

#if SMG_DIG_ACTIVE_HIGH
#define SMG_DIG_ALL_OFF ((uint16_t)0x0000) /* 4 位全低 = 都不选通 */
#define SMG_DIG_ONE(pos) SMG_DIG_BIT(pos)  /* 只把这一位拉高 */
#else
#define SMG_DIG_ALL_OFF SMG_DIG_MASK                       /* 4 位全高 = 都不选通 */
#define SMG_DIG_ONE(pos) ((uint16_t)(SMG_DIG_MASK & ~SMG_DIG_BIT(pos))) /* 只把这一位拉低 */
#endif

#if SMG_DIG_OUT_OD
#define SMG_DIG_OUT_MODE GPIO_Mode_Out_OD
#else
#define SMG_DIG_OUT_MODE GPIO_Mode_Out_PP
#endif

/* 小数点：共阳时把 dp 位清 0 点亮；共阴时置 1 点亮 */
#if SMG_COMMON_ANODE
#define SMG_DP_ON(seg) ((uint8_t)((seg) & (uint8_t)~0x80u))
#else
#define SMG_DP_ON(seg) ((uint8_t)((seg) | 0x80u))
#endif

/* ------------------------------------------------------------------
 * 显示缓存：主循环写，TIM4 中断读
 *   s_buf[pos] = 0~9 / SMG_MINUS / SMG_BLANK
 *   s_dp[pos]  = 1 表示该位小数点点亮
 * ------------------------------------------------------------------ */
static volatile uint8_t s_buf[SMG_DIGIT_NUM];
static volatile uint8_t s_dp[SMG_DIGIT_NUM];
static uint8_t s_pos; /* 当前扫到第几位 */

/* 主循环写缓存时短暂关中断，避免中断读到"半新半旧"的 4 位内容 */
static uint32_t SMG_IrqLock(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void SMG_IrqUnlock(uint32_t primask)
{
    __set_PRIMASK(primask);
}

/* ------------------------------------------------------------------
 * 底层端口写：用 BSRR 一次写完，只动本模块的引脚
 * ------------------------------------------------------------------ */
static void SMG_SendSeg(uint8_t seg)
{
    uint32_t bits = ((uint32_t)seg << SMG_SEG_SHIFT) & SMG_SEG_MASK;
    uint32_t reset = (~bits) & SMG_SEG_MASK;

    /* BSRR 低 16 位置 1、高 16 位清 0；写 0 的位不动作 */
    SMG_SEG_PORT->BSRR = bits | (reset << 16);
}

static void SMG_SendDig(uint16_t dig)
{
    uint32_t bits = (uint32_t)dig & SMG_DIG_MASK;
    uint32_t reset = (~bits) & SMG_DIG_MASK;

    SMG_DIG_PORT->BSRR = bits | (reset << 16);
}

/* 先灭段码，再关位选：保证换位过程中不会把上一位的笔画带过来 */
static void SMG_AllOff(void)
{
    SMG_SendSeg(SMG_SEG_OFF);
    SMG_SendDig(SMG_DIG_ALL_OFF);
}

#if SMG_GHOST_US
/* 纯空循环：不调用 delay_us，避免和扫描/其它延时抢 SysTick。
 * volatile 防止编译器把空循环优化掉。72MHz 下一层约 83ns。 */
static void SMG_GhostDelay(void)
{
    volatile uint16_t i;

    for (i = 0; i < (uint16_t)(SMG_GHOST_US * 12); i++)
    {
        ;
    }
}
#else
#define SMG_GhostDelay() do { } while (0)
#endif

/* ------------------------------------------------------------------
 * 中断里扫一位
 * ------------------------------------------------------------------ */
static void SMG_Scan(void)
{
    uint8_t pos = s_pos;
    uint8_t num;
    uint8_t seg;

    SMG_AllOff();    /* ① 段全灭 ② 位全关 */
    SMG_GhostDelay(); /* ③ 等上一位彻底断开 */

    num = s_buf[pos];
    if (num < SMG_GLYPH_NUM)
    {
        seg = SEG_TAB[num];
    }
    else
    {
        seg = SMG_SEG_OFF; /* 越界 / SMG_BLANK 一律熄灭 */
    }
    if (s_dp[pos])
    {
        seg = SMG_DP_ON(seg);
    }

    SMG_SendSeg(seg);                /* ④ 先准备好段码 */
    SMG_SendDig(SMG_DIG_ONE(pos));   /* ⑤ 再只选通这一位 */

    pos++;
    if (pos >= SMG_DIGIT_NUM)
    {
        pos = 0;
    }
    s_pos = pos;
}

/* TIM4 更新中断：1ms 一次，扫一位，4ms 扫完 4 位 */
void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        SMG_Scan();
    }
}

/* ------------------------------------------------------------------
 * 初始化
 * ------------------------------------------------------------------ */
void SMG_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(SMG_SEG_RCC | SMG_DIG_RCC, ENABLE); /* PD + PG */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);       /* TIM4 在 APB1 */

    /* 先把输出锁存器写成"全灭"，再切输出模式，避免上电瞬间闪一下 */
    SMG_AllOff();

    /* 段选 PD0~PD7：推挽输出 */
    GPIO_InitStructure.GPIO_Pin = SMG_SEG_MASK;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(SMG_SEG_PORT, &GPIO_InitStructure);

    /* 位选 PG9~PG12：默认推挽；PNP 基极上拉到 5V 时改 SMG_DIG_OUT_OD=1 */
    GPIO_InitStructure.GPIO_Pin = SMG_DIG_MASK;
    GPIO_InitStructure.GPIO_Mode = SMG_DIG_OUT_MODE;
    GPIO_Init(SMG_DIG_PORT, &GPIO_InitStructure);

    SMG_AllOff();
    SMG_Clear();
    s_pos = 0;

    /* TIM4：72MHz / (71+1) = 1MHz，ARR=999 -> 1ms 更新一次 */
    TIM_TimeBaseStructure.TIM_Period = 999;
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM4, TIM_FLAG_Update);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM4, ENABLE); /* 启动扫描 */

#if SMG_INIT_SELFTEST
    SMG_SelfTest(); /* 需要 delay_init() 已经调用过 */
#endif
}

/* ------------------------------------------------------------------
 * 显示缓冲区写入
 * ------------------------------------------------------------------ */
static void SMG_SetBufferDp(const uint8_t *buf, const uint8_t *dp)
{
    uint8_t i;
    uint32_t primask = SMG_IrqLock();

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        s_buf[i] = buf[i];
        s_dp[i] = (uint8_t)(dp[i] ? 1 : 0);
    }

    SMG_IrqUnlock(primask);
}

/* 兼容旧接口：旧工程里 SMG_Display(buf) 是"扫一遍"，这里改成"写缓冲区"，
 * 扫描已经交给 TIM4 中断，所以连续调用它也只会更新内容，不会拖慢主循环。 */
void SMG_Display(const uint8_t *buf)
{
    uint8_t dp[SMG_DIGIT_NUM];
    uint8_t i;

    if (buf == 0)
    {
        SMG_Clear();
        return;
    }

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        dp[i] = 0;
    }
    SMG_SetBufferDp(buf, dp);
}

void SMG_ShowOne(uint8_t pos, uint8_t num)
{
    uint32_t primask;

    if (pos >= SMG_DIGIT_NUM)
    {
        return;
    }
    if (num >= SMG_GLYPH_NUM)
    {
        num = SMG_BLANK;
    }

    primask = SMG_IrqLock();
    s_buf[pos] = num;
    s_dp[pos] = 0;
    SMG_IrqUnlock(primask);
}

void SMG_Clear(void)
{
    uint8_t i;
    uint32_t primask = SMG_IrqLock();

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        s_buf[i] = SMG_BLANK;
        s_dp[i] = 0;
    }

    SMG_IrqUnlock(primask);
}

void SMG_ShowDashes(void)
{
    uint8_t buf[SMG_DIGIT_NUM];
    uint8_t i;

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        buf[i] = SMG_MINUS;
    }
    SMG_Display(buf);
}

/* 显示 0~9999：从最右位往前拆，前导位保持 SMG_BLANK；超过 4 位显示 "----" */
void SMG_ShowNum(uint16_t value)
{
    uint8_t buf[SMG_DIGIT_NUM];
    uint8_t i;

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        buf[i] = SMG_BLANK;
    }

    i = SMG_DIGIT_NUM;
    do
    {
        i--;
        buf[i] = (uint8_t)(value % 10u);
        value /= 10u;
    } while ((value > 0u) && (i > 0u));

    if (value > 0u) /* 还有剩余 -> 超过 4 位 */
    {
        for (i = 0; i < SMG_DIGIT_NUM; i++)
        {
            buf[i] = SMG_MINUS;
        }
    }

    SMG_Display(buf);
}

/* 显示温度：253 -> "25.3"，-50 -> "-5.0"，1000 -> "100.0"。
 * 小数点挂在最后一个整数位上；负号占最左一位。
 * 超过 4 位能放下的范围（如 -100.0 需要 5 格）显示 "----"。 */
void SMG_ShowTemp(int16_t temp_x10)
{
    uint8_t g[5];  /* 字形序列（含负号），最多 5 个 */
    uint8_t d[5];  /* 每个字形是否带小数点 */
    uint8_t buf[SMG_DIGIT_NUM];
    uint8_t dp[SMG_DIGIT_NUM];
    uint8_t n = 0;
    uint8_t i;
    uint8_t start;
    int32_t v = temp_x10;
    uint16_t ip;
    uint8_t dec;

    if (v < 0)
    {
        g[n] = SMG_MINUS;
        n++;
        v = -v;
    }

    ip = (uint16_t)(v / 10);
    dec = (uint8_t)(v % 10);

    /* 4 位放不下 1000.0 以上：直接显示 "----"，避免拆出大于 9 的字形 */
    if (ip >= 1000u)
    {
        SMG_ShowDashes();
        return;
    }

    for (i = 0; i < 5; i++)
    {
        d[i] = 0;
    }

    if (ip >= 100u)
    {
        g[n] = (uint8_t)(ip / 100u);
        n++;
        g[n] = (uint8_t)((ip / 10u) % 10u);
        n++;
        g[n] = (uint8_t)(ip % 10u);
        n++;
    }
    else if (ip >= 10u)
    {
        g[n] = (uint8_t)(ip / 10u);
        n++;
        g[n] = (uint8_t)(ip % 10u);
        n++;
    }
    else
    {
        g[n] = (uint8_t)ip;
        n++;
    }

    d[n - 1] = 1; /* 小数点挂在最后一个整数位后面 */
    g[n] = dec;
    n++;

    if (n > SMG_DIGIT_NUM)
    {
        SMG_ShowDashes();
        return;
    }

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        buf[i] = SMG_BLANK;
        dp[i] = 0;
    }

    start = (uint8_t)(SMG_DIGIT_NUM - n); /* 右对齐 */
    for (i = 0; i < n; i++)
    {
        buf[start + i] = g[i];
        dp[start + i] = d[i];
    }

    SMG_SetBufferDp(buf, dp);
}

/* 逐位点亮 8，再显示 1234，最后清屏；用于第一次接线时确认段/位没接反 */
void SMG_SelfTest(void)
{
    uint8_t buf[SMG_DIGIT_NUM];
    uint8_t i;
    uint8_t j;

    for (i = 0; i < SMG_DIGIT_NUM; i++)
    {
        for (j = 0; j < SMG_DIGIT_NUM; j++)
        {
            buf[j] = SMG_BLANK;
        }
        buf[i] = 8;
        SMG_Display(buf);
        delay_ms(SMG_SELFTEST_MS);
    }

    SMG_ShowNum(1234);
    delay_ms(SMG_SELFTEST_MS);
    SMG_Clear();
}
