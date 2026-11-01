/**
 * @file    photo_app.c
 * @brief   光敏电阻传感器驱动：AO = PA5（ADC1_IN5 模拟输入），DO = PB0（数字输入，保留）
 *
 * 三个关键设计点：
 *   1. LED 的明暗改由 AO 分值判断（photo_score_is_dark），门槛写在 photo_app.h 里，
 *      不再受模块蓝色电位器影响；DO 仍能读，但主程序不再用它控制 LED。
 *   2. AO 配成"模拟输入"（GPIO_Mode_AIN）。这一步不能省：F1 的引脚只要配成
 *      数字输入，内部的施密特触发器就会把模拟电压整形成 0/1，ADC 就采不到真实电压了。
 *   3. DO 配成"上拉输入"而不是浮空输入。模块没插好时引脚被内部上拉钳在高电平，
 *      读到的是确定的 1 而不是乱跳的随机值 —— 排错时能一眼看出"是没接，还是在读数据"。
 */

#include "photo_app.h" /* 本模块接口 */

/* ------------------------------------------------------------------
 * 内部函数声明
 * ------------------------------------------------------------------ */
static void photo_gpio_init(void);
static void photo_adc_init(void);

/* ------------------------------------------------------------------
 * 初始化
 * ------------------------------------------------------------------ */

/**
 * @brief  配置 PB0 与 PA5 的 GPIO
 * @note   PB0 上拉输入读开关量；PA5 模拟输入给 ADC 用，两者模式不同，
 *         所以寄存器要分两次写。
 */
static void photo_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure; /* 定义 GPIO 配置结构体 */

    /* PB0 和 PA5 分属 GPIOB / GPIOA，两个端口的时钟都要开。
     * 用宏而不是写死端口名：以后把 AO 换到 PC0 之类，只改 photo_app.h 即可。 */
    RCC_APB2PeriphClockCmd(PHOTO_DO_RCC | PHOTO_AO_RCC, ENABLE);

    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; /* 输入模式下速度位不影响功能，统一填一个值 */

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; /* 上拉输入：没接模块时读到确定的高电平 */
    GPIO_InitStructure.GPIO_Pin = PHOTO_DO_PIN;
    GPIO_Init(PHOTO_DO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN; /* 模拟输入：断开数字部分，让 ADC 直接量电压 */
    GPIO_InitStructure.GPIO_Pin = PHOTO_AO_PIN;
    GPIO_Init(PHOTO_AO_PORT, &GPIO_InitStructure);
}

/**
 * @brief  配置并校准 ADC1
 * @note   只用规则组的第 1 个序列、单次转换、软件触发 —— 最省事也最常用的配置。
 *         要连续转换或扫描多通道时改 ADC_ContinuousConvMode / ADC_ScanConvMode。
 */
static void photo_adc_init(void)
{
    ADC_InitTypeDef ADC_InitStructure; /* 定义 ADC 配置结构体 */

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); /* ADC1 挂在 APB2 */

    /* ADC 时钟不能超过 14MHz：PCLK2 = 72MHz，6 分频得 12MHz，正好合规 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 规则组序列 1 放通道 5（PA5），采样时间 55.5 个周期。
     * 采样时间越长，对高内阻信号源越友好；光敏分压的输出阻抗偏高，所以不选最短档。 */
    ADC_RegularChannelConfig(ADC1, PHOTO_AO_CHANNEL, 1, ADC_SampleTime_55Cycles5);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;                   /* 独立模式：只用 ADC1 */
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;               /* 右对齐：读出来直接就是 0~4095 */
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;  /* 不用外部触发，靠软件启动 */
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;                  /* 单次转换：需要时再软件启动 */
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;                        /* 不扫描：规则组里只有一个通道 */
    ADC_InitStructure.ADC_NbrOfChannel = 1;                              /* 通道数 1 */
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE); /* 使能 ADC1，内部电路才开始工作 */

    /* 校准是固定流程，不校准会有几到几十个 LSB 的固定偏差，照抄即可 */
    ADC_ResetCalibration(ADC1);                          /* 复位校准寄存器 */
    while (ADC_GetResetCalibrationStatus(ADC1) == SET)   /* 等复位完成 */
    {
        /* 空等 */
    }

    ADC_StartCalibration(ADC1);                          /* 开始校准 */
    while (ADC_GetCalibrationStatus(ADC1) == SET)        /* 等校准完成 */
    {
        /* 空等 */
    }
}

/**
 * @brief  初始化光敏电阻传感器（GPIO + ADC）
 */
void photo_init(void)
{
    photo_gpio_init();
    photo_adc_init();
}

/* ------------------------------------------------------------------
 * 读 DO（数字量）
 * ------------------------------------------------------------------ */

/**
 * @brief  读 DO 引脚的原始电平
 * @retval 0 或 1
 */
uint8_t photo_do_raw(void)
{
    return (uint8_t)GPIO_ReadInputDataBit(PHOTO_DO_PORT, PHOTO_DO_PIN);
}

/**
 * @brief  读 DO 并做 5 取 3 多数表决
 * @retval 0 或 1
 * @note   LM393 是开环比较器，没有回差：输入电压正好卡在门槛上时输出会来回抖。
 *         连读 5 次取多数，等于给判断加了一点点迟滞，屏幕上的 0/1 就不会乱跳。
 *         5 次读 IO 只要几百纳秒，随便调。
 */
uint8_t photo_do_get(void)
{
    uint8_t i;
    uint8_t count = 0; /* 统计读到 1 的次数 */

    for (i = 0; i < 5; i++)
    {
        if (photo_do_raw())
        {
            count++;
        }
    }

    return (count >= 3) ? 1 : 0; /* 3 票及以上算 1，否则算 0 */
}

/**
 * @brief  判断当前光照是否偏暗
 * @retval 1 = 暗，0 = 亮
 */
uint8_t photo_is_dark(void)
{
    return (photo_do_get() == PHOTO_DO_DARK_LEVEL) ? 1 : 0;
}

/* ------------------------------------------------------------------
 * 读 AO（模拟量）
 * ------------------------------------------------------------------ */

/**
 * @brief  启动一次 ADC 转换并读回结果
 * @retval 0~4095
 * @note   流程：软件触发 -> 死等 EOC 标志 -> 读数据寄存器。
 *         单次转换耗时约 (55.5 + 12.5) 个 ADC 周期，12MHz 下约 5.7us，
 *         比一次 OLED 刷新（毫秒级）短得多，直接阻塞等待完全没问题。
 */
uint16_t photo_ao_raw(void)
{
    ADC_SoftwareStartConvCmd(ADC1, ENABLE); /* 软件启动一次转换 */

    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) /* 等转换结束标志 */
    {
        /* 空等 */
    }

    return ADC_GetConversionValue(ADC1); /* 读结果，读动作本身会清 EOC 标志 */
}

/**
 * @brief  连续采样求平均
 * @param  times 采样次数，传 0 按 1 次处理
 * @retval 平均后的 0~4095
 * @note   累加用 uint32_t：4095 * 255 也不会溢出 32 位。
 *         平均能压掉随机噪声，但压不掉固定偏差 —— 要更准得靠外部基准。
 */
uint16_t photo_ao_avg(uint8_t times)
{
    uint8_t i;
    uint32_t sum = 0;

    if (times == 0) /* 防止后面除以 0 */
    {
        times = 1;
    }

    for (i = 0; i < times; i++)
    {
        sum += photo_ao_raw();
    }

    return (uint16_t)(sum / times);
}

/**
 * @brief  把 AO 原始值换算成毫伏
 * @param  raw photo_ao_raw() 或 photo_ao_avg() 的返回值，0~4095
 * @retval 0~3300 mV
 * @note   读数 0~4095，按满量程 4096 归一化（本板 VREF+ 经 0 欧电阻接 VDDA，即 3.3V）：
 *         mV = raw * 3300 / 4096。先乘后除，避免整数除法提前截断。
 */
uint16_t photo_raw_to_mv(uint16_t raw)
{
    return (uint16_t)((uint32_t)raw * 3300 / 4096);
}

/**
 * @brief  把 AO 原始值换算成 0~100 的光照分值
 * @param  raw photo_ao_raw() 或 photo_ao_avg() 的返回值，0~4095
 * @retval 0~100
 * @note   先乘 100 再除，避免先除后乘把小数全部截掉；加 2047（分母的一半）
 *         是为了四舍五入，不然 4094 会算成 99，看起来像"永远到不了满分"。
 *         映射方向由 PHOTO_SCORE_INVERT 控制，换模块不用改主程序。
 */
uint8_t photo_score(uint16_t raw)
{
    uint32_t value;

    if (raw > 4096) /* 防御性截断，正常 ADC 不会超过 */
    {
        raw = 4096;
    }

#if PHOTO_SCORE_INVERT
    raw = (uint16_t)(4096 - raw); /* 模块方向相反时翻过来 */
#endif

    value = (uint32_t)raw * 100 + 2047;
    return (uint8_t)(value / 4096);
}

/* ------------------------------------------------------------------
 * 用 AO 分值判断明暗（LED / 屏幕"光照"状态）
 * ------------------------------------------------------------------ */

/**
 * @brief  用 AO 分值判断当前是否偏暗（带 3 分回差）
 * @param  score       photo_score() 的返回值，0~100
 * @param  dark_score  当前暗阈值，由主程序传入（可按键调整）
 * @retval 1 = 暗，0 = 亮
 * @note   回差（迟滞）是必要的：分值刚好卡在 dark_score 附近时，噪声会让它
 *         在门槛两侧来回跳；没有回差的话 LED 就会 100ms 亮、100ms 灭地闪。
 *         现在低于 dark_score 才转"暗"，要回到 dark_score + PHOTO_DARK_HYST
 *         以上才转"亮"，中间保持上一次结果。
 *         用函数内部的 static 变量记住状态，主循环不用额外传参。
 */
uint8_t photo_score_is_dark(uint8_t score, uint8_t dark_score)
{
    static uint8_t dark = 0; /* 上一次的判定结果，上电默认"亮" */
    uint8_t release_score = (uint8_t)(dark_score + PHOTO_DARK_HYST);

    if (release_score > 100) /* 阈值调得很大时，回差不要超过满分 */
    {
        release_score = 100;
    }

    if (score < dark_score)
    {
        dark = 1; /* 低于门槛：暗 */
    }
    else if (score >= release_score)
    {
        dark = 0; /* 高出回差：亮 */
    }
    /* 门槛与回差之间保持原状态，避免临界点抖动 */

    return dark;
}
