/**
 * @file    main.h
 * @brief   全局定义 + 板级初始化
 *
 * 各模块的初始化统一放在本文件的 Board_Init() 里，main.c 只调一次：
 *     Board_Init();
 * 以后加新模块，只要在上面 include 对应头文件、在 Board_Init() 里加一行 xxx_init()。
 *
 * 注意：Board_Init() 写成了 static（只在本文件里可见），这样即使被多个 .c 包含
 * 也不会重复定义；目前只有 main.c 包含 main.h。
 */
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 各模块头文件（初始化要用，业务代码也直接能用）
 * ------------------------------------------------------------------ */
#include "delay.h"
#include "iic.h"
#include "smg.h"
#include "led_app.h"
#include "photo_app.h"
#include "tick_irq.h"
#include "soft_timer.h"
#include "key_irq.h"
#include "matrix_irq.h"
#include "buzzer_irq.h"
#include "relay_app.h"
#include "rgb_app.h"
#include "stepper_app.h"
#include "dht11_app.h"
#include "ds18b20_app.h"
#include "oled_app.h"
#include "bh1750.h"

/* ------------------------------------------------------------------
 * 初始化开关
 * ------------------------------------------------------------------ */

/* 温度传感器二选一：1 = DHT11（默认），0 = DS18B20 */
#define TEMP_SENSOR_DHT11 1

/* BH1750 照度传感器：1 = 上电初始化（没接也不影响，BH1750_Init() 返回 0），0 = 完全不用 */
#define USE_BH1750 1

/* ------------------------------------------------------------------
 * 常用阈值（后续写阈值判断时直接用）
 *   温度 0~50°C，步进 1；湿度 20~90%RH，步进 5；光敏 0~95，步进 5
 * ------------------------------------------------------------------ */
#define TEMP_THRESHOLD_DEFAULT 27
#define TEMP_THRESHOLD_MIN     0
#define TEMP_THRESHOLD_MAX     50
#define TEMP_THRESHOLD_STEP    1

#define HUMI_THRESHOLD_DEFAULT 50
#define HUMI_THRESHOLD_MIN     20
#define HUMI_THRESHOLD_MAX     90
#define HUMI_THRESHOLD_STEP    5

#define LIGHT_THRESHOLD_MIN    0
#define LIGHT_THRESHOLD_MAX    95
#define LIGHT_THRESHOLD_STEP   5

/* ------------------------------------------------------------------
 * RGB 光照判暗门限（写光照/RGB 逻辑时用）
 *   AO：亮度 = 4095 - 原始值，低于 AO_BRIGHT_LEVEL 判暗
 *   BH1750：lux 低于 BH_DARK_LEVEL 判暗
 * ------------------------------------------------------------------ */
#define AO_BRIGHT_LEVEL    1500
#define BH_DARK_LEVEL       300
#define LIGHT_LEVEL_HYST     20
#define LIGHT_LEVEL_RAW_MAX 4095

/* ------------------------------------------------------------------
 * 周期任务：每秒采样一次
 *   软定时器的回调是在 TIM6 中断里跑的，所以这里只置一个标志；
 *   真正的采样（读 DHT11/DS18B20、刷新 OLED）放在主循环里做——
 *   DHT11 读一次要 5ms 左右，绝不能塞进 1ms 中断。
 * ------------------------------------------------------------------ */
#define SAMPLE_PERIOD_MS 1000u

static volatile uint8_t s_sample_due = 0;
static soft_timer_t s_sample_timer;

static void Sample_Timer_Cb(void)
{
    s_sample_due = 1;
}

/* ------------------------------------------------------------------
 * 板级初始化：按顺序把每个模块初始化好，main.c 只需调这一个函数
 *   顺序有讲究：delay_init() 必须最先（别的驱动可能用 delay），
 *   TICK_Start() 必须最后（前面模块初始化时可能挂 1ms 钩子）。
 * ------------------------------------------------------------------ */
static void Board_Init(void)
{
    SOFTTIMER_Init(); /* 先清空软定时器表，各模块的 init 里才能注册 */
    delay_init();     /* SysTick 延时，必须最先 */
    IIC_Init();       /* PB6/PB7 软件 I2C 总线 */
    SMG_Init();       /* 数码管 + TIM4 扫描 */
    led_init();       /* 板载 D0 / D1 */
    photo_init();     /* 光敏 PA5 + ADC1 */
    TICK_Init();      /* TIM6 1ms 时基（先不启动） */
    KeyIRQ_Init();    /* PE0~PE3 外部中断 */
    MatrixIRQ_Init(); /* PF0~PF7 矩阵扫描 */
    BuzzerIRQ_Init(); /* PB10 蜂鸣器 */
    relay_init();     /* PC0 / PC1 / PC2 三路继电器 */
    rgb_init();       /* PC6~PC8 PWM */
    stepper_init();   /* PE12~PE14 步进电机（TIM7 发脉冲） */

#if TEMP_SENSOR_DHT11
    dht11_init();     /* PB11 DHT11 */
#else
    ds18b20_init();   /* PB11 DS18B20 */
#endif

    oled_init();      /* PB6/PB7 OLED，内部自动选 0x78 / 0x7A */

#if USE_BH1750
    (void)BH1750_Init(); /* PB6/PB7 照度传感器（没接也没关系，返回 0） */
#endif

    /* 每秒一次采样节拍：回调只置标志，主循环里再干活 */
    SOFTTIMER_Add(&s_sample_timer, Sample_Timer_Cb, SAMPLE_PERIOD_MS);

    TICK_Start();     /* 1ms 时基最后启动（所有软定时任务都要在它之前注册） */
}

/* ------------------------------------------------------------------
 * 下面写你自己的定义 / 变量声明 / 函数声明
 * ------------------------------------------------------------------ */

#endif /* __MAIN_H */
