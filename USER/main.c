/**
 * @file    main.c
 * @brief   程序入口：初始化各外设模块
 */
#include "main.h"      /* 自己的定义 / 变量 */
#include "delay.h"     /* delay_init */
#include "iic.h"       /* IIC_Init：PB6/PB7 软件 I2C 总线 */
#include "smg.h"       /* SMG_Init */
#include "led_app.h"   /* led_init */
#include "photo_app.h" /* photo_init */
#include "tick_irq.h"  /* TICK_Init / TICK_Start */
#include "key_irq.h"   /* KeyIRQ_Init */
#include "matrix_irq.h" /* MatrixIRQ_Init */
#include "buzzer_irq.h" /* BuzzerIRQ_Init */
#include "relay_app.h" /* relay_init */
#include "rgb_app.h"   /* rgb_init */
#include "dht11_app.h"   /* dht11_init */
#include "ds18b20_app.h" /* ds18b20_init */
#include "oled_app.h"    /* oled_init */
#include "bh1750.h"      /* BH1750_Init */

int main(void)
{
    /* ---- 各模块初始化 ---- */
    delay_init();     /* SysTick 延时，必须最先 */
    IIC_Init();       /* PB6/PB7 软件 I2C 总线 */
    SMG_Init();       /* 数码管 + TIM4 扫描 */
    led_init();       /* 板载 D0 / D1 */
    photo_init();     /* 光敏 PA5 + ADC1 */
    TICK_Init();      /* TIM6 1ms 时基（先不启动） */
    KeyIRQ_Init();    /* PE0~PE3 外部中断 */
    MatrixIRQ_Init(); /* PF0~PF7 矩阵扫描 */
    BuzzerIRQ_Init(); /* PB10 蜂鸣器 */
    relay_init();     /* PC0 / PC1 */
    rgb_init();       /* PC6~PC8 PWM */
#if TEMP_SENSOR_DHT11
    dht11_init();     /* PB11 DHT11 */
#else
    ds18b20_init();   /* PB11 DS18B20 */
#endif
    oled_init();      /* PB6/PB7 OLED，内部自动选 0x78 / 0x7A */
#if USE_BH1750
    (void)BH1750_Init(); /* PB6/PB7 照度传感器（没接也没关系，返回 0） */
#endif

    TICK_Start();     /* 1ms 时基最后启动 */

    while (1)
    {
        /* 在这里写你的功能 */
    }
}
